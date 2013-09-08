#include "Dalvik.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
static bool isPowerOfTwo(int x) { return (x & (x - 1)) == 0; }
#endif

#define kMaxAllocRecordStackDepth   16      /* max 255 */

#define kDefaultNumAllocRecords 64*1024 /* MUST be power of 2 */

struct AllocRecord {
    ClassObject*    clazz;
    u4              size;
    u2              threadId;

    struct {
        const Method* method;
        int         pc;
    } stackElem[kMaxAllocRecordStackDepth];
};

bool dvmAllocTrackerStartup()
{
    dvmInitMutex(&gDvm.allocTrackerLock);

    assert(gDvm.allocRecords == NULL);

    return true;
}

void dvmAllocTrackerShutdown()
{
    free(gDvm.allocRecords);
    dvmDestroyMutex(&gDvm.allocTrackerLock);
}



static int getAllocRecordMax() {
#ifdef HAVE_ANDROID_OS
    const char* propertyName = "dalvik.vm.allocTrackerMax";
    char allocRecordMaxString[PROPERTY_VALUE_MAX];
    if (property_get(propertyName, allocRecordMaxString, "") > 0) {
        char* end;
        size_t value = strtoul(allocRecordMaxString, &end, 10);
        if (*end != '\0') {
            ALOGE("Ignoring %s '%s' --- invalid", propertyName, allocRecordMaxString);
            return kDefaultNumAllocRecords;
        }
        if (!isPowerOfTwo(value)) {
            ALOGE("Ignoring %s '%s' --- not power of two", propertyName, allocRecordMaxString);
            return kDefaultNumAllocRecords;
        }
        return value;
    }
#endif
    return kDefaultNumAllocRecords;
}

bool dvmEnableAllocTracker()
{
    bool result = true;
    dvmLockMutex(&gDvm.allocTrackerLock);

    if (gDvm.allocRecords == NULL) {
        gDvm.allocRecordMax = getAllocRecordMax();

        ALOGI("Enabling alloc tracker (%d entries, %d frames --> %d bytes)",
              gDvm.allocRecordMax, kMaxAllocRecordStackDepth,
              sizeof(AllocRecord) * gDvm.allocRecordMax);
        gDvm.allocRecordHead = gDvm.allocRecordCount = 0;
        gDvm.allocRecords = (AllocRecord*) malloc(sizeof(AllocRecord) * gDvm.allocRecordMax);

        if (gDvm.allocRecords == NULL)
            result = false;
    }

    dvmUnlockMutex(&gDvm.allocTrackerLock);
    return result;
}

void dvmDisableAllocTracker()
{
    dvmLockMutex(&gDvm.allocTrackerLock);

    if (gDvm.allocRecords != NULL) {
        free(gDvm.allocRecords);
        gDvm.allocRecords = NULL;
    }

    dvmUnlockMutex(&gDvm.allocTrackerLock);
}

static void getStackFrames(Thread* self, AllocRecord* pRec)
{
    int stackDepth = 0;
    void* fp;

    fp = self->interpSave.curFrame;

    while ((fp != NULL) && (stackDepth < kMaxAllocRecordStackDepth)) {
        const StackSaveArea* saveArea = SAVEAREA_FROM_FP(fp);
        const Method* method = saveArea->method;

        if (!dvmIsBreakFrame((u4*) fp)) {
            pRec->stackElem[stackDepth].method = method;
            if (dvmIsNativeMethod(method)) {
                pRec->stackElem[stackDepth].pc = 0;
            } else {
                assert(saveArea->xtra.currentPc >= method->insns &&
                        saveArea->xtra.currentPc <
                        method->insns + dvmGetMethodInsnsSize(method));
                pRec->stackElem[stackDepth].pc =
                    (int) (saveArea->xtra.currentPc - method->insns);
            }
            stackDepth++;
        }

        assert(fp != saveArea->prevFrame);
        fp = saveArea->prevFrame;
    }

    /* clear out the rest (normally there won't be any) */
    while (stackDepth < kMaxAllocRecordStackDepth) {
        pRec->stackElem[stackDepth].method = NULL;
        pRec->stackElem[stackDepth].pc = 0;
        stackDepth++;
    }
}

void dvmDoTrackAllocation(ClassObject* clazz, size_t size)
{
    Thread* self = dvmThreadSelf();
    if (self == NULL) {
        ALOGW("alloc tracker: no thread");
        return;
    }

    dvmLockMutex(&gDvm.allocTrackerLock);
    if (gDvm.allocRecords == NULL) {
        dvmUnlockMutex(&gDvm.allocTrackerLock);
        return;
    }

    if (++gDvm.allocRecordHead == gDvm.allocRecordMax)
        gDvm.allocRecordHead = 0;

    AllocRecord* pRec = &gDvm.allocRecords[gDvm.allocRecordHead];

    pRec->clazz = clazz;
    pRec->size = size;
    pRec->threadId = self->threadId;
    getStackFrames(self, pRec);

    if (gDvm.allocRecordCount < gDvm.allocRecordMax)
        gDvm.allocRecordCount++;

    dvmUnlockMutex(&gDvm.allocTrackerLock);
}


The data we send to DDMS contains everything we have recorded.

Message header (all values big-endian):
  (1b) message header len (to allow future expansion); includes itself
  (1b) entry header len
  (1b) stack frame len
  (2b) number of entries
  (4b) offset to string table from start of message
  (2b) number of class name strings
  (2b) number of method name strings
  (2b) number of source file name strings
  For each entry:
    (4b) total allocation size
    (2b) threadId
    (2b) allocated object's class name index
    (1b) stack depth
    For each stack frame:
      (2b) method's class name
      (2b) method name
      (2b) method source file
      (2b) line number, clipped to 32767; -2 if native; -1 if no source
  (xb) class name strings
  (xb) method name strings
  (xb) source file strings

  As with other DDM traffic, strings are sent as a 4-byte length
  followed by UTF-16 data.

We send up 16-bit unsigned indexes into string tables.  In theory there
can be (kMaxAllocRecordStackDepth * gDvm.allocRecordMax) unique strings in
each table, but in practice there should be far fewer.

The chief reason for using a string table here is to keep the size of
the DDMS message to a minimum.  This is partly to make the protocol
efficient, but also because we have to form the whole thing up all at
once in a memory buffer.

We use separate string tables for class names, method names, and source
files to keep the indexes small.  There will generally be no overlap
between the contents of these tables.

const int kMessageHeaderLen = 15;
const int kEntryHeaderLen = 9;
const int kStackFrameLen = 8;

inline static int headIndex()
{
    return (gDvm.allocRecordHead+1 + gDvm.allocRecordMax - gDvm.allocRecordCount)
            & (gDvm.allocRecordMax-1);
}

static void dumpStringTable(PointerSet* strings)
{
    int count = dvmPointerSetGetCount(strings);
    int i;

    for (i = 0; i < count; i++)
        printf("  %s\n", (const char*) dvmPointerSetGetEntry(strings, i));
}

static const char* getMethodSourceFile(const Method* method)
{
    const char* fileName = dvmGetMethodSourceFile(method);
    if (fileName == NULL)
        fileName = "";
    return fileName;
}

static bool populateStringTables(PointerSet* classNames,
    PointerSet* methodNames, PointerSet* fileNames)
{
    int count = gDvm.allocRecordCount;
    int idx = headIndex();
    int classCount, methodCount, fileCount;

    classCount = methodCount = fileCount = 0;

    while (count--) {
        AllocRecord* pRec = &gDvm.allocRecords[idx];

        dvmPointerSetAddEntry(classNames, pRec->clazz->descriptor);
        classCount++;

        int i;
        for (i = 0; i < kMaxAllocRecordStackDepth; i++) {
            if (pRec->stackElem[i].method == NULL)
                break;

            const Method* method = pRec->stackElem[i].method;
            dvmPointerSetAddEntry(classNames, method->clazz->descriptor);
            classCount++;
            dvmPointerSetAddEntry(methodNames, method->name);
            methodCount++;
            dvmPointerSetAddEntry(fileNames, getMethodSourceFile(method));
            fileCount++;
        }

        idx = (idx + 1) & (gDvm.allocRecordMax-1);
    }

    ALOGI("class %d/%d, method %d/%d, file %d/%d",
        dvmPointerSetGetCount(classNames), classCount,
        dvmPointerSetGetCount(methodNames), methodCount,
        dvmPointerSetGetCount(fileNames), fileCount);

    return true;
}

static size_t generateBaseOutput(u1* ptr, size_t baseLen,
    const PointerSet* classNames, const PointerSet* methodNames,
    const PointerSet* fileNames)
{
    u1* origPtr = ptr;
    int count = gDvm.allocRecordCount;
    int idx = headIndex();

    if (origPtr != NULL) {
        set1(&ptr[0], kMessageHeaderLen);
        set1(&ptr[1], kEntryHeaderLen);
        set1(&ptr[2], kStackFrameLen);
        set2BE(&ptr[3], count);
        set4BE(&ptr[5], baseLen);
        set2BE(&ptr[9], dvmPointerSetGetCount(classNames));
        set2BE(&ptr[11], dvmPointerSetGetCount(methodNames));
        set2BE(&ptr[13], dvmPointerSetGetCount(fileNames));
    }
    ptr += kMessageHeaderLen;

    while (count--) {
        AllocRecord* pRec = &gDvm.allocRecords[idx];

        /* compute depth */
        int  depth;
        for (depth = 0; depth < kMaxAllocRecordStackDepth; depth++) {
            if (pRec->stackElem[depth].method == NULL)
                break;
        }

        /* output header */
        if (origPtr != NULL) {
            set4BE(&ptr[0], pRec->size);
            set2BE(&ptr[4], pRec->threadId);
            set2BE(&ptr[6],
                dvmPointerSetFind(classNames, pRec->clazz->descriptor));
            set1(&ptr[8], depth);
        }
        ptr += kEntryHeaderLen;

        /* convert stack frames */
        int i;
        for (i = 0; i < depth; i++) {
            if (origPtr != NULL) {
                const Method* method = pRec->stackElem[i].method;
                int lineNum;

                lineNum = dvmLineNumFromPC(method, pRec->stackElem[i].pc);
                if (lineNum > 32767)
                    lineNum = 32767;

                set2BE(&ptr[0], dvmPointerSetFind(classNames,
                        method->clazz->descriptor));
                set2BE(&ptr[2], dvmPointerSetFind(methodNames,
                        method->name));
                set2BE(&ptr[4], dvmPointerSetFind(fileNames,
                        getMethodSourceFile(method)));
                set2BE(&ptr[6], (u2)lineNum);
            }
            ptr += kStackFrameLen;
        }

        idx = (idx + 1) & (gDvm.allocRecordMax-1);
    }

    return ptr - origPtr;
}

static size_t computeStringTableSize(PointerSet* strings)
{
    int count = dvmPointerSetGetCount(strings);
    size_t size = 0;
    int i;

    for (i = 0; i < count; i++) {
        const char* str = (const char*) dvmPointerSetGetEntry(strings, i);

        size += 4 + dvmUtf8Len(str) * 2;
    }

    return size;
}

static int convertUtf8ToUtf16BEUA(u1* utf16Str, const char* utf8Str)
{
    u1* origUtf16Str = utf16Str;

    while (*utf8Str != '\0') {
        u2 utf16 = dexGetUtf16FromUtf8(&utf8Str);
        set2BE(utf16Str, utf16);
        utf16Str += 2;
    }

    return (utf16Str - origUtf16Str) / 2;
}

static size_t outputStringTable(PointerSet* strings, u1* ptr)
{
    int count = dvmPointerSetGetCount(strings);
    u1* origPtr = ptr;
    int i;

    for (i = 0; i < count; i++) {
        const char* str = (const char*) dvmPointerSetGetEntry(strings, i);
        int charLen;

        /* copy UTF-8 string to big-endian unaligned UTF-16 */
        charLen = convertUtf8ToUtf16BEUA(&ptr[4], str);
        set4BE(&ptr[0], charLen);

        ptr += 4 + charLen * 2;
    }

    return ptr - origPtr;
}

bool dvmGenerateTrackedAllocationReport(u1** pData, size_t* pDataLen)
{
    bool result = false;
    u1* buffer = NULL;

    dvmLockMutex(&gDvm.allocTrackerLock);

    PointerSet* classNames = NULL;
    PointerSet* methodNames = NULL;
    PointerSet* fileNames = NULL;

    classNames = dvmPointerSetAlloc(128);
    methodNames = dvmPointerSetAlloc(128);
    fileNames = dvmPointerSetAlloc(128);
    if (classNames == NULL || methodNames == NULL || fileNames == NULL) {
        ALOGE("Failed allocating pointer sets");
        goto bail;
    }

    if (!populateStringTables(classNames, methodNames, fileNames))
        goto bail;

    if (false) {
        printf("Classes:\n");
        dumpStringTable(classNames);
        printf("Methods:\n");
        dumpStringTable(methodNames);
        printf("Files:\n");
        dumpStringTable(fileNames);
    }

    size_t baseSize, totalSize;
    baseSize = generateBaseOutput(NULL, 0, classNames, methodNames, fileNames);
    assert(baseSize > 0);
    totalSize = baseSize;
    totalSize += computeStringTableSize(classNames);
    totalSize += computeStringTableSize(methodNames);
    totalSize += computeStringTableSize(fileNames);
    ALOGI("Generated AT, size is %zd/%zd", baseSize, totalSize);

    u1* strPtr;

    buffer = (u1*) malloc(totalSize);
    strPtr = buffer + baseSize;
    generateBaseOutput(buffer, baseSize, classNames, methodNames, fileNames);
    strPtr += outputStringTable(classNames, strPtr);
    strPtr += outputStringTable(methodNames, strPtr);
    strPtr += outputStringTable(fileNames, strPtr);
    if (strPtr - buffer != (int)totalSize) {
        ALOGE("size mismatch (%d vs %zd)", strPtr - buffer, totalSize);
        dvmAbort();
    }

    *pData = buffer;
    *pDataLen = totalSize;
    buffer = NULL;
    result = true;

bail:
    dvmPointerSetFree(classNames);
    dvmPointerSetFree(methodNames);
    dvmPointerSetFree(fileNames);
    free(buffer);
    dvmUnlockMutex(&gDvm.allocTrackerLock);
    //dvmDumpTrackedAllocations(false);
    return result;
}

void dvmDumpTrackedAllocations(bool enable)
{
    if (enable)
        dvmEnableAllocTracker();

    dvmLockMutex(&gDvm.allocTrackerLock);
    if (gDvm.allocRecords == NULL) {
        dvmUnlockMutex(&gDvm.allocTrackerLock);
        return;
    }

    int idx = headIndex();
    int count = gDvm.allocRecordCount;

    ALOGI("Tracked allocations, (head=%d count=%d)",
        gDvm.allocRecordHead, count);
    while (count--) {
        AllocRecord* pRec = &gDvm.allocRecords[idx];
        ALOGI(" T=%-2d %6d %s",
            pRec->threadId, pRec->size, pRec->clazz->descriptor);

        if (true) {
            for (int i = 0; i < kMaxAllocRecordStackDepth; i++) {
                if (pRec->stackElem[i].method == NULL)
                    break;

                const Method* method = pRec->stackElem[i].method;
                if (dvmIsNativeMethod(method)) {
                    ALOGI("    %s.%s (Native)",
                        method->clazz->descriptor, method->name);
                } else {
                    ALOGI("    %s.%s +%d",
                        method->clazz->descriptor, method->name,
                        pRec->stackElem[i].pc);
                }
            }
        }

        /* pause periodically to help logcat catch up */
        if ((count % 5) == 0)
            usleep(40000);

        idx = (idx + 1) & (gDvm.allocRecordMax-1);
    }

    dvmUnlockMutex(&gDvm.allocTrackerLock);
    if (false) {
        u1* data;
        size_t dataLen;
        if (dvmGenerateTrackedAllocationReport(&data, &dataLen))
            free(data);
    }
}
