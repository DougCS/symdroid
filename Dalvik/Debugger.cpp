#define THREAD_GROUP_ALL ((ObjectId) 0x12345)   // magic, internal-only value

#define kSlot0Sub   1000    // Eclipse workaround

bool dvmDebuggerStartup()
{
    if (!dvmBreakpointStartup())
        return false;

    gDvm.dbgRegistry = dvmHashTableCreate(1000, NULL);
    return (gDvm.dbgRegistry != NULL);
}

void dvmDebuggerShutdown()
{
    dvmHashTableFree(gDvm.dbgRegistry);
    gDvm.dbgRegistry = NULL;
    dvmBreakpointShutdown();
}


void dvmDbgInitMutex(pthread_mutex_t* pMutex)
{
    dvmInitMutex(pMutex);
}
void dvmDbgLockMutex(pthread_mutex_t* pMutex)
{
    dvmLockMutex(pMutex);
}
void dvmDbgUnlockMutex(pthread_mutex_t* pMutex)
{
    dvmUnlockMutex(pMutex);
}
void dvmDbgInitCond(pthread_cond_t* pCond)
{
    pthread_cond_init(pCond, NULL);
}
void dvmDbgCondWait(pthread_cond_t* pCond, pthread_mutex_t* pMutex)
{
    int cc __attribute__ ((__unused__)) = pthread_cond_wait(pCond, pMutex);
    assert(cc == 0);
}
void dvmDbgCondSignal(pthread_cond_t* pCond)
{
    int cc __attribute__ ((__unused__)) = pthread_cond_signal(pCond);
    assert(cc == 0);
}
void dvmDbgCondBroadcast(pthread_cond_t* pCond)
{
    int cc __attribute__ ((__unused__)) = pthread_cond_broadcast(pCond);
    assert(cc == 0);
}


/* keep track of type, in case we need to distinguish them someday */
enum RegistryType {
    kObjectId = 0xc1, kRefTypeId
};

static inline u4 registryHash(u4 val)
{
    return val >> 4;
}

static int registryCompare(const void* obj1, const void* obj2)
{
    return (int) obj1 - (int) obj2;
}


#ifndef NDEBUG
static bool lookupId(ObjectId id)
{
    void* found;

    found = dvmHashTableLookup(gDvm.dbgRegistry, registryHash((u4) id),
                (void*)(u4) id, registryCompare, false);
    if (found == NULL)
        return false;
    assert(found == (void*)(u4) id);
    return true;
}
#endif

static ObjectId registerObject(const Object* obj, RegistryType type, bool reg)
{
    ObjectId id;

    if (obj == NULL)
        return 0;

    assert((u4) obj != 0xcccccccc);
    assert((u4) obj > 0x100);

    id = (ObjectId)(u4)obj | ((u8) type) << 32;
    if (!reg)
        return id;

    dvmHashTableLock(gDvm.dbgRegistry);
    if (!gDvm.debuggerConnected) {
        /* debugger has detached while we were doing stuff? */
        ALOGI("ignoring registerObject request in thread=%d",
            dvmThreadSelf()->threadId);
        //dvmAbort();
        goto bail;
    }

    dvmHashTableLookup(gDvm.dbgRegistry, registryHash((u4) id),
                (void*)(u4) id, registryCompare, true);

bail:
    dvmHashTableUnlock(gDvm.dbgRegistry);
    return id;
}

#ifndef NDEBUG
static bool objectIsRegistered(ObjectId id, RegistryType type)
{
    UNUSED_PARAMETER(type);

    if (id == 0)        // null reference?
        return true;

    dvmHashTableLock(gDvm.dbgRegistry);
    bool result = lookupId(id);
    dvmHashTableUnlock(gDvm.dbgRegistry);
    return result;
}
#endif

static RefTypeId classObjectToRefTypeId(ClassObject* clazz)
{
    return (RefTypeId) registerObject((Object*) clazz, kRefTypeId, true);
}
#if 0
static RefTypeId classObjectToRefTypeIdNoReg(ClassObject* clazz)
{
    return (RefTypeId) registerObject((Object*) clazz, kRefTypeId, false);
}
#endif
static ClassObject* refTypeIdToClassObject(RefTypeId id)
{
    assert(objectIsRegistered(id, kRefTypeId) || !gDvm.debuggerConnected);
    return (ClassObject*)(u4) id;
}

static ObjectId objectToObjectId(const Object* obj)
{
    return registerObject(obj, kObjectId, true);
}
static ObjectId objectToObjectIdNoReg(const Object* obj)
{
    return registerObject(obj, kObjectId, false);
}
static Object* objectIdToObject(ObjectId id)
{
    assert(objectIsRegistered(id, kObjectId) || !gDvm.debuggerConnected);
    return (Object*)(u4) id;
}

void dvmDbgRegisterObjectId(ObjectId id)
{
    Object* obj = (Object*)(u4) id;
    ALOGV("+++ registering %p (%s)", obj, obj->clazz->descriptor);
    registerObject(obj, kObjectId, true);
}

static MethodId methodToMethodId(const Method* meth)
{
    return (MethodId)(u4) meth;
}
static Method* methodIdToMethod(RefTypeId refTypeId, MethodId id)
{
    // TODO? verify "id" is actually a method in "refTypeId"
    return (Method*)(u4) id;
}

static FieldId fieldToFieldId(const Field* field)
{
    return (FieldId)(u4) field;
}
static Field* fieldIdToField(RefTypeId refTypeId, FieldId id)
{
    // TODO? verify "id" is actually a field in "refTypeId"
    return (Field*)(u4) id;
}

static FrameId frameToFrameId(const void* frame)
{
    return (FrameId)(u4) frame;
}
static u4* frameIdToFrame(FrameId id)
{
    return (u4*)(u4) id;
}


DebugInvokeReq* dvmDbgGetInvokeReq()
{
    return &dvmThreadSelf()->invokeReq;
}

void dvmDbgConnected()
{
    assert(!gDvm.debuggerConnected);

    ALOGV("JDWP has attached");
    assert(dvmHashTableNumEntries(gDvm.dbgRegistry) == 0);
    gDvm.debuggerConnected = true;
}

void dvmDbgActive()
{
    if (gDvm.debuggerActive)
        return;

    ALOGI("Debugger is active");
    dvmInitBreakpoints();
    gDvm.debuggerActive = true;
    dvmEnableAllSubMode(kSubModeDebuggerActive);
#if defined(WITH_JIT)
    dvmCompilerUpdateGlobalState();
#endif
}

void dvmDbgDisconnected()
{
    assert(gDvm.debuggerConnected);

    gDvm.debuggerActive = false;
    dvmDisableAllSubMode(kSubModeDebuggerActive);
#if defined(WITH_JIT)
    dvmCompilerUpdateGlobalState();
#endif

    dvmHashTableLock(gDvm.dbgRegistry);
    gDvm.debuggerConnected = false;

    ALOGD("Debugger has detached; object registry had %d entries",
        dvmHashTableNumEntries(gDvm.dbgRegistry));
    //int i;
    //for (i = 0; i < gDvm.dbgRegistryNext; i++)
    //    LOGVV("%4d: 0x%llx", i, gDvm.dbgRegistryTable[i]);

    dvmHashTableClear(gDvm.dbgRegistry);
    dvmHashTableUnlock(gDvm.dbgRegistry);
}

bool dvmDbgIsDebuggerConnected()
{
    return gDvm.debuggerActive;
}

s8 dvmDbgLastDebuggerActivity()
{
    return dvmJdwpLastDebuggerActivity(gDvm.jdwpState);
}

int dvmDbgThreadRunning()
{
    ThreadStatus oldStatus = dvmChangeStatus(NULL, THREAD_RUNNING);
    return static_cast<int>(oldStatus);
}

int dvmDbgThreadWaiting()
{
    ThreadStatus oldStatus = dvmChangeStatus(NULL, THREAD_VMWAIT);
    return static_cast<int>(oldStatus);
}

int dvmDbgThreadContinuing(int status)
{
    ThreadStatus newStatus = static_cast<ThreadStatus>(status);
    ThreadStatus oldStatus = dvmChangeStatus(NULL, newStatus);
    return static_cast<int>(oldStatus);
}

void dvmDbgExit(int status)
{
    // TODO? invoke System.exit() to perform exit processing; ends up
    // in System.exitInternal(), which can call JNI exit hook
    ALOGI("GC lifetime allocation: %d bytes", gDvm.allocProf.allocCount);
    if (CALC_CACHE_STATS) {
        dvmDumpAtomicCacheStats(gDvm.instanceofCache);
        dvmDumpBootClassPath();
    }
    exit(status);
}



const char* dvmDbgGetClassDescriptor(RefTypeId id)
{
    ClassObject* clazz;

    clazz = refTypeIdToClassObject(id);
    return clazz->descriptor;
}

ObjectId dvmDbgGetClassObject(RefTypeId id)
{
    ClassObject* clazz = refTypeIdToClassObject(id);
    return objectToObjectId((Object*) clazz);
}

RefTypeId dvmDbgGetSuperclass(RefTypeId id)
{
    ClassObject* clazz = refTypeIdToClassObject(id);
    return classObjectToRefTypeId(clazz->super);
}

RefTypeId dvmDbgGetClassLoader(RefTypeId id)
{
    ClassObject* clazz = refTypeIdToClassObject(id);
    return objectToObjectId(clazz->classLoader);
}

u4 dvmDbgGetAccessFlags(RefTypeId id)
{
    ClassObject* clazz = refTypeIdToClassObject(id);
    return clazz->accessFlags & JAVA_FLAGS_MASK;
}

bool dvmDbgIsInterface(RefTypeId id)
{
    ClassObject* clazz = refTypeIdToClassObject(id);
    return dvmIsInterfaceClass(clazz);
}

static int copyRefType(void* vclazz, void* varg)
{
    RefTypeId** pRefType = (RefTypeId**)varg;
    **pRefType = classObjectToRefTypeId((ClassObject*) vclazz);
    (*pRefType)++;
    return 0;
}

void dvmDbgGetClassList(u4* pNumClasses, RefTypeId** pClassRefBuf)
{
    RefTypeId* pRefType;

    dvmHashTableLock(gDvm.loadedClasses);
    *pNumClasses = dvmHashTableNumEntries(gDvm.loadedClasses);
    pRefType = *pClassRefBuf =
        (RefTypeId*)malloc(sizeof(RefTypeId) * *pNumClasses);

    if (dvmHashForeach(gDvm.loadedClasses, copyRefType, &pRefType) != 0) {
        ALOGW("Warning: problem getting class list");
        /* not really expecting this to happen */
    } else {
        assert(pRefType - *pClassRefBuf == (int) *pNumClasses);
    }

    dvmHashTableUnlock(gDvm.loadedClasses);
}

void dvmDbgGetVisibleClassList(ObjectId classLoaderId, u4* pNumClasses,
    RefTypeId** pClassRefBuf)
{
    Object* classLoader;
    int numClasses = 0, maxClasses;

    classLoader = objectIdToObject(classLoaderId);
    // I don't think classLoader can be NULL, but the spec doesn't say

    LOGVV("GetVisibleList: comparing to %p", classLoader);

    dvmHashTableLock(gDvm.loadedClasses);

    /* over-allocate the return buffer */
    maxClasses = dvmHashTableNumEntries(gDvm.loadedClasses);
    *pClassRefBuf = (RefTypeId*)malloc(sizeof(RefTypeId) * maxClasses);

    HashIter iter;
    for (dvmHashIterBegin(gDvm.loadedClasses, &iter); !dvmHashIterDone(&iter);
        dvmHashIterNext(&iter))
    {
        ClassObject* clazz = (ClassObject*) dvmHashIterData(&iter);

        if (clazz->classLoader == classLoader ||
            dvmLoaderInInitiatingList(clazz, classLoader))
        {
            LOGVV("  match '%s'", clazz->descriptor);
            (*pClassRefBuf)[numClasses++] = classObjectToRefTypeId(clazz);
        }
    }
    *pNumClasses = numClasses;

    dvmHashTableUnlock(gDvm.loadedClasses);
}

static const char* jniSignature(ClassObject* clazz)
{
    return clazz->descriptor;
}

void dvmDbgGetClassInfo(RefTypeId classId, u1* pTypeTag, u4* pStatus,
    const char** pSignature)
{
    ClassObject* clazz = refTypeIdToClassObject(classId);

    if (clazz->descriptor[0] == '[') {
        /* generated array class */
        *pStatus = CS_VERIFIED | CS_PREPARED;
        *pTypeTag = TT_ARRAY;
    } else {
        if (clazz->status == CLASS_ERROR)
            *pStatus = CS_ERROR;
        else
            *pStatus = CS_VERIFIED | CS_PREPARED | CS_INITIALIZED;
        if (dvmIsInterfaceClass(clazz))
            *pTypeTag = TT_INTERFACE;
        else
            *pTypeTag = TT_CLASS;
    }
    if (pSignature != NULL)
        *pSignature = jniSignature(clazz);
}

bool dvmDbgFindLoadedClassBySignature(const char* classDescriptor,
        RefTypeId* pRefTypeId)
{
    ClassObject* clazz;

    clazz = dvmFindLoadedClass(classDescriptor);
    if (clazz != NULL) {
        *pRefTypeId = classObjectToRefTypeId(clazz);
        return true;
    } else
        return false;
}


void dvmDbgGetObjectType(ObjectId objectId, u1* pRefTypeTag,
    RefTypeId* pRefTypeId)
{
    Object* obj = objectIdToObject(objectId);

    if (dvmIsArrayClass(obj->clazz))
        *pRefTypeTag = TT_ARRAY;
    else if (dvmIsInterfaceClass(obj->clazz))
        *pRefTypeTag = TT_INTERFACE;
    else
        *pRefTypeTag = TT_CLASS;
    *pRefTypeId = classObjectToRefTypeId(obj->clazz);
}

u1 dvmDbgGetClassObjectType(RefTypeId refTypeId)
{
    ClassObject* clazz = refTypeIdToClassObject(refTypeId);

    if (dvmIsArrayClass(clazz))
        return TT_ARRAY;
    else if (dvmIsInterfaceClass(clazz))
        return TT_INTERFACE;
    else
        return TT_CLASS;
}

const char* dvmDbgGetSignature(RefTypeId refTypeId)
{
    ClassObject* clazz;

    clazz = refTypeIdToClassObject(refTypeId);
    assert(clazz != NULL);

    return jniSignature(clazz);
}

const char* dvmDbgGetSourceFile(RefTypeId refTypeId)
{
    ClassObject* clazz;

    clazz = refTypeIdToClassObject(refTypeId);
    assert(clazz != NULL);

    return clazz->sourceFile;
}

const char* dvmDbgGetObjectTypeName(ObjectId objectId)
{
    if (objectId == 0)
        return "(null)";

    Object* obj = objectIdToObject(objectId);
    return jniSignature(obj->clazz);
}

static bool isTagPrimitive(u1 tag)
{
    switch (tag) {
    case JT_BYTE:
    case JT_CHAR:
    case JT_FLOAT:
    case JT_DOUBLE:
    case JT_INT:
    case JT_LONG:
    case JT_SHORT:
    case JT_VOID:
    case JT_BOOLEAN:
        return true;
    case JT_ARRAY:
    case JT_OBJECT:
    case JT_STRING:
    case JT_CLASS_OBJECT:
    case JT_THREAD:
    case JT_THREAD_GROUP:
    case JT_CLASS_LOADER:
        return false;
    default:
        ALOGE("ERROR: unhandled tag '%c'", tag);
        assert(false);
        return false;
    }
}

static u1 tagFromClass(ClassObject* clazz)
{
    if (dvmIsArrayClass(clazz))
        return JT_ARRAY;

    if (clazz == gDvm.classJavaLangString) {
        return JT_STRING;
    } else if (dvmIsTheClassClass(clazz)) {
        return JT_CLASS_OBJECT;
    } else if (dvmInstanceof(clazz, gDvm.classJavaLangThread)) {
        return JT_THREAD;
    } else if (dvmInstanceof(clazz, gDvm.classJavaLangThreadGroup)) {
        return JT_THREAD_GROUP;
    } else if (dvmInstanceof(clazz, gDvm.classJavaLangClassLoader)) {
        return JT_CLASS_LOADER;
    } else {
        return JT_OBJECT;
    }
}

static u1 basicTagFromDescriptor(const char* descriptor)
{
    return descriptor[0];
}

static u1 tagFromObject(const Object* obj)
{
    if (obj == NULL)
        return JT_OBJECT;
    return tagFromClass(obj->clazz);
}

u1 dvmDbgGetObjectTag(ObjectId objectId)
{
    return tagFromObject(objectIdToObject(objectId));
}

int dvmDbgGetTagWidth(int tag)
{
    switch (tag) {
    case JT_VOID:
        return 0;
    case JT_BYTE:
    case JT_BOOLEAN:
        return 1;
    case JT_CHAR:
    case JT_SHORT:
        return 2;
    case JT_FLOAT:
    case JT_INT:
        return 4;
    case JT_ARRAY:
    case JT_OBJECT:
    case JT_STRING:
    case JT_THREAD:
    case JT_THREAD_GROUP:
    case JT_CLASS_LOADER:
    case JT_CLASS_OBJECT:
        return sizeof(ObjectId);
    case JT_DOUBLE:
    case JT_LONG:
        return 8;
    default:
        ALOGE("ERROR: unhandled tag '%c'", tag);
        assert(false);
        return -1;
    }
}


int dvmDbgGetArrayLength(ObjectId arrayId)
{
    ArrayObject* arrayObj = (ArrayObject*) objectIdToObject(arrayId);
    assert(dvmIsArray(arrayObj));
    return arrayObj->length;
}

u1 dvmDbgGetArrayElementTag(ObjectId arrayId)
{
    ArrayObject* arrayObj = (ArrayObject*) objectIdToObject(arrayId);

    ClassObject* arrayClass = arrayObj->clazz;
    u1 tag = basicTagFromDescriptor(arrayClass->descriptor + 1);
    if (!isTagPrimitive(tag)) {
        /* try to refine it */
        tag = tagFromClass(arrayClass->elementClass);
    }

    return tag;
}

static void copyValuesToBE(u1* out, const u1* in, int count, int width)
{
    int i;

    switch (width) {
    case 1:
        memcpy(out, in, count);
        break;
    case 2:
        for (i = 0; i < count; i++)
            *(((u2*) out)+i) = get2BE(in + i*2);
        break;
    case 4:
        for (i = 0; i < count; i++)
            *(((u4*) out)+i) = get4BE(in + i*4);
        break;
    case 8:
        for (i = 0; i < count; i++)
            *(((u8*) out)+i) = get8BE(in + i*8);
        break;
    default:
        assert(false);
    }
}

static void copyValuesFromBE(u1* out, const u1* in, int count, int width)
{
    int i;

    switch (width) {
    case 1:
        memcpy(out, in, count);
        break;
    case 2:
        for (i = 0; i < count; i++)
            set2BE(out + i*2, *((u2*)in + i));
        break;
    case 4:
        for (i = 0; i < count; i++)
            set4BE(out + i*4, *((u4*)in + i));
        break;
    case 8:
        for (i = 0; i < count; i++)
            set8BE(out + i*8, *((u8*)in + i));
        break;
    default:
        assert(false);
    }
}

bool dvmDbgOutputArray(ObjectId arrayId, int firstIndex, int count,
    ExpandBuf* pReply)
{
    ArrayObject* arrayObj = (ArrayObject*) objectIdToObject(arrayId);
    const u1* data = (const u1*)arrayObj->contents;
    u1 tag;

    assert(dvmIsArray(arrayObj));

    if (firstIndex + count > (int)arrayObj->length) {
        ALOGW("Request for index=%d + count=%d excceds length=%d",
            firstIndex, count, arrayObj->length);
        return false;
    }

    tag = basicTagFromDescriptor(arrayObj->clazz->descriptor + 1);

    if (isTagPrimitive(tag)) {
        int width = dvmDbgGetTagWidth(tag);
        u1* outBuf;

        outBuf = expandBufAddSpace(pReply, count * width);

        copyValuesToBE(outBuf, data + firstIndex*width, count, width);
    } else {
        Object** pObjects;
        int i;

        pObjects = (Object**) data;
        pObjects += firstIndex;

        ALOGV("    --> copying %d object IDs", count);
        //assert(tag == JT_OBJECT);     // could be object or "refined" type

        for (i = 0; i < count; i++, pObjects++) {
            u1 thisTag;
            if (*pObjects != NULL)
                thisTag = tagFromObject(*pObjects);
            else
                thisTag = tag;
            expandBufAdd1(pReply, thisTag);
            expandBufAddObjectId(pReply, objectToObjectId(*pObjects));
        }
    }

    return true;
}

bool dvmDbgSetArrayElements(ObjectId arrayId, int firstIndex, int count,
    const u1* buf)
{
    ArrayObject* arrayObj = (ArrayObject*) objectIdToObject(arrayId);
    u1* data = (u1*)arrayObj->contents;
    u1 tag;

    assert(dvmIsArray(arrayObj));

    if (firstIndex + count > (int)arrayObj->length) {
        ALOGW("Attempt to set index=%d + count=%d excceds length=%d",
            firstIndex, count, arrayObj->length);
        return false;
    }

    tag = basicTagFromDescriptor(arrayObj->clazz->descriptor + 1);

    if (isTagPrimitive(tag)) {
        int width = dvmDbgGetTagWidth(tag);

        ALOGV("    --> setting %d '%c' width=%d", count, tag, width);

        copyValuesFromBE(data + firstIndex*width, buf, count, width);
    } else {
        Object** pObjects;
        int i;

        pObjects = (Object**) data;
        pObjects += firstIndex;

        ALOGV("    --> setting %d objects", count);

        /* should do array type check here */
        for (i = 0; i < count; i++) {
            ObjectId id = dvmReadObjectId(&buf);
            *pObjects++ = objectIdToObject(id);
        }
    }

    return true;
}

ObjectId dvmDbgCreateString(const char* str)
{
    StringObject* strObj;

    strObj = dvmCreateStringFromCstr(str);
    dvmReleaseTrackedAlloc((Object*) strObj, NULL);
    return objectToObjectId((Object*) strObj);
}

ObjectId dvmDbgCreateObject(RefTypeId classId)
{
    ClassObject* clazz = refTypeIdToClassObject(classId);
    Object* newObj = dvmAllocObject(clazz, ALLOC_DEFAULT);
    dvmReleaseTrackedAlloc(newObj, NULL);
    return objectToObjectId(newObj);
}

ObjectId dvmDbgCreateArrayObject(RefTypeId arrayTypeId, u4 length)
{
    ClassObject* clazz = refTypeIdToClassObject(arrayTypeId);
    Object* newObj = (Object*) dvmAllocArrayByClass(clazz, length, ALLOC_DEFAULT);
    dvmReleaseTrackedAlloc(newObj, NULL);
    return objectToObjectId(newObj);
}

bool dvmDbgMatchType(RefTypeId instClassId, RefTypeId classId)
{
    ClassObject* instClazz = refTypeIdToClassObject(instClassId);
    ClassObject* clazz = refTypeIdToClassObject(classId);

    return dvmInstanceof(instClazz, clazz);
}



const char* dvmDbgGetMethodName(RefTypeId refTypeId, MethodId id)
{
    Method* meth;

    meth = methodIdToMethod(refTypeId, id);
    return meth->name;
}

static u4 augmentedAccessFlags(u4 accessFlags)
{
    accessFlags &= JAVA_FLAGS_MASK;

    if ((accessFlags & ACC_SYNTHETIC) != 0) {
        return accessFlags | 0xf0000000;
    } else {
        return accessFlags;
    }
}

void dvmDbgOutputAllFields(RefTypeId refTypeId, bool withGeneric,
    ExpandBuf* pReply)
{
    ClassObject* clazz = refTypeIdToClassObject(refTypeId);
    assert(clazz != NULL);

    u4 declared = clazz->sfieldCount + clazz->ifieldCount;
    expandBufAdd4BE(pReply, declared);

    for (int i = 0; i < clazz->sfieldCount; i++) {
        Field* field = &clazz->sfields[i];
        expandBufAddFieldId(pReply, fieldToFieldId(field));
        expandBufAddUtf8String(pReply, (const u1*) field->name);
        expandBufAddUtf8String(pReply, (const u1*) field->signature);
        if (withGeneric) {
            static const u1 genericSignature[1] = "";
            expandBufAddUtf8String(pReply, genericSignature);
        }
        expandBufAdd4BE(pReply, augmentedAccessFlags(field->accessFlags));
    }
    for (int i = 0; i < clazz->ifieldCount; i++) {
        Field* field = &clazz->ifields[i];
        expandBufAddFieldId(pReply, fieldToFieldId(field));
        expandBufAddUtf8String(pReply, (const u1*) field->name);
        expandBufAddUtf8String(pReply, (const u1*) field->signature);
        if (withGeneric) {
            static const u1 genericSignature[1] = "";
            expandBufAddUtf8String(pReply, genericSignature);
        }
        expandBufAdd4BE(pReply, augmentedAccessFlags(field->accessFlags));
    }
}

void dvmDbgOutputAllMethods(RefTypeId refTypeId, bool withGeneric,
    ExpandBuf* pReply)
{
    DexStringCache stringCache;
    static const u1 genericSignature[1] = "";
    ClassObject* clazz;
    Method* meth;
    u4 declared;
    int i;

    dexStringCacheInit(&stringCache);

    clazz = refTypeIdToClassObject(refTypeId);
    assert(clazz != NULL);

    declared = clazz->directMethodCount + clazz->virtualMethodCount;
    expandBufAdd4BE(pReply, declared);

    for (i = 0; i < clazz->directMethodCount; i++) {
        meth = &clazz->directMethods[i];

        expandBufAddMethodId(pReply, methodToMethodId(meth));
        expandBufAddUtf8String(pReply, (const u1*) meth->name);

        expandBufAddUtf8String(pReply,
            (const u1*) dexProtoGetMethodDescriptor(&meth->prototype,
                    &stringCache));

        if (withGeneric)
            expandBufAddUtf8String(pReply, genericSignature);
        expandBufAdd4BE(pReply, augmentedAccessFlags(meth->accessFlags));
    }
    for (i = 0; i < clazz->virtualMethodCount; i++) {
        meth = &clazz->virtualMethods[i];

        expandBufAddMethodId(pReply, methodToMethodId(meth));
        expandBufAddUtf8String(pReply, (const u1*) meth->name);

        expandBufAddUtf8String(pReply,
            (const u1*) dexProtoGetMethodDescriptor(&meth->prototype,
                    &stringCache));

        if (withGeneric)
            expandBufAddUtf8String(pReply, genericSignature);
        expandBufAdd4BE(pReply, augmentedAccessFlags(meth->accessFlags));
    }

    dexStringCacheRelease(&stringCache);
}

void dvmDbgOutputAllInterfaces(RefTypeId refTypeId, ExpandBuf* pReply)
{
    ClassObject* clazz;
    int i, count;

    clazz = refTypeIdToClassObject(refTypeId);
    assert(clazz != NULL);

    count = clazz->interfaceCount;
    expandBufAdd4BE(pReply, count);
    for (i = 0; i < count; i++) {
        ClassObject* iface = clazz->interfaces[i];
        expandBufAddRefTypeId(pReply, classObjectToRefTypeId(iface));
    }
}

struct DebugCallbackContext {
    int numItems;
    ExpandBuf* pReply;
    // used by locals table
    bool withGeneric;
};

static int lineTablePositionsCb(void *cnxt, u4 address, u4 lineNum)
{
    DebugCallbackContext *pContext = (DebugCallbackContext *)cnxt;

    expandBufAdd8BE(pContext->pReply, address);
    expandBufAdd4BE(pContext->pReply, lineNum);
    pContext->numItems++;

    return 0;
}

void dvmDbgOutputLineTable(RefTypeId refTypeId, MethodId methodId,
    ExpandBuf* pReply)
{
    Method* method;
    u8 start, end;
    DebugCallbackContext context;

    memset (&context, 0, sizeof(DebugCallbackContext));

    method = methodIdToMethod(refTypeId, methodId);
    if (dvmIsNativeMethod(method)) {
        start = (u8) -1;
        end = (u8) -1;
    } else {
        start = 0;
        end = dvmGetMethodInsnsSize(method);
    }

    expandBufAdd8BE(pReply, start);
    expandBufAdd8BE(pReply, end);

    // Add numLines later
    size_t numLinesOffset = expandBufGetLength(pReply);
    expandBufAdd4BE(pReply, 0);

    context.pReply = pReply;

    dexDecodeDebugInfo(method->clazz->pDvmDex->pDexFile,
        dvmGetMethodCode(method),
        method->clazz->descriptor,
        method->prototype.protoIdx,
        method->accessFlags,
        lineTablePositionsCb, NULL, &context);

    set4BE(expandBufGetBuffer(pReply) + numLinesOffset, context.numItems);
}

static int tweakSlot(int slot, const char* name)
{
    int newSlot = slot;

    if (strcmp(name, "this") == 0)      // only remap "this" ptr
        newSlot = 0;
    else if (slot == 0)                 // always remap slot 0
        newSlot = kSlot0Sub;

    ALOGV("untweak: %d to %d", slot, newSlot);
    return newSlot;
}

static int untweakSlot(int slot, const void* framePtr)
{
    int newSlot = slot;

    if (slot == kSlot0Sub) {
        newSlot = 0;
    } else if (slot == 0) {
        const StackSaveArea* saveArea = SAVEAREA_FROM_FP(framePtr);
        const Method* method = saveArea->method;
        newSlot = method->registersSize - method->insSize;
    }

    ALOGV("untweak: %d to %d", slot, newSlot);
    return newSlot;
}

static void variableTableCb (void *cnxt, u2 reg, u4 startAddress,
        u4 endAddress, const char *name, const char *descriptor,
        const char *signature)
{
    DebugCallbackContext *pContext = (DebugCallbackContext *)cnxt;

    reg = (u2) tweakSlot(reg, name);

    ALOGV("    %2d: %d(%d) '%s' '%s' slot=%d",
        pContext->numItems, startAddress, endAddress - startAddress,
        name, descriptor, reg);

    expandBufAdd8BE(pContext->pReply, startAddress);
    expandBufAddUtf8String(pContext->pReply, (const u1*)name);
    expandBufAddUtf8String(pContext->pReply, (const u1*)descriptor);
    if (pContext->withGeneric) {
        expandBufAddUtf8String(pContext->pReply, (const u1*) signature);
    }
    expandBufAdd4BE(pContext->pReply, endAddress - startAddress);
    expandBufAdd4BE(pContext->pReply, reg);

    pContext->numItems++;
}

void dvmDbgOutputVariableTable(RefTypeId refTypeId, MethodId methodId,
    bool withGeneric, ExpandBuf* pReply)
{
    Method* method;
    DebugCallbackContext context;

    memset (&context, 0, sizeof(DebugCallbackContext));

    method = methodIdToMethod(refTypeId, methodId);

    expandBufAdd4BE(pReply, method->insSize);

    // Add numLocals later
    size_t numLocalsOffset = expandBufGetLength(pReply);
    expandBufAdd4BE(pReply, 0);

    context.pReply = pReply;
    context.withGeneric = withGeneric;
    dexDecodeDebugInfo(method->clazz->pDvmDex->pDexFile,
        dvmGetMethodCode(method),
        method->clazz->descriptor,
        method->prototype.protoIdx,
        method->accessFlags,
        NULL, variableTableCb, &context);

    set4BE(expandBufGetBuffer(pReply) + numLocalsOffset, context.numItems);
}

u1 dvmDbgGetFieldBasicTag(ObjectId objId, FieldId fieldId)
{
    Object* obj = objectIdToObject(objId);
    RefTypeId classId = classObjectToRefTypeId(obj->clazz);
    const Field* field = fieldIdToField(classId, fieldId);
    return basicTagFromDescriptor(field->signature);
}

u1 dvmDbgGetStaticFieldBasicTag(RefTypeId refTypeId, FieldId fieldId)
{
    const Field* field = fieldIdToField(refTypeId, fieldId);
    return basicTagFromDescriptor(field->signature);
}


void dvmDbgGetFieldValue(ObjectId objectId, FieldId fieldId, ExpandBuf* pReply)
{
    Object* obj = objectIdToObject(objectId);
    RefTypeId classId = classObjectToRefTypeId(obj->clazz);
    InstField* ifield = (InstField*) fieldIdToField(classId, fieldId);
    u1 tag = basicTagFromDescriptor(ifield->signature);

    if (tag == JT_ARRAY || tag == JT_OBJECT) {
        Object* objVal = dvmGetFieldObject(obj, ifield->byteOffset);
        tag = tagFromObject(objVal);
        expandBufAdd1(pReply, tag);
        expandBufAddObjectId(pReply, objectToObjectId(objVal));
        ALOGV("    --> ifieldId %x --> tag '%c' %p", fieldId, tag, objVal);
    } else {
        ALOGV("    --> ifieldId %x --> tag '%c'", fieldId, tag);
        expandBufAdd1(pReply, tag);

        switch (tag) {
        case JT_BOOLEAN:
            expandBufAdd1(pReply, dvmGetFieldBoolean(obj, ifield->byteOffset));
            break;
        case JT_BYTE:
            expandBufAdd1(pReply, dvmGetFieldByte(obj, ifield->byteOffset));
            break;
        case JT_SHORT:
            expandBufAdd2BE(pReply, dvmGetFieldShort(obj, ifield->byteOffset));
            break;
        case JT_CHAR:
            expandBufAdd2BE(pReply, dvmGetFieldChar(obj, ifield->byteOffset));
            break;
        case JT_INT:
        case JT_FLOAT:
            expandBufAdd4BE(pReply, dvmGetFieldInt(obj, ifield->byteOffset));
            break;
        case JT_LONG:
        case JT_DOUBLE:
            expandBufAdd8BE(pReply, dvmGetFieldLong(obj, ifield->byteOffset));
            break;
        default:
            ALOGE("ERROR: unhandled field type '%s'", ifield->signature);
            assert(false);
            break;
        }
    }
}

void dvmDbgSetFieldValue(ObjectId objectId, FieldId fieldId, u8 value,
    int width)
{
    Object* obj = objectIdToObject(objectId);
    RefTypeId classId = classObjectToRefTypeId(obj->clazz);
    InstField* field = (InstField*) fieldIdToField(classId, fieldId);

    switch (field->signature[0]) {
    case JT_BOOLEAN:
        assert(width == 1);
        dvmSetFieldBoolean(obj, field->byteOffset, value != 0);
        break;
    case JT_BYTE:
        assert(width == 1);
        dvmSetFieldInt(obj, field->byteOffset, value);
        break;
    case JT_SHORT:
    case JT_CHAR:
        assert(width == 2);
        dvmSetFieldInt(obj, field->byteOffset, value);
        break;
    case JT_INT:
    case JT_FLOAT:
        assert(width == 4);
        dvmSetFieldInt(obj, field->byteOffset, value);
        break;
    case JT_ARRAY:
    case JT_OBJECT:
        assert(width == sizeof(ObjectId));
        dvmSetFieldObject(obj, field->byteOffset, objectIdToObject(value));
        break;
    case JT_DOUBLE:
    case JT_LONG:
        assert(width == 8);
        dvmSetFieldLong(obj, field->byteOffset, value);
        break;
    default:
        ALOGE("ERROR: unhandled class type '%s'", field->signature);
        assert(false);
        break;
    }
}

void dvmDbgGetStaticFieldValue(RefTypeId refTypeId, FieldId fieldId,
    ExpandBuf* pReply)
{
    StaticField* sfield = (StaticField*) fieldIdToField(refTypeId, fieldId);
    u1 tag = basicTagFromDescriptor(sfield->signature);

    if (tag == JT_ARRAY || tag == JT_OBJECT) {
        Object* objVal = dvmGetStaticFieldObject(sfield);
        tag = tagFromObject(objVal);
        expandBufAdd1(pReply, tag);
        expandBufAddObjectId(pReply, objectToObjectId(objVal));
        ALOGV("    --> sfieldId %x --> tag '%c' %p", fieldId, tag, objVal);
    } else {
        JValue value;

        ALOGV("    --> sfieldId %x --> tag '%c'", fieldId, tag);
        expandBufAdd1(pReply, tag);

        switch (tag) {
        case JT_BOOLEAN:
            expandBufAdd1(pReply, dvmGetStaticFieldBoolean(sfield));
            break;
        case JT_BYTE:
            expandBufAdd1(pReply, dvmGetStaticFieldByte(sfield));
            break;
        case JT_SHORT:
            expandBufAdd2BE(pReply, dvmGetStaticFieldShort(sfield));
            break;
        case JT_CHAR:
            expandBufAdd2BE(pReply, dvmGetStaticFieldChar(sfield));
            break;
        case JT_INT:
            expandBufAdd4BE(pReply, dvmGetStaticFieldInt(sfield));
            break;
        case JT_FLOAT:
            value.f = dvmGetStaticFieldFloat(sfield);
            expandBufAdd4BE(pReply, value.i);
            break;
        case JT_LONG:
            expandBufAdd8BE(pReply, dvmGetStaticFieldLong(sfield));
            break;
        case JT_DOUBLE:
            value.d = dvmGetStaticFieldDouble(sfield);
            expandBufAdd8BE(pReply, value.j);
            break;
        default:
            ALOGE("ERROR: unhandled field type '%s'", sfield->signature);
            assert(false);
            break;
        }
    }
}

void dvmDbgSetStaticFieldValue(RefTypeId refTypeId, FieldId fieldId,
    u8 rawValue, int width)
{
    StaticField* sfield = (StaticField*) fieldIdToField(refTypeId, fieldId);
    Object* objVal;
    JValue value;

    value.j = rawValue;

    switch (sfield->signature[0]) {
    case JT_BOOLEAN:
        assert(width == 1);
        dvmSetStaticFieldBoolean(sfield, value.z);
        break;
    case JT_BYTE:
        assert(width == 1);
        dvmSetStaticFieldByte(sfield, value.b);
        break;
    case JT_SHORT:
        assert(width == 2);
        dvmSetStaticFieldShort(sfield, value.s);
        break;
    case JT_CHAR:
        assert(width == 2);
        dvmSetStaticFieldChar(sfield, value.c);
        break;
    case JT_INT:
        assert(width == 4);
        dvmSetStaticFieldInt(sfield, value.i);
        break;
    case JT_FLOAT:
        assert(width == 4);
        dvmSetStaticFieldFloat(sfield, value.f);
        break;
    case JT_ARRAY:
    case JT_OBJECT:
        assert(width == sizeof(ObjectId));
        objVal = objectIdToObject(rawValue);
        dvmSetStaticFieldObject(sfield, objVal);
        break;
    case JT_LONG:
        assert(width == 8);
        dvmSetStaticFieldLong(sfield, value.j);
        break;
    case JT_DOUBLE:
        assert(width == 8);
        dvmSetStaticFieldDouble(sfield, value.d);
        break;
    default:
        ALOGE("ERROR: unhandled class type '%s'", sfield->signature);
        assert(false);
        break;
    }
}

char* dvmDbgStringToUtf8(ObjectId strId)
{
    StringObject* strObj = (StringObject*) objectIdToObject(strId);

    return dvmCreateCstrFromString(strObj);
}



static Thread* threadObjToThread(Object* threadObj)
{
    Thread* thread;

    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->threadObj == threadObj)
            break;
    }

    return thread;
}

bool dvmDbgGetThreadStatus(ObjectId threadId, u4* pThreadStatus,
    u4* pSuspendStatus)
{
    Object* threadObj;
    Thread* thread;
    bool result = false;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    /* lock the thread list, so the thread doesn't vanish while we work */
    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL)
        goto bail;

    switch (thread->status) {
    case THREAD_ZOMBIE:         *pThreadStatus = TS_ZOMBIE;     break;
    case THREAD_RUNNING:        *pThreadStatus = TS_RUNNING;    break;
    case THREAD_TIMED_WAIT:     *pThreadStatus = TS_SLEEPING;   break;
    case THREAD_MONITOR:        *pThreadStatus = TS_MONITOR;    break;
    case THREAD_WAIT:           *pThreadStatus = TS_WAIT;       break;
    case THREAD_INITIALIZING:   *pThreadStatus = TS_ZOMBIE;     break;
    case THREAD_STARTING:       *pThreadStatus = TS_ZOMBIE;     break;
    case THREAD_NATIVE:         *pThreadStatus = TS_RUNNING;    break;
    case THREAD_VMWAIT:         *pThreadStatus = TS_WAIT;       break;
    case THREAD_SUSPENDED:      *pThreadStatus = TS_RUNNING;    break;
    default:
        assert(false);
        *pThreadStatus = THREAD_ZOMBIE;
        break;
    }

    if (dvmIsSuspended(thread))
        *pSuspendStatus = SUSPEND_STATUS_SUSPENDED;
    else
        *pSuspendStatus = 0;

    result = true;

bail:
    dvmUnlockThreadList();
    return result;
}

u4 dvmDbgGetThreadSuspendCount(ObjectId threadId)
{
    Object* threadObj;
    Thread* thread;
    u4 result = 0;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    /* lock the thread list, so the thread doesn't vanish while we work */
    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL)
        goto bail;

    result = thread->suspendCount;

bail:
    dvmUnlockThreadList();
    return result;
}

bool dvmDbgThreadExists(ObjectId threadId)
{
    Object* threadObj;
    Thread* thread;
    bool result;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    /* lock the thread list, so the thread doesn't vanish while we work */
    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL)
        result = false;
    else
        result = true;

    dvmUnlockThreadList();
    return result;
}

bool dvmDbgIsSuspended(ObjectId threadId)
{
    Object* threadObj;
    Thread* thread;
    bool result = false;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    /* lock the thread list, so the thread doesn't vanish while we work */
    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL)
        goto bail;

    result = dvmIsSuspended(thread);

bail:
    dvmUnlockThreadList();
    return result;
}

ObjectId dvmDbgGetSystemThreadGroupId()
{
    Object* groupObj = dvmGetSystemThreadGroup();
    return objectToObjectId(groupObj);
}

ObjectId dvmDbgGetMainThreadGroupId()
{
    Object* groupObj = dvmGetMainThreadGroup();
    return objectToObjectId(groupObj);
}

char* dvmDbgGetThreadName(ObjectId threadId)
{
    Object* threadObj;
    StringObject* nameStr;
    char* str;
    char* result;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    nameStr = (StringObject*) dvmGetFieldObject(threadObj,
                                                gDvm.offJavaLangThread_name);
    str = dvmCreateCstrFromString(nameStr);
    result = (char*) malloc(strlen(str) + 20);

    /* lock the thread list, so the thread doesn't vanish while we work */
    dvmLockThreadList(NULL);
    Thread* thread = threadObjToThread(threadObj);
    if (thread != NULL)
        sprintf(result, "<%d> %s", thread->threadId, str);
    else
        sprintf(result, "%s", str);
    dvmUnlockThreadList();

    free(str);
    return result;
}

ObjectId dvmDbgGetThreadGroup(ObjectId threadId)
{
    Object* threadObj;
    Object* group;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    group = dvmGetFieldObject(threadObj, gDvm.offJavaLangThread_group);
    return objectToObjectId(group);
}


char* dvmDbgGetThreadGroupName(ObjectId threadGroupId)
{
    Object* threadGroup;
    StringObject* nameStr;

    threadGroup = objectIdToObject(threadGroupId);
    assert(threadGroup != NULL);

    nameStr = (StringObject*)
        dvmGetFieldObject(threadGroup, gDvm.offJavaLangThreadGroup_name);
    return dvmCreateCstrFromString(nameStr);
}

ObjectId dvmDbgGetThreadGroupParent(ObjectId threadGroupId)
{
    Object* threadGroup;
    Object* parent;

    threadGroup = objectIdToObject(threadGroupId);
    assert(threadGroup != NULL);

    parent = dvmGetFieldObject(threadGroup, gDvm.offJavaLangThreadGroup_parent);
    return objectToObjectId(parent);
}

void dvmDbgGetThreadGroupThreads(ObjectId threadGroupId,
    ObjectId** ppThreadIds, u4* pThreadCount)
{
    Object* targetThreadGroup = NULL;
    Thread* thread;
    int count;

    if (threadGroupId != THREAD_GROUP_ALL) {
        targetThreadGroup = objectIdToObject(threadGroupId);
        assert(targetThreadGroup != NULL);
    }

    dvmLockThreadList(NULL);

    thread = gDvm.threadList;
    count = 0;
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        Object* group;

        if (thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState))
            continue;

        if (thread->threadObj == NULL)
            continue;

        group = dvmGetFieldObject(thread->threadObj,
                    gDvm.offJavaLangThread_group);
        if (threadGroupId == THREAD_GROUP_ALL || group == targetThreadGroup)
            count++;
    }

    *pThreadCount = count;

    if (count == 0) {
        *ppThreadIds = NULL;
    } else {
        ObjectId* ptr;
        ptr = *ppThreadIds = (ObjectId*) malloc(sizeof(ObjectId) * count);

        for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
            Object* group;

            if (thread->handle == dvmJdwpGetDebugThread(gDvm.jdwpState))
                continue;

            if (thread->threadObj == NULL)
                continue;

            group = dvmGetFieldObject(thread->threadObj,
                        gDvm.offJavaLangThread_group);
            if (threadGroupId == THREAD_GROUP_ALL || group == targetThreadGroup)
            {
                *ptr++ = objectToObjectId(thread->threadObj);
                count--;
            }
        }

        assert(count == 0);
    }

    dvmUnlockThreadList();
}

void dvmDbgGetAllThreads(ObjectId** ppThreadIds, u4* pThreadCount)
{
    dvmDbgGetThreadGroupThreads(THREAD_GROUP_ALL, ppThreadIds, pThreadCount);
}


int dvmDbgGetThreadFrameCount(ObjectId threadId)
{
    Object* threadObj;
    Thread* thread;
    int count = -1;

    threadObj = objectIdToObject(threadId);

    dvmLockThreadList(NULL);
    thread = threadObjToThread(threadObj);
    if (thread != NULL) {
        count = dvmComputeExactFrameDepth(thread->interpSave.curFrame);
    }
    dvmUnlockThreadList();

    return count;
}

bool dvmDbgGetThreadFrame(ObjectId threadId, int num, FrameId* pFrameId,
    JdwpLocation* pLoc)
{
    Object* threadObj;
    Thread* thread;
    void* framePtr;
    int count;

    threadObj = objectIdToObject(threadId);

    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL)
        goto bail;

    framePtr = thread->interpSave.curFrame;
    count = 0;
    while (framePtr != NULL) {
        const StackSaveArea* saveArea = SAVEAREA_FROM_FP(framePtr);
        const Method* method = saveArea->method;

        if (!dvmIsBreakFrame((u4*)framePtr)) {
            if (count == num) {
                *pFrameId = frameToFrameId(framePtr);
                if (dvmIsInterfaceClass(method->clazz))
                    pLoc->typeTag = TT_INTERFACE;
                else
                    pLoc->typeTag = TT_CLASS;
                pLoc->classId = classObjectToRefTypeId(method->clazz);
                pLoc->methodId = methodToMethodId(method);
                if (dvmIsNativeMethod(method))
                    pLoc->idx = (u8)-1;
                else
                    pLoc->idx = saveArea->xtra.currentPc - method->insns;
                dvmUnlockThreadList();
                return true;
            }

            count++;
        }

        framePtr = saveArea->prevFrame;
    }

bail:
    dvmUnlockThreadList();
    return false;
}

ObjectId dvmDbgGetThreadSelfId()
{
    Thread* self = dvmThreadSelf();
    return objectToObjectId(self->threadObj);
}

void dvmDbgSuspendVM(bool isEvent)
{
    dvmSuspendAllThreads(isEvent ? SUSPEND_FOR_DEBUG_EVENT : SUSPEND_FOR_DEBUG);
}

void dvmDbgResumeVM()
{
    dvmResumeAllThreads(SUSPEND_FOR_DEBUG);
}

void dvmDbgSuspendThread(ObjectId threadId)
{
    Object* threadObj = objectIdToObject(threadId);
    Thread* thread;

    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL) {
        /* can happen if our ThreadDeath notify crosses in the mail */
        ALOGW("WARNING: threadid=%llx obj=%p no match", threadId, threadObj);
    } else {
        dvmSuspendThread(thread);
    }

    dvmUnlockThreadList();
}

void dvmDbgResumeThread(ObjectId threadId)
{
    Object* threadObj = objectIdToObject(threadId);
    Thread* thread;

    dvmLockThreadList(NULL);

    thread = threadObjToThread(threadObj);
    if (thread == NULL) {
        ALOGW("WARNING: threadid=%llx obj=%p no match", threadId, threadObj);
    } else {
        dvmResumeThread(thread);
    }

    dvmUnlockThreadList();
}

void dvmDbgSuspendSelf()
{
    dvmSuspendSelf(true);
}

static Object* getThisObject(const u4* framePtr)
{
    const StackSaveArea* saveArea = SAVEAREA_FROM_FP(framePtr);
    const Method* method = saveArea->method;
    int argOffset = method->registersSize - method->insSize;
    Object* thisObj;

    if (method == NULL) {
        /* this is a "break" frame? */
        assert(false);
        return NULL;
    }

    LOGVV("  Pulling this object for frame at %p", framePtr);
    LOGVV("    Method='%s' native=%d static=%d this=%p",
        method->name, dvmIsNativeMethod(method),
        dvmIsStaticMethod(method), (Object*) framePtr[argOffset]);

    if (dvmIsNativeMethod(method) || dvmIsStaticMethod(method))
        thisObj = NULL;
    else
        thisObj = (Object*) framePtr[argOffset];

    if (thisObj != NULL && !dvmIsHeapAddress(thisObj)) {
        ALOGW("Debugger: invalid 'this' pointer %p in %s.%s; returning NULL",
            framePtr, method->clazz->descriptor, method->name);
        thisObj = NULL;
    }

    return thisObj;
}

bool dvmDbgGetThisObject(ObjectId threadId, FrameId frameId, ObjectId* pThisId)
{
    const u4* framePtr = frameIdToFrame(frameId);
    Object* thisObj;

    UNUSED_PARAMETER(threadId);

    thisObj = getThisObject(framePtr);

    *pThisId = objectToObjectId(thisObj);
    return true;
}

void dvmDbgGetLocalValue(ObjectId threadId, FrameId frameId, int slot,
    u1 tag, u1* buf, int expectedLen)
{
    const u4* framePtr = frameIdToFrame(frameId);
    Object* objVal;
    u4 intVal;
    u8 longVal;

    UNUSED_PARAMETER(threadId);

    slot = untweakSlot(slot, framePtr);     // Eclipse workaround

    switch (tag) {
    case JT_BOOLEAN:
        assert(expectedLen == 1);
        intVal = framePtr[slot];
        set1(buf+1, intVal != 0);
        break;
    case JT_BYTE:
        assert(expectedLen == 1);
        intVal = framePtr[slot];
        set1(buf+1, intVal);
        break;
    case JT_SHORT:
    case JT_CHAR:
        assert(expectedLen == 2);
        intVal = framePtr[slot];
        set2BE(buf+1, intVal);
        break;
    case JT_INT:
    case JT_FLOAT:
        assert(expectedLen == 4);
        intVal = framePtr[slot];
        set4BE(buf+1, intVal);
        break;
    case JT_ARRAY:
        assert(expectedLen == sizeof(ObjectId));
        {
            /* convert to "ObjectId" */
            objVal = (Object*)framePtr[slot];
            if (objVal != NULL && !dvmIsHeapAddress(objVal)) {
                ALOGW("JDWP: slot %d expected to hold array, %p invalid",
                    slot, objVal);
                dvmAbort();         // DEBUG: make it obvious
                objVal = NULL;
                tag = JT_OBJECT;    // JT_ARRAY not expected for NULL ref
            }
            dvmSetObjectId(buf+1, objectToObjectId(objVal));
        }
        break;
    case JT_OBJECT:
        assert(expectedLen == sizeof(ObjectId));
        {
            /* convert to "ObjectId" */
            objVal = (Object*)framePtr[slot];

            if (objVal != NULL && !dvmIsHeapAddress(objVal)) {
                ALOGW("JDWP: slot %d expected to hold object, %p invalid",
                    slot, objVal);
                dvmAbort();         // DEBUG: make it obvious
                objVal = NULL;
            }
            tag = tagFromObject(objVal);
            dvmSetObjectId(buf+1, objectToObjectId(objVal));
        }
        break;
    case JT_DOUBLE:
    case JT_LONG:
        assert(expectedLen == 8);
        memcpy(&longVal, &framePtr[slot], 8);
        set8BE(buf+1, longVal);
        break;
    default:
        ALOGE("ERROR: unhandled tag '%c'", tag);
        assert(false);
        break;
    }

    /* prepend tag, which may have been updated */
    set1(buf, tag);
}

void dvmDbgSetLocalValue(ObjectId threadId, FrameId frameId, int slot, u1 tag,
    u8 value, int width)
{
    u4* framePtr = frameIdToFrame(frameId);

    UNUSED_PARAMETER(threadId);

    slot = untweakSlot(slot, framePtr);     // Eclipse workaround

    switch (tag) {
    case JT_BOOLEAN:
        assert(width == 1);
        framePtr[slot] = (u4)value;
        break;
    case JT_BYTE:
        assert(width == 1);
        framePtr[slot] = (u4)value;
        break;
    case JT_SHORT:
    case JT_CHAR:
        assert(width == 2);
        framePtr[slot] = (u4)value;
        break;
    case JT_INT:
    case JT_FLOAT:
        assert(width == 4);
        framePtr[slot] = (u4)value;
        break;
    case JT_STRING:
        /* The debugger calls VirtualMachine.CreateString to create a new
         * string, then uses this to set the object reference, when you
         * edit a String object */
    case JT_ARRAY:
    case JT_OBJECT:
        assert(width == sizeof(ObjectId));
        framePtr[slot] = (u4) objectIdToObject(value);
        break;
    case JT_DOUBLE:
    case JT_LONG:
        assert(width == 8);
        memcpy(&framePtr[slot], &value, 8);
        break;
    case JT_VOID:
    case JT_CLASS_OBJECT:
    case JT_THREAD:
    case JT_THREAD_GROUP:
    case JT_CLASS_LOADER:
        /* not expecting these from debugger; fall through to failure */
    default:
        ALOGE("ERROR: unhandled tag '%c'", tag);
        assert(false);
        break;
    }
}



void dvmDbgPostLocationEvent(const Method* method, int pcOffset,
    Object* thisPtr, int eventFlags)
{
    JdwpLocation loc;

    if (dvmIsInterfaceClass(method->clazz))
        loc.typeTag = TT_INTERFACE;
    else
        loc.typeTag = TT_CLASS;
    loc.classId = classObjectToRefTypeId(method->clazz);
    loc.methodId = methodToMethodId(method);
    loc.idx = pcOffset;


    if (dvmJdwpPostLocationEvent(gDvm.jdwpState, &loc,
            objectToObjectIdNoReg(thisPtr), eventFlags))
    {
        classObjectToRefTypeId(method->clazz);
        objectToObjectId(thisPtr);
    }
}

void dvmDbgPostException(void* throwFp, int throwRelPc, void* catchFp,
    int catchRelPc, Object* exception)
{
    JdwpLocation throwLoc, catchLoc;
    const Method* throwMeth;
    const Method* catchMeth;

    throwMeth = SAVEAREA_FROM_FP(throwFp)->method;
    if (dvmIsInterfaceClass(throwMeth->clazz))
        throwLoc.typeTag = TT_INTERFACE;
    else
        throwLoc.typeTag = TT_CLASS;
    throwLoc.classId = classObjectToRefTypeId(throwMeth->clazz);
    throwLoc.methodId = methodToMethodId(throwMeth);
    throwLoc.idx = throwRelPc;

    if (catchRelPc < 0) {
        memset(&catchLoc, 0, sizeof(catchLoc));
    } else {
        catchMeth = SAVEAREA_FROM_FP(catchFp)->method;
        if (dvmIsInterfaceClass(catchMeth->clazz))
            catchLoc.typeTag = TT_INTERFACE;
        else
            catchLoc.typeTag = TT_CLASS;
        catchLoc.classId = classObjectToRefTypeId(catchMeth->clazz);
        catchLoc.methodId = methodToMethodId(catchMeth);
        catchLoc.idx = catchRelPc;
    }

    /* need this for InstanceOnly filters */
    Object* thisObj = getThisObject((u4*)throwFp);

    dvmJdwpPostException(gDvm.jdwpState, &throwLoc,
        objectToObjectIdNoReg(exception),
        classObjectToRefTypeId(exception->clazz), &catchLoc,
        objectToObjectId(thisObj));
}

void dvmDbgPostThreadStart(Thread* thread)
{
    if (gDvm.debuggerActive) {
        dvmJdwpPostThreadChange(gDvm.jdwpState,
            objectToObjectId(thread->threadObj), true);
    }
    if (gDvm.ddmThreadNotification)
        dvmDdmSendThreadNotification(thread, true);
}

void dvmDbgPostThreadDeath(Thread* thread)
{
    if (gDvm.debuggerActive) {
        dvmJdwpPostThreadChange(gDvm.jdwpState,
            objectToObjectId(thread->threadObj), false);
    }
    if (gDvm.ddmThreadNotification)
        dvmDdmSendThreadNotification(thread, false);
}

void dvmDbgPostClassPrepare(ClassObject* clazz)
{
    const char* signature;
    int tag;

    if (dvmIsInterfaceClass(clazz))
        tag = TT_INTERFACE;
    else
        tag = TT_CLASS;

    // TODO - we currently always send both "verified" and "prepared" since
    // debuggers seem to like that.  There might be some advantage to honesty,
    // since the class may not yet be verified.
    signature = jniSignature(clazz);
    dvmJdwpPostClassPrepare(gDvm.jdwpState, tag, classObjectToRefTypeId(clazz),
        signature, CS_VERIFIED | CS_PREPARED);
}

bool dvmDbgWatchLocation(const JdwpLocation* pLoc)
{
    Method* method = methodIdToMethod(pLoc->classId, pLoc->methodId);
    assert(!dvmIsNativeMethod(method));
    dvmAddBreakAddr(method, pLoc->idx);
    return true;        /* assume success */
}

void dvmDbgUnwatchLocation(const JdwpLocation* pLoc)
{
    Method* method = methodIdToMethod(pLoc->classId, pLoc->methodId);
    assert(!dvmIsNativeMethod(method));
    dvmClearBreakAddr(method, pLoc->idx);
}

bool dvmDbgConfigureStep(ObjectId threadId, JdwpStepSize size,
    JdwpStepDepth depth)
{
    Object* threadObj;
    Thread* thread;
    bool result = false;

    threadObj = objectIdToObject(threadId);
    assert(threadObj != NULL);

    dvmLockThreadList(NULL);
    thread = threadObjToThread(threadObj);

    if (thread == NULL) {
        ALOGE("Thread for single-step not found");
        goto bail;
    }
    if (!dvmIsSuspended(thread)) {
        ALOGE("Thread for single-step not suspended");
        assert(!"non-susp step");      // I want to know if this can happen
        goto bail;
    }

    assert(dvmIsSuspended(thread));
    if (!dvmAddSingleStep(thread, size, depth))
        goto bail;

    result = true;

bail:
    dvmUnlockThreadList();
    return result;
}

void dvmDbgUnconfigureStep(ObjectId threadId)
{
    UNUSED_PARAMETER(threadId);

    /* right now it's global, so don't need to find Thread */
    dvmClearSingleStep(NULL);
}

JdwpError dvmDbgInvokeMethod(ObjectId threadId, ObjectId objectId,
    RefTypeId classId, MethodId methodId, u4 numArgs, ObjectId* argArray,
    u4 options, u1* pResultTag, u8* pResultValue, ObjectId* pExceptObj)
{
    Object* threadObj = objectIdToObject(threadId);

    dvmLockThreadList(NULL);

    Thread* targetThread = threadObjToThread(threadObj);
    if (targetThread == NULL) {
        dvmUnlockThreadList();
        return ERR_INVALID_THREAD;       /* thread does not exist */
    }
    if (!targetThread->invokeReq.ready) {
        dvmUnlockThreadList();
        return ERR_INVALID_THREAD;       /* thread not stopped by event */
    }

    if (targetThread->suspendCount > 1) {
        ALOGW("threadid=%d: suspend count on threadid=%d is %d, too deep "
             "for method exec",
            dvmThreadSelf()->threadId, targetThread->threadId,
            targetThread->suspendCount);
        dvmUnlockThreadList();
        return ERR_THREAD_SUSPENDED;     /* probably not expected here */
    }


    targetThread->invokeReq.obj = objectIdToObject(objectId);
    targetThread->invokeReq.thread = threadObj;
    targetThread->invokeReq.clazz = refTypeIdToClassObject(classId);
    targetThread->invokeReq.method = methodIdToMethod(classId, methodId);
    targetThread->invokeReq.numArgs = numArgs;
    targetThread->invokeReq.argArray = argArray;
    targetThread->invokeReq.options = options;
    targetThread->invokeReq.invokeNeeded = true;

    dvmUnlockThreadList();

    Thread* self = dvmThreadSelf();
    ThreadStatus oldStatus = dvmChangeStatus(self, THREAD_VMWAIT);

    ALOGV("    Transferring control to event thread");
    dvmLockMutex(&targetThread->invokeReq.lock);

    if ((options & INVOKE_SINGLE_THREADED) == 0) {
        ALOGV("      Resuming all threads");
        dvmResumeAllThreads(SUSPEND_FOR_DEBUG_EVENT);
    } else {
        ALOGV("      Resuming event thread only");
        dvmResumeThread(targetThread);
    }

    while (targetThread->invokeReq.invokeNeeded) {
        pthread_cond_wait(&targetThread->invokeReq.cv,
                          &targetThread->invokeReq.lock);
    }
    dvmUnlockMutex(&targetThread->invokeReq.lock);
    ALOGV("    Control has returned from event thread");

    /* wait for thread to re-suspend itself */
    dvmWaitForSuspend(targetThread);

    dvmChangeStatus(self, oldStatus);

    if ((options & INVOKE_SINGLE_THREADED) == 0) {
        ALOGV("      Suspending all threads");
        dvmSuspendAllThreads(SUSPEND_FOR_DEBUG_EVENT);
        ALOGV("      Resuming event thread to balance the count");
        dvmResumeThread(targetThread);
    }

    *pResultTag = targetThread->invokeReq.resultTag;
    if (isTagPrimitive(targetThread->invokeReq.resultTag))
        *pResultValue = targetThread->invokeReq.resultValue.j;
    else {
        Object* tmpObj = (Object*)targetThread->invokeReq.resultValue.l;
        *pResultValue = objectToObjectId(tmpObj);
    }
    *pExceptObj = targetThread->invokeReq.exceptObj;
    return targetThread->invokeReq.err;
}

static u1 getReturnTypeBasicTag(const Method* method)
{
    const char* descriptor = dexProtoGetReturnType(&method->prototype);
    return basicTagFromDescriptor(descriptor);
}

void dvmDbgExecuteMethod(DebugInvokeReq* pReq)
{
    Thread* self = dvmThreadSelf();
    const Method* meth;
    Object* oldExcept;
    ThreadStatus oldStatus;

    oldExcept = dvmGetException(self);
    if (oldExcept != NULL) {
        dvmAddTrackedAlloc(oldExcept, self);
        dvmClearException(self);
    }

    oldStatus = dvmChangeStatus(self, THREAD_RUNNING);

    if ((pReq->options & INVOKE_NONVIRTUAL) != 0 || pReq->obj == NULL ||
        dvmIsDirectMethod(pReq->method))
    {
        meth = pReq->method;
    } else {
        meth = dvmGetVirtualizedMethod(pReq->clazz, pReq->method);
    }
    assert(meth != NULL);

    assert(sizeof(jvalue) == sizeof(u8));

    IF_ALOGV() {
        char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
        ALOGV("JDWP invoking method %p/%p %s.%s:%s",
            pReq->method, meth, meth->clazz->descriptor, meth->name, desc);
        free(desc);
    }

    dvmCallMethodA(self, meth, pReq->obj, false, &pReq->resultValue,
        (jvalue*)pReq->argArray);
    pReq->exceptObj = objectToObjectId(dvmGetException(self));
    pReq->resultTag = getReturnTypeBasicTag(meth);
    if (pReq->exceptObj != 0) {
        Object* exc = dvmGetException(self);
        ALOGD("  JDWP invocation returning with exceptObj=%p (%s)",
            exc, exc->clazz->descriptor);
        //dvmLogExceptionStackTrace();
        dvmClearException(self);
        pReq->resultValue.j = 0; /*0xadadadad;*/
    } else if (pReq->resultTag == JT_OBJECT) {
        /* if no exception thrown, examine object result more closely */
        u1 newTag = tagFromObject((Object*)pReq->resultValue.l);
        if (newTag != pReq->resultTag) {
            LOGVV("  JDWP promoted result from %d to %d",
                pReq->resultTag, newTag);
            pReq->resultTag = newTag;
        }

        objectToObjectId((Object*)pReq->resultValue.l);
    }

    if (oldExcept != NULL) {
        dvmSetException(self, oldExcept);
        dvmReleaseTrackedAlloc(oldExcept, self);
    }
    dvmChangeStatus(self, oldStatus);
}

// for dvmAddressSetForLine
struct AddressSetContext {
    bool lastAddressValid;
    u4 lastAddress;
    u4 lineNum;
    AddressSet *pSet;
};

// for dvmAddressSetForLine
static int addressSetCb (void *cnxt, u4 address, u4 lineNum)
{
    AddressSetContext *pContext = (AddressSetContext *)cnxt;

    if (lineNum == pContext->lineNum) {
        if (!pContext->lastAddressValid) {
            // Everything from this address until the next line change is ours
            pContext->lastAddress = address;
            pContext->lastAddressValid = true;
        }
    } else if (pContext->lastAddressValid) { // and the line number is new
        u4 i;
        // Add everything from the last entry up until here to the set
        for (i = pContext->lastAddress; i < address; i++) {
            dvmAddressSetSet(pContext->pSet, i);
        }

        pContext->lastAddressValid = false;
    }

    // there may be multiple entries for a line
    return 0;
}
const AddressSet *dvmAddressSetForLine(const Method* method, int line)
{
    AddressSet *result;
    const DexFile *pDexFile = method->clazz->pDvmDex->pDexFile;
    u4 insnsSize = dvmGetMethodInsnsSize(method);
    AddressSetContext context;

    result = (AddressSet*)calloc(1, sizeof(AddressSet) + (insnsSize/8) + 1);
    result->setSize = insnsSize;

    memset(&context, 0, sizeof(context));
    context.pSet = result;
    context.lineNum = line;
    context.lastAddressValid = false;

    dexDecodeDebugInfo(pDexFile, dvmGetMethodCode(method),
        method->clazz->descriptor,
        method->prototype.protoIdx,
        method->accessFlags,
        addressSetCb, NULL, &context);

    // If the line number was the last in the position table...
    if (context.lastAddressValid) {
        u4 i;
        for (i = context.lastAddress; i < insnsSize; i++) {
            dvmAddressSetSet(result, i);
        }
    }

    return result;
}



bool dvmDbgDdmHandlePacket(const u1* buf, int dataLen, u1** pReplyBuf,
    int* pReplyLen)
{
    return dvmDdmHandlePacket(buf, dataLen, pReplyBuf, pReplyLen);
}

void dvmDbgDdmConnected()
{
    dvmDdmConnected();
}

void dvmDbgDdmDisconnected()
{
    dvmDdmDisconnected();
}

void dvmDbgDdmSendChunk(int type, size_t len, const u1* buf)
{
    assert(buf != NULL);
    struct iovec vec[1] = { {(void*)buf, len} };
    dvmDbgDdmSendChunkV(type, vec, 1);
}

void dvmDbgDdmSendChunkV(int type, const struct iovec* iov, int iovcnt)
{
    if (gDvm.jdwpState == NULL) {
        ALOGV("Debugger thread not active, ignoring DDM send (t=0x%08x)",
            type);
        return;
    }

    dvmJdwpDdmSendChunkV(gDvm.jdwpState, type, iov, iovcnt);
}
