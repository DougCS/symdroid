static void ReportJniError() {
    dvmDumpThread(dvmThreadSelf(), false);
    dvmAbort();
}

#ifdef WITH_JNI_STACK_CHECK
# define COMPUTE_STACK_SUM(_self)   computeStackSum(_self);
# define CHECK_STACK_SUM(_self)     checkStackSum(_self);

static void computeStackSum(Thread* self) {
    const u1* low = (const u1*)SAVEAREA_FROM_FP(self->interpSave.curFrame);
    u4 crc = dvmInitCrc32();
    self->stackCrc = 0;
    crc = dvmComputeCrc32(crc, low, self->interpStackStart - low);
    self->stackCrc = crc;
}

static void checkStackSum(Thread* self) {
    const u1* low = (const u1*)SAVEAREA_FROM_FP(self->interpSave.curFrame);
    u4 stackCrc = self->stackCrc;
    self->stackCrc = 0;
    u4 crc = dvmInitCrc32();
    crc = dvmComputeCrc32(crc, low, self->interpStackStart - low);
    if (crc != stackCrc) {
        const Method* meth = dvmGetCurrentJNIMethod();
        if (dvmComputeExactFrameDepth(self->interpSave.curFrame) == 1) {
            ALOGD("JNI: bad stack CRC (0x%08x) -- okay during init", stackCrc);
        } else if (strcmp(meth->name, "nativeLoad") == 0 &&
                (strcmp(meth->clazz->descriptor, "Ljava/lang/Runtime;") == 0)) {
            ALOGD("JNI: bad stack CRC (0x%08x) -- okay during JNI_OnLoad", stackCrc);
        } else {
            ALOGW("JNI: bad stack CRC (%08x vs %08x)", crc, stackCrc);
            ReportJniError();
        }
    }
    self->stackCrc = (u4) -1;       /* make logic errors more noticeable */
}

#else
# define COMPUTE_STACK_SUM(_self)   ((void)0)
# define CHECK_STACK_SUM(_self)     ((void)0)
#endif

class ScopedJniThreadState {
public:
    explicit ScopedJniThreadState(JNIEnv* env) {
        mSelf = ((JNIEnvExt*) env)->self;

        if (UNLIKELY(gDvmJni.workAroundAppJniBugs)) {
            Thread* self = gDvmJni.workAroundAppJniBugs ? dvmThreadSelf() : mSelf;
            if (self != mSelf) {
                ALOGE("JNI ERROR: env->self != thread-self (%p vs. %p); auto-correcting", mSelf, self);
                mSelf = self;
            }
        }

        CHECK_STACK_SUM(mSelf);
        dvmChangeStatus(mSelf, THREAD_RUNNING);
    }

    ~ScopedJniThreadState() {
        dvmChangeStatus(mSelf, THREAD_NATIVE);
        COMPUTE_STACK_SUM(mSelf);
    }

    inline Thread* self() {
        return mSelf;
    }

private:
    Thread* mSelf;

    ScopedJniThreadState(const ScopedJniThreadState&);
    void operator=(const ScopedJniThreadState&);
};

#define kGlobalRefsTableInitialSize 512
#define kGlobalRefsTableMaxSize     51200

#define kWeakGlobalRefsTableInitialSize 16

#define kPinTableInitialSize        16
#define kPinTableMaxSize            1024
#define kPinComplainThreshold       10

bool dvmJniStartup() {
    if (!gDvm.jniGlobalRefTable.init(kGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindGlobal)) {
        return false;
    }
    if (!gDvm.jniWeakGlobalRefTable.init(kWeakGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindWeakGlobal)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniGlobalRefLock);
    dvmInitMutex(&gDvm.jniWeakGlobalRefLock);

    if (!dvmInitReferenceTable(&gDvm.jniPinRefTable, kPinTableInitialSize, kPinTableMaxSize)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniPinRefLock);

    return true;
}

void dvmJniShutdown() {
    gDvm.jniGlobalRefTable.destroy();
    gDvm.jniWeakGlobalRefTable.destroy();
    dvmClearReferenceTable(&gDvm.jniPinRefTable);
}

bool dvmIsBadJniVersion(int version) {
  return version != JNI_VERSION_1_2 && version != JNI_VERSION_1_4 && version != JNI_VERSION_1_6;
}

JNIEnvExt* dvmGetJNIEnvForThread() {
    Thread* self = dvmThreadSelf();
    if (self == NULL) {
        return NULL;
    }
    return (JNIEnvExt*) dvmGetThreadJNIEnv(self);
}

Object* dvmDecodeIndirectRef(Thread* self, jobject jobj) {
    if (jobj == NULL) {
        return NULL;
    }

    switch (indirectRefKind(jobj)) {
    case kIndirectKindLocal:
        {
            Object* result = self->jniLocalRefTable.get(jobj);
            if (UNLIKELY(result == NULL)) {
                ALOGE("JNI ERROR (app bug): use of deleted local reference (%p)", jobj);
                ReportJniError();
            }
            return result;
        }
    case kIndirectKindGlobal:
        {
            IndirectRefTable* pRefTable = &gDvm.jniGlobalRefTable;
            ScopedPthreadMutexLock lock(&gDvm.jniGlobalRefLock);
            Object* result = pRefTable->get(jobj);
            if (UNLIKELY(result == NULL)) {
                ALOGE("JNI ERROR (app bug): use of deleted global reference (%p)", jobj);
                ReportJniError();
            }
            return result;
        }
    case kIndirectKindWeakGlobal:
        {
            IndirectRefTable* pRefTable = &gDvm.jniWeakGlobalRefTable;
            ScopedPthreadMutexLock lock(&gDvm.jniWeakGlobalRefLock);
            Object* result = pRefTable->get(jobj);
            if (result == kClearedJniWeakGlobal) {
                result = NULL;
            } else if (UNLIKELY(result == NULL)) {
                ALOGE("JNI ERROR (app bug): use of deleted weak global reference (%p)", jobj);
                ReportJniError();
            }
            return result;
        }
    case kIndirectKindInvalid:
    default:
        if (UNLIKELY(gDvmJni.workAroundAppJniBugs)) {
            return reinterpret_cast<Object*>(jobj);
        }
        ALOGW("Invalid indirect reference %p in decodeIndirectRef", jobj);
        ReportJniError();
        return kInvalidIndirectRefObject;
    }
}

static void AddLocalReferenceFailure(IndirectRefTable* pRefTable) {
    pRefTable->dump("JNI local");
    ALOGE("Failed adding to JNI local ref table (has %zd entries)", pRefTable->capacity());
    ReportJniError();
}

static inline jobject addLocalReference(Thread* self, Object* obj) {
    if (obj == NULL) {
        return NULL;
    }

    IndirectRefTable* pRefTable = &self->jniLocalRefTable;
    void* curFrame = self->interpSave.curFrame;
    u4 cookie = SAVEAREA_FROM_FP(curFrame)->xtra.localRefCookie;
    jobject jobj = (jobject) pRefTable->add(cookie, obj);
    if (UNLIKELY(jobj == NULL)) {
        AddLocalReferenceFailure(pRefTable);
    }

    if (UNLIKELY(gDvmJni.workAroundAppJniBugs)) {
        return reinterpret_cast<jobject>(obj);
    }
    return jobj;
}

static bool ensureLocalCapacity(Thread* self, int capacity) {
    int numEntries = self->jniLocalRefTable.capacity();
    return ((kJniLocalRefMax - numEntries) >= capacity);
}

static void deleteLocalReference(Thread* self, jobject jobj) {
    if (jobj == NULL) {
        return;
    }

    IndirectRefTable* pRefTable = &self->jniLocalRefTable;
    void* curFrame = self->interpSave.curFrame;
    u4 cookie = SAVEAREA_FROM_FP(curFrame)->xtra.localRefCookie;
    if (!pRefTable->remove(cookie, jobj)) {
        ALOGW("JNI WARNING: DeleteLocalRef(%p) failed to find entry", jobj);
    }
}

static jobject addGlobalReference(Object* obj) {
    if (obj == NULL) {
        return NULL;
    }

    ALOGI("adding obj=%p", obj);
    dvmDumpThread(dvmThreadSelf(), false);

    if (false && dvmIsClassObject((Object*)obj)) {
        ClassObject* clazz = (ClassObject*) obj;
        ALOGI("-------");
        ALOGI("Adding global ref on class %s", clazz->descriptor);
        dvmDumpThread(dvmThreadSelf(), false);
    }
    if (false && ((Object*)obj)->clazz == gDvm.classJavaLangString) {
        StringObject* strObj = (StringObject*) obj;
        char* str = dvmCreateCstrFromString(strObj);
        if (strcmp(str, "sync-response") == 0) {
            ALOGI("-------");
            ALOGI("Adding global ref on string '%s'", str);
            dvmDumpThread(dvmThreadSelf(), false);
            dvmAbort();
        }
        free(str);
    }
    if (false && ((Object*)obj)->clazz == gDvm.classArrayByte) {
        ArrayObject* arrayObj = (ArrayObject*) obj;
        if (arrayObj->length == 8192 /*&&
            dvmReferenceTableEntries(&gDvm.jniGlobalRefTable) > 400*/)
        {
            ALOGI("Adding global ref on byte array %p (len=%d)",
                arrayObj, arrayObj->length);
            dvmDumpThread(dvmThreadSelf(), false);
        }
    }

    ScopedPthreadMutexLock lock(&gDvm.jniGlobalRefLock);

    jobject jobj = (jobject) gDvm.jniGlobalRefTable.add(IRT_FIRST_SEGMENT, obj);
    if (jobj == NULL) {
        gDvm.jniGlobalRefTable.dump("JNI global");
        ALOGE("Failed adding to JNI global ref table (%zd entries)",
                gDvm.jniGlobalRefTable.capacity());
        ReportJniError();
    }

    LOGVV("GREF add %p  (%s.%s)", obj,
        dvmGetCurrentJNIMethod()->clazz->descriptor,
        dvmGetCurrentJNIMethod()->name);

    return jobj;
}

static jobject addWeakGlobalReference(Object* obj) {
    if (obj == NULL) {
        return NULL;
    }

    ScopedPthreadMutexLock lock(&gDvm.jniWeakGlobalRefLock);
    IndirectRefTable *table = &gDvm.jniWeakGlobalRefTable;
    jobject jobj = (jobject) table->add(IRT_FIRST_SEGMENT, obj);
    if (jobj == NULL) {
        gDvm.jniWeakGlobalRefTable.dump("JNI weak global");
        ALOGE("Failed adding to JNI weak global ref table (%zd entries)", table->capacity());
        ReportJniError();
    }
    return jobj;
}

static void deleteWeakGlobalReference(jobject jobj) {
    if (jobj == NULL) {
        return;
    }

    ScopedPthreadMutexLock lock(&gDvm.jniWeakGlobalRefLock);
    IndirectRefTable *table = &gDvm.jniWeakGlobalRefTable;
    if (!table->remove(IRT_FIRST_SEGMENT, jobj)) {
        ALOGW("JNI: DeleteWeakGlobalRef(%p) failed to find entry", jobj);
    }
}

static void deleteGlobalReference(jobject jobj) {
    if (jobj == NULL) {
        return;
    }

    ScopedPthreadMutexLock lock(&gDvm.jniGlobalRefLock);
    if (!gDvm.jniGlobalRefTable.remove(IRT_FIRST_SEGMENT, jobj)) {
        ALOGW("JNI: DeleteGlobalRef(%p) failed to find entry", jobj);
        return;
    }
}

static void pinPrimitiveArray(ArrayObject* arrayObj) {
    if (arrayObj == NULL) {
        return;
    }

    ScopedPthreadMutexLock lock(&gDvm.jniPinRefLock);

    if (!dvmAddToReferenceTable(&gDvm.jniPinRefTable, (Object*)arrayObj)) {
        dvmDumpReferenceTable(&gDvm.jniPinRefTable, "JNI pinned array");
        ALOGE("Failed adding to JNI pinned array ref table (%d entries)",
           (int) dvmReferenceTableEntries(&gDvm.jniPinRefTable));
        ReportJniError();
    }

    int count = 0;
    Object** ppObj = gDvm.jniPinRefTable.table;
    while (ppObj < gDvm.jniPinRefTable.nextEntry) {
        if (*ppObj++ == (Object*) arrayObj) {
            count++;
        }
    }

    if (count > kPinComplainThreshold) {
        ALOGW("JNI: pin count on array %p (%s) is now %d",
              arrayObj, arrayObj->clazz->descriptor, count);
    }
}

static void unpinPrimitiveArray(ArrayObject* arrayObj) {
    if (arrayObj == NULL) {
        return;
    }

    ScopedPthreadMutexLock lock(&gDvm.jniPinRefLock);
    if (!dvmRemoveFromReferenceTable(&gDvm.jniPinRefTable,
            gDvm.jniPinRefTable.table, (Object*) arrayObj))
    {
        ALOGW("JNI: unpinPrimitiveArray(%p) failed to find entry (valid=%d)",
            arrayObj, dvmIsHeapAddress((Object*) arrayObj));
        return;
    }
}

void dvmDumpJniReferenceTables() {
    Thread* self = dvmThreadSelf();
    self->jniLocalRefTable.dump("JNI local");
    gDvm.jniGlobalRefTable.dump("JNI global");
    dvmDumpReferenceTable(&gDvm.jniPinRefTable, "JNI pinned array");
}

void dvmDumpJniStats(DebugOutputTarget* target) {
    dvmPrintDebugMessage(target, "JNI: CheckJNI is %s", gDvmJni.useCheckJni ? "on" : "off");
    if (gDvmJni.forceCopy) {
        dvmPrintDebugMessage(target, " (with forcecopy)");
    }
    dvmPrintDebugMessage(target, "; workarounds are %s", gDvmJni.workAroundAppJniBugs ? "on" : "off");

    dvmLockMutex(&gDvm.jniPinRefLock);
    dvmPrintDebugMessage(target, "; pins=%d", dvmReferenceTableEntries(&gDvm.jniPinRefTable));
    dvmUnlockMutex(&gDvm.jniPinRefLock);

    dvmLockMutex(&gDvm.jniGlobalRefLock);
    dvmPrintDebugMessage(target, "; globals=%d", gDvm.jniGlobalRefTable.capacity());
    dvmUnlockMutex(&gDvm.jniGlobalRefLock);

    dvmLockMutex(&gDvm.jniWeakGlobalRefLock);
    size_t weaks = gDvm.jniWeakGlobalRefTable.capacity();
    if (weaks > 0) {
        dvmPrintDebugMessage(target, " (plus %d weak)", weaks);
    }
    dvmUnlockMutex(&gDvm.jniWeakGlobalRefLock);

    dvmPrintDebugMessage(target, "\n\n");
}

jobjectRefType dvmGetJNIRefType(Thread* self, jobject jobj) {
    assert(jobj != NULL);

    Object* obj = dvmDecodeIndirectRef(self, jobj);
    if (obj == reinterpret_cast<Object*>(jobj) && gDvmJni.workAroundAppJniBugs) {
        return self->jniLocalRefTable.contains(obj) ? JNILocalRefType : JNIInvalidRefType;
    } else if (obj == kInvalidIndirectRefObject) {
        return JNIInvalidRefType;
    } else {
        return (jobjectRefType) indirectRefKind(jobj);
    }
}

static void dumpMethods(Method* methods, size_t methodCount, const char* name) {
    size_t i;
    for (i = 0; i < methodCount; ++i) {
        Method* method = &methods[i];
        if (strcmp(name, method->name) == 0) {
            char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
            ALOGE("Candidate: %s.%s:%s", method->clazz->descriptor, name, desc);
            free(desc);
        }
    }
}

static void dumpCandidateMethods(ClassObject* clazz, const char* methodName, const char* signature) {
    ALOGE("ERROR: couldn't find native method");
    ALOGE("Requested: %s.%s:%s", clazz->descriptor, methodName, signature);
    dumpMethods(clazz->virtualMethods, clazz->virtualMethodCount, methodName);
    dumpMethods(clazz->directMethods, clazz->directMethodCount, methodName);
}

static void throwNoSuchMethodError(ClassObject* c, const char* name, const char* sig, const char* kind) {
    std::string msg(StringPrintf("no %s method \"%s.%s%s\"", kind, c->descriptor, name, sig));
    dvmThrowNoSuchMethodError(msg.c_str());
}

static bool dvmRegisterJNIMethod(ClassObject* clazz, const char* methodName,
    const char* signature, void* fnPtr)
{
    if (fnPtr == NULL) {
        return false;
    }

    bool fastJni = false;
    if (*signature == '!') {
        fastJni = true;
        ++signature;
        ALOGV("fast JNI method %s.%s:%s detected", clazz->descriptor, methodName, signature);
    }

    Method* method = dvmFindDirectMethodByDescriptor(clazz, methodName, signature);
    if (method == NULL) {
        method = dvmFindVirtualMethodByDescriptor(clazz, methodName, signature);
    }
    if (method == NULL) {
        dumpCandidateMethods(clazz, methodName, signature);
        throwNoSuchMethodError(clazz, methodName, signature, "static or non-static");
        return false;
    }

    if (!dvmIsNativeMethod(method)) {
        ALOGW("Unable to register: not native: %s.%s:%s", clazz->descriptor, methodName, signature);
        throwNoSuchMethodError(clazz, methodName, signature, "native");
        return false;
    }

    if (fastJni) {
        // In this case, we have extra constraints to check...
        if (dvmIsSynchronizedMethod(method)) {
            ALOGE("fast JNI method %s.%s:%s cannot be synchronized",
                    clazz->descriptor, methodName, signature);
            return false;
        }
        if (!dvmIsStaticMethod(method)) {
            ALOGE("fast JNI method %s.%s:%s cannot be non-static",
                    clazz->descriptor, methodName, signature);
            return false;
        }
    }

    if (method->nativeFunc != dvmResolveNativeMethod) {
        ALOGV("Note: %s.%s:%s was already registered", clazz->descriptor, methodName, signature);
    }

    method->fastJni = fastJni;
    dvmUseJNIBridge(method, fnPtr);

    ALOGV("JNI-registered %s.%s:%s", clazz->descriptor, methodName, signature);
    return true;
}

static const char* builtInPrefixes[] = {
    "Landroid/",
    "Lcom/android/",
    "Lcom/google/android/",
    "Ldalvik/",
    "Ljava/",
    "Ljavax/",
    "Llibcore/",
    "Lorg/apache/harmony/",
};

static bool shouldTrace(Method* method) {
    const char* className = method->clazz->descriptor;
    if (gDvm.jniTrace && strstr(className, gDvm.jniTrace)) {
        return true;
    }
    if (gDvmJni.logThirdPartyJni) {
        for (size_t i = 0; i < NELEM(builtInPrefixes); ++i) {
            if (strstr(className, builtInPrefixes[i]) == className) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void dvmUseJNIBridge(Method* method, void* func) {
    method->shouldTrace = shouldTrace(method);

    method->noRef = true;
    const char* cp = method->shorty;
    while (*++cp != '\0') { // Pre-increment to skip return type.
        if (*cp == 'L') {
            method->noRef = false;
            break;
        }
    }

    DalvikBridgeFunc bridge = gDvmJni.useCheckJni ? dvmCheckCallJNIMethod : dvmCallJNIMethod;
    dvmSetNativeFunc(method, bridge, (const u2*) func);
}

static void appendValue(char type, const JValue value, char* buf, size_t n, bool appendComma)
{
    size_t len = strlen(buf);
    if (len >= n - 32) { // 32 should be longer than anything we could append.
        buf[len - 1] = '.';
        buf[len - 2] = '.';
        buf[len - 3] = '.';
        return;
    }
    char* p = buf + len;
    switch (type) {
    case 'B':
        if (value.b >= 0 && value.b < 10) {
            sprintf(p, "%d", value.b);
        } else {
            sprintf(p, "%#x (%d)", value.b, value.b);
        }
        break;
    case 'C':
        if (value.c < 0x7f && value.c >= ' ') {
            sprintf(p, "U+%x ('%c')", value.c, value.c);
        } else {
            sprintf(p, "U+%x", value.c);
        }
        break;
    case 'D':
        sprintf(p, "%g", value.d);
        break;
    case 'F':
        sprintf(p, "%g", value.f);
        break;
    case 'I':
        sprintf(p, "%d", value.i);
        break;
    case 'L':
        sprintf(p, "%#x", value.i);
        break;
    case 'J':
        sprintf(p, "%lld", value.j);
        break;
    case 'S':
        sprintf(p, "%d", value.s);
        break;
    case 'V':
        strcpy(p, "void");
        break;
    case 'Z':
        strcpy(p, value.z ? "true" : "false");
        break;
    default:
        sprintf(p, "unknown type '%c'", type);
        break;
    }

    if (appendComma) {
        strcat(p, ", ");
    }
}

static void logNativeMethodEntry(const Method* method, const u4* args)
{
    char thisString[32] = { 0 };
    const u4* sp = args;
    if (!dvmIsStaticMethod(method)) {
        sprintf(thisString, "this=0x%08x ", *sp++);
    }

    char argsString[128]= { 0 };
    const char* desc = &method->shorty[1];
    while (*desc != '\0') {
        char argType = *desc++;
        JValue value;
        if (argType == 'D' || argType == 'J') {
            value.j = dvmGetArgLong(sp, 0);
            sp += 2;
        } else {
            value.i = *sp++;
        }
        appendValue(argType, value, argsString, sizeof(argsString),
        *desc != '\0');
    }

    std::string className(dvmHumanReadableDescriptor(method->clazz->descriptor));
    char* signature = dexProtoCopyMethodDescriptor(&method->prototype);
    ALOGI("-> %s %s%s %s(%s)", className.c_str(), method->name, signature, thisString, argsString);
    free(signature);
}

static void logNativeMethodExit(const Method* method, Thread* self, const JValue returnValue)
{
    std::string className(dvmHumanReadableDescriptor(method->clazz->descriptor));
    char* signature = dexProtoCopyMethodDescriptor(&method->prototype);
    if (dvmCheckException(self)) {
        Object* exception = dvmGetException(self);
        std::string exceptionClassName(dvmHumanReadableDescriptor(exception->clazz->descriptor));
        ALOGI("<- %s %s%s threw %s", className.c_str(),
                method->name, signature, exceptionClassName.c_str());
    } else {
        char returnValueString[128] = { 0 };
        char returnType = method->shorty[0];
        appendValue(returnType, returnValue, returnValueString, sizeof(returnValueString), false);
        ALOGI("<- %s %s%s returned %s", className.c_str(),
                method->name, signature, returnValueString);
    }
    free(signature);
}

const Method* dvmGetCurrentJNIMethod() {
    assert(dvmThreadSelf() != NULL);

    void* fp = dvmThreadSelf()->interpSave.curFrame;
    const Method* meth = SAVEAREA_FROM_FP(fp)->method;

    assert(meth != NULL);
    assert(dvmIsNativeMethod(meth));
    return meth;
}

static void trackMonitorEnter(Thread* self, Object* obj) {
    static const int kInitialSize = 16;
    ReferenceTable* refTable = &self->jniMonitorRefTable;

    if (refTable->table == NULL) {
        assert(refTable->maxEntries == 0);

        if (!dvmInitReferenceTable(refTable, kInitialSize, INT_MAX)) {
            ALOGE("Unable to initialize monitor tracking table");
            ReportJniError();
        }
    }

    if (!dvmAddToReferenceTable(refTable, obj)) {
        ALOGE("Unable to add entry to monitor tracking table");
        ReportJniError();
    } else {
        LOGVV("--- added monitor %p", obj);
    }
}

static void trackMonitorExit(Thread* self, Object* obj) {
    ReferenceTable* pRefTable = &self->jniMonitorRefTable;

    if (!dvmRemoveFromReferenceTable(pRefTable, pRefTable->table, obj)) {
        ALOGE("JNI monitor %p not found in tracking list", obj);
    } else {
        LOGVV("--- removed monitor %p", obj);
    }
}

void dvmReleaseJniMonitors(Thread* self) {
    ReferenceTable* pRefTable = &self->jniMonitorRefTable;
    Object** top = pRefTable->table;

    if (top == NULL) {
        return;
    }
    Object** ptr = pRefTable->nextEntry;
    while (--ptr >= top) {
        if (!dvmUnlockObject(self, *ptr)) {
            ALOGW("Unable to unlock monitor %p at thread detach", *ptr);
        } else {
            LOGVV("--- detach-releasing monitor %p", *ptr);
        }
    }

    pRefTable->nextEntry = pRefTable->table;
}

static bool canAllocClass(ClassObject* clazz) {
    if (dvmIsAbstractClass(clazz) || dvmIsInterfaceClass(clazz)) {
        dvmThrowInstantiationException(clazz, "abstract class or interface");
        return false;
    } else if (dvmIsArrayClass(clazz) || dvmIsTheClassClass(clazz)) {
        dvmThrowInstantiationException(clazz, "wrong JNI function");
        return false;
    }
    return true;
}

static inline void convertReferenceResult(JNIEnv* env, JValue* pResult,
    const Method* method, Thread* self)
{
    if (method->shorty[0] == 'L' && !dvmCheckException(self) && pResult->l != NULL) {
        pResult->l = dvmDecodeIndirectRef(self, (jobject) pResult->l);
    }
}

void dvmCallJNIMethod(const u4* args, JValue* pResult, const Method* method, Thread* self) {
    u4* modArgs = (u4*) args;
    jclass staticMethodClass = NULL;

    u4 accessFlags = method->accessFlags;
    bool isSynchronized = (accessFlags & ACC_SYNCHRONIZED) != 0;

    ALOGI("JNI calling %p (%s.%s:%s):", method->insns,
        method->clazz->descriptor, method->name, method->shorty);

    int idx = 0;
    Object* lockObj;
    if ((accessFlags & ACC_STATIC) != 0) {
        lockObj = (Object*) method->clazz;
        staticMethodClass = (jclass) addLocalReference(self, (Object*) method->clazz);
    } else {
        lockObj = (Object*) args[0];
        modArgs[idx++] = (u4) addLocalReference(self, (Object*) modArgs[0]);
    }

    if (!method->noRef) {
        const char* shorty = &method->shorty[1];        /* skip return type */
        while (*shorty != '\0') {
            switch (*shorty++) {
            case 'L':
                ALOGI("  local %d: 0x%08x", idx, modArgs[idx]);
                if (modArgs[idx] != 0) {
                    modArgs[idx] = (u4) addLocalReference(self, (Object*) modArgs[idx]);
                }
                break;
            case 'D':
            case 'J':
                idx++;
                break;
            default:
                break;
            }
            idx++;
        }
    }

    if (UNLIKELY(method->shouldTrace)) {
        logNativeMethodEntry(method, args);
    }
    if (UNLIKELY(isSynchronized)) {
        dvmLockObject(self, lockObj);
    }

    ThreadStatus oldStatus = dvmChangeStatus(self, THREAD_NATIVE);

    ANDROID_MEMBAR_FULL();      /* guarantee ordering on method->insns */
    assert(method->insns != NULL);

    JNIEnv* env = self->jniEnv;
    COMPUTE_STACK_SUM(self);
    dvmPlatformInvoke(env,
            (ClassObject*) staticMethodClass,
            method->jniArgInfo, method->insSize, modArgs, method->shorty,
            (void*) method->insns, pResult);
    CHECK_STACK_SUM(self);

    dvmChangeStatus(self, oldStatus);

    convertReferenceResult(env, pResult, method, self);

    if (UNLIKELY(isSynchronized)) {
        dvmUnlockObject(self, lockObj);
    }
    if (UNLIKELY(method->shouldTrace)) {
        logNativeMethodExit(method, self, *pResult);
    }
}

static jint GetVersion(JNIEnv* env) {
    ScopedJniThreadState ts(env);
    return JNI_VERSION_1_6;
}

static jclass DefineClass(JNIEnv* env, const char *name, jobject loader,
    const jbyte* buf, jsize bufLen)
{
    UNUSED_PARAMETER(name);
    UNUSED_PARAMETER(loader);
    UNUSED_PARAMETER(buf);
    UNUSED_PARAMETER(bufLen);

    ScopedJniThreadState ts(env);
    ALOGW("JNI DefineClass is not supported");
    return NULL;
}

static jclass FindClass(JNIEnv* env, const char* name) {
    ScopedJniThreadState ts(env);

    const Method* thisMethod = dvmGetCurrentJNIMethod();
    assert(thisMethod != NULL);

    Object* loader;
    Object* trackedLoader = NULL;
    if (ts.self()->classLoaderOverride != NULL) {
        assert(strcmp(thisMethod->name, "nativeLoad") == 0);
        loader = ts.self()->classLoaderOverride;
    } else if (thisMethod == gDvm.methDalvikSystemNativeStart_main ||
               thisMethod == gDvm.methDalvikSystemNativeStart_run) {
        if (!gDvm.initializing) {
            loader = trackedLoader = dvmGetSystemClassLoader();
        } else {
            loader = NULL;
        }
    } else {
        loader = thisMethod->clazz->classLoader;
    }

    char* descriptor = dvmNameToDescriptor(name);
    if (descriptor == NULL) {
        return NULL;
    }
    ClassObject* clazz = dvmFindClassNoInit(descriptor, loader);
    free(descriptor);

    jclass jclazz = (jclass) addLocalReference(ts.self(), (Object*) clazz);
    dvmReleaseTrackedAlloc(trackedLoader, ts.self());
    return jclazz;
}

static jclass GetSuperclass(JNIEnv* env, jclass jclazz) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    return (jclass) addLocalReference(ts.self(), (Object*)clazz->super);
}

static jboolean IsAssignableFrom(JNIEnv* env, jclass jclazz1, jclass jclazz2) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz1 = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz1);
    ClassObject* clazz2 = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz2);
    return dvmInstanceof(clazz1, clazz2);
}

static jmethodID FromReflectedMethod(JNIEnv* env, jobject jmethod) {
    ScopedJniThreadState ts(env);
    Object* method = dvmDecodeIndirectRef(ts.self(), jmethod);
    return (jmethodID) dvmGetMethodFromReflectObj(method);
}

static jfieldID FromReflectedField(JNIEnv* env, jobject jfield) {
    ScopedJniThreadState ts(env);
    Object* field = dvmDecodeIndirectRef(ts.self(), jfield);
    return (jfieldID) dvmGetFieldFromReflectObj(field);
}

static jobject ToReflectedMethod(JNIEnv* env, jclass jcls, jmethodID methodID, jboolean isStatic) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jcls);
    Object* obj = dvmCreateReflectObjForMethod(clazz, (Method*) methodID);
    dvmReleaseTrackedAlloc(obj, NULL);
    return addLocalReference(ts.self(), obj);
}

static jobject ToReflectedField(JNIEnv* env, jclass jcls, jfieldID fieldID, jboolean isStatic) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jcls);
    Object* obj = dvmCreateReflectObjForField(clazz, (Field*) fieldID);
    dvmReleaseTrackedAlloc(obj, NULL);
    return addLocalReference(ts.self(), obj);
}

static jint Throw(JNIEnv* env, jthrowable jobj) {
    ScopedJniThreadState ts(env);
    if (jobj != NULL) {
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        dvmSetException(ts.self(), obj);
        return JNI_OK;
    }
    return JNI_ERR;
}

static jint ThrowNew(JNIEnv* env, jclass jclazz, const char* message) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    dvmThrowException(clazz, message);
    return JNI_OK;
}

static jthrowable ExceptionOccurred(JNIEnv* env) {
    ScopedJniThreadState ts(env);
    Object* exception = dvmGetException(ts.self());
    jthrowable localException = (jthrowable) addLocalReference(ts.self(), exception);
    if (localException == NULL && exception != NULL) {
        ALOGW("JNI WARNING: addLocal/exception combo");
    }
    return localException;
}

static void ExceptionDescribe(JNIEnv* env) {
    ScopedJniThreadState ts(env);
    Object* exception = dvmGetException(ts.self());
    if (exception != NULL) {
        dvmPrintExceptionStackTrace();
    } else {
        ALOGI("Odd: ExceptionDescribe called, but no exception pending");
    }
}

static void ExceptionClear(JNIEnv* env) {
    ScopedJniThreadState ts(env);
    dvmClearException(ts.self());
}

static void FatalError(JNIEnv* env, const char* msg) {
    //dvmChangeStatus(NULL, THREAD_RUNNING);
    ALOGE("JNI posting fatal error: %s", msg);
    ReportJniError();
}

static jint PushLocalFrame(JNIEnv* env, jint capacity) {
    ScopedJniThreadState ts(env);
    if (!ensureLocalCapacity(ts.self(), capacity) ||
            !dvmPushLocalFrame(ts.self(), dvmGetCurrentJNIMethod()))
    {
        dvmClearException(ts.self());
        dvmThrowOutOfMemoryError("out of stack in JNI PushLocalFrame");
        return JNI_ERR;
    }
    return JNI_OK;
}

static jobject PopLocalFrame(JNIEnv* env, jobject jresult) {
    ScopedJniThreadState ts(env);
    Object* result = dvmDecodeIndirectRef(ts.self(), jresult);
    if (!dvmPopLocalFrame(ts.self())) {
        ALOGW("JNI WARNING: too many PopLocalFrame calls");
        dvmClearException(ts.self());
        dvmThrowRuntimeException("too many PopLocalFrame calls");
    }
    return addLocalReference(ts.self(), result);
}

static jobject NewGlobalRef(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    return addGlobalReference(obj);
}

static void DeleteGlobalRef(JNIEnv* env, jobject jglobalRef) {
    ScopedJniThreadState ts(env);
    deleteGlobalReference(jglobalRef);
}

static jobject NewLocalRef(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    return addLocalReference(ts.self(), obj);
}

static void DeleteLocalRef(JNIEnv* env, jobject jlocalRef) {
    ScopedJniThreadState ts(env);
    deleteLocalReference(ts.self(), jlocalRef);
}

static jint EnsureLocalCapacity(JNIEnv* env, jint capacity) {
    ScopedJniThreadState ts(env);
    bool okay = ensureLocalCapacity(ts.self(), capacity);
    if (!okay) {
        dvmThrowOutOfMemoryError("can't ensure local reference capacity");
    }
    return okay ? 0 : -1;
}

static jboolean IsSameObject(JNIEnv* env, jobject jref1, jobject jref2) {
    ScopedJniThreadState ts(env);
    Object* obj1 = dvmDecodeIndirectRef(ts.self(), jref1);
    Object* obj2 = dvmDecodeIndirectRef(ts.self(), jref2);
    return (obj1 == obj2);
}

static jobject AllocObject(JNIEnv* env, jclass jclazz) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    if (!canAllocClass(clazz) ||
        (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz)))
    {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    Object* newObj = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    return addLocalReference(ts.self(), newObj);
}

static jobject NewObject(JNIEnv* env, jclass jclazz, jmethodID methodID, ...) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);

    if (!canAllocClass(clazz) || (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz))) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    Object* newObj = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    jobject result = addLocalReference(ts.self(), newObj);
    if (newObj != NULL) {
        JValue unused;
        va_list args;
        va_start(args, methodID);
        dvmCallMethodV(ts.self(), (Method*) methodID, newObj, true, &unused, args);
        va_end(args);
    }
    return result;
}

static jobject NewObjectV(JNIEnv* env, jclass jclazz, jmethodID methodID, va_list args) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);

    if (!canAllocClass(clazz) || (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz))) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    Object* newObj = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    jobject result = addLocalReference(ts.self(), newObj);
    if (newObj != NULL) {
        JValue unused;
        dvmCallMethodV(ts.self(), (Method*) methodID, newObj, true, &unused, args);
    }
    return result;
}

static jobject NewObjectA(JNIEnv* env, jclass jclazz, jmethodID methodID, jvalue* args) {
    ScopedJniThreadState ts(env);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);

    if (!canAllocClass(clazz) || (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz))) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    Object* newObj = dvmAllocObject(clazz, ALLOC_DONT_TRACK);
    jobject result = addLocalReference(ts.self(), newObj);
    if (newObj != NULL) {
        JValue unused;
        dvmCallMethodA(ts.self(), (Method*) methodID, newObj, true, &unused, args);
    }
    return result;
}

static jclass GetObjectClass(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);

    assert(jobj != NULL);

    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    return (jclass) addLocalReference(ts.self(), (Object*) obj->clazz);
}

static jboolean IsInstanceOf(JNIEnv* env, jobject jobj, jclass jclazz) {
    ScopedJniThreadState ts(env);

    assert(jclazz != NULL);
    if (jobj == NULL) {
        return true;
    }

    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    return dvmInstanceof(obj->clazz, clazz);
}

static jmethodID GetMethodID(JNIEnv* env, jclass jclazz, const char* name, const char* sig) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    if (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz)) {
        assert(dvmCheckException(ts.self()));
    } else if (dvmIsInterfaceClass(clazz)) {
        Method* meth = dvmFindInterfaceMethodHierByDescriptor(clazz, name, sig);
        if (meth == NULL) {
            dvmThrowExceptionFmt(gDvm.exNoSuchMethodError,
                "no method with name='%s' signature='%s' in interface %s",
                name, sig, clazz->descriptor);
        }
        return (jmethodID) meth;
    }
    Method* meth = dvmFindVirtualMethodHierByDescriptor(clazz, name, sig);
    if (meth == NULL) {
        /* search private methods and constructors; non-hierarchical */
        meth = dvmFindDirectMethodByDescriptor(clazz, name, sig);
    }
    if (meth != NULL && dvmIsStaticMethod(meth)) {
        IF_ALOGD() {
            char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
            ALOGD("GetMethodID: not returning static method %s.%s %s",
                    clazz->descriptor, meth->name, desc);
            free(desc);
        }
        meth = NULL;
    }
    if (meth == NULL) {
        dvmThrowExceptionFmt(gDvm.exNoSuchMethodError,
                "no method with name='%s' signature='%s' in class %s",
                name, sig, clazz->descriptor);
    } else {
        assert(dvmIsClassInitialized(meth->clazz) || dvmIsClassInitializing(meth->clazz));
    }
    return (jmethodID) meth;
}

static jfieldID GetFieldID(JNIEnv* env, jclass jclazz, const char* name, const char* sig) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);

    if (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz)) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    jfieldID id = (jfieldID) dvmFindInstanceFieldHier(clazz, name, sig);
    if (id == NULL) {
        dvmThrowExceptionFmt(gDvm.exNoSuchFieldError,
                "no field with name='%s' signature='%s' in class %s",
                name, sig, clazz->descriptor);
    }
    return id;
}

static jmethodID GetStaticMethodID(JNIEnv* env, jclass jclazz, const char* name, const char* sig) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    if (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz)) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    Method* meth = dvmFindDirectMethodHierByDescriptor(clazz, name, sig);

    if (meth != NULL && !dvmIsStaticMethod(meth)) {
        IF_ALOGD() {
            char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
            ALOGD("GetStaticMethodID: not returning nonstatic method %s.%s %s",
                    clazz->descriptor, meth->name, desc);
            free(desc);
        }
        meth = NULL;
    }

    jmethodID id = (jmethodID) meth;
    if (id == NULL) {
        dvmThrowExceptionFmt(gDvm.exNoSuchMethodError,
                "no static method with name='%s' signature='%s' in class %s",
                name, sig, clazz->descriptor);
    }
    return id;
}

static jfieldID GetStaticFieldID(JNIEnv* env, jclass jclazz, const char* name, const char* sig) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    if (!dvmIsClassInitialized(clazz) && !dvmInitClass(clazz)) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }

    jfieldID id = (jfieldID) dvmFindStaticFieldHier(clazz, name, sig);
    if (id == NULL) {
        dvmThrowExceptionFmt(gDvm.exNoSuchFieldError,
                "no static field with name='%s' signature='%s' in class %s",
                name, sig, clazz->descriptor);
    }
    return id;
}

#define GET_STATIC_TYPE_FIELD(_ctype, _jname, _isref)
    static _ctype GetStatic##_jname##Field(JNIEnv* env, jclass jclazz,
        jfieldID fieldID)
    {
        UNUSED_PARAMETER(jclazz);
        ScopedJniThreadState ts(env);
        StaticField* sfield = (StaticField*) fieldID;
        _ctype value;
        if (dvmIsVolatileField(sfield)) {
            if (_isref) {
                Object* obj = dvmGetStaticFieldObjectVolatile(sfield);
                value = (_ctype)(u4)addLocalReference(ts.self(), obj);
            } else {
                value = (_ctype) dvmGetStaticField##_jname##Volatile(sfield);
            }
        } else {
            if (_isref) {
                Object* obj = dvmGetStaticFieldObject(sfield);
                value = (_ctype)(u4)addLocalReference(ts.self(), obj);
            } else {
                value = (_ctype) dvmGetStaticField##_jname(sfield);
            }
        }
        return value;
    }
GET_STATIC_TYPE_FIELD(jobject, Object, true);
GET_STATIC_TYPE_FIELD(jboolean, Boolean, false);
GET_STATIC_TYPE_FIELD(jbyte, Byte, false);
GET_STATIC_TYPE_FIELD(jchar, Char, false);
GET_STATIC_TYPE_FIELD(jshort, Short, false);
GET_STATIC_TYPE_FIELD(jint, Int, false);
GET_STATIC_TYPE_FIELD(jlong, Long, false);
GET_STATIC_TYPE_FIELD(jfloat, Float, false);
GET_STATIC_TYPE_FIELD(jdouble, Double, false);

#define SET_STATIC_TYPE_FIELD(_ctype, _ctype2, _jname, _isref)
    static void SetStatic##_jname##Field(JNIEnv* env, jclass jclazz,
        jfieldID fieldID, _ctype value)
    {
        UNUSED_PARAMETER(jclazz);
        ScopedJniThreadState ts(env);
        StaticField* sfield = (StaticField*) fieldID;
        if (dvmIsVolatileField(sfield)) {
            if (_isref) {
                Object* valObj = dvmDecodeIndirectRef(ts.self(), (jobject)(u4)value);
                dvmSetStaticFieldObjectVolatile(sfield, valObj);
            } else {
                dvmSetStaticField##_jname##Volatile(sfield, (_ctype2)value);
            }
        } else {
            if (_isref) {
                Object* valObj = dvmDecodeIndirectRef(ts.self(), (jobject)(u4)value);
                dvmSetStaticFieldObject(sfield, valObj);
            } else {
                dvmSetStaticField##_jname(sfield, (_ctype2)value);
            }
        }
    }
SET_STATIC_TYPE_FIELD(jobject, Object*, Object, true);
SET_STATIC_TYPE_FIELD(jboolean, bool, Boolean, false);
SET_STATIC_TYPE_FIELD(jbyte, s1, Byte, false);
SET_STATIC_TYPE_FIELD(jchar, u2, Char, false);
SET_STATIC_TYPE_FIELD(jshort, s2, Short, false);
SET_STATIC_TYPE_FIELD(jint, s4, Int, false);
SET_STATIC_TYPE_FIELD(jlong, s8, Long, false);
SET_STATIC_TYPE_FIELD(jfloat, float, Float, false);
SET_STATIC_TYPE_FIELD(jdouble, double, Double, false);

#define GET_TYPE_FIELD(_ctype, _jname, _isref)
    static _ctype Get##_jname##Field(JNIEnv* env, jobject jobj,
        jfieldID fieldID)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        InstField* field = (InstField*) fieldID;
        _ctype value;
        if (dvmIsVolatileField(field)) {
            if (_isref) {
                Object* valObj =
                    dvmGetFieldObjectVolatile(obj, field->byteOffset);
                value = (_ctype)(u4)addLocalReference(ts.self(), valObj);
            } else {
                value = (_ctype)
                    dvmGetField##_jname##Volatile(obj, field->byteOffset);
            }
        } else {
            if (_isref) {
                Object* valObj = dvmGetFieldObject(obj, field->byteOffset);
                value = (_ctype)(u4)addLocalReference(ts.self(), valObj);
            } else {
                value = (_ctype) dvmGetField##_jname(obj, field->byteOffset);
            }
        }
        return value;
    }
GET_TYPE_FIELD(jobject, Object, true);
GET_TYPE_FIELD(jboolean, Boolean, false);
GET_TYPE_FIELD(jbyte, Byte, false);
GET_TYPE_FIELD(jchar, Char, false);
GET_TYPE_FIELD(jshort, Short, false);
GET_TYPE_FIELD(jint, Int, false);
GET_TYPE_FIELD(jlong, Long, false);
GET_TYPE_FIELD(jfloat, Float, false);
GET_TYPE_FIELD(jdouble, Double, false);

#define SET_TYPE_FIELD(_ctype, _ctype2, _jname, _isref)
    static void Set##_jname##Field(JNIEnv* env, jobject jobj,
        jfieldID fieldID, _ctype value)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        InstField* field = (InstField*) fieldID;
        if (dvmIsVolatileField(field)) {
            if (_isref) {
                Object* valObj = dvmDecodeIndirectRef(ts.self(), (jobject)(u4)value);
                dvmSetFieldObjectVolatile(obj, field->byteOffset, valObj);
            } else {
                dvmSetField##_jname##Volatile(obj,
                    field->byteOffset, (_ctype2)value);
            }
        } else {
            if (_isref) {
                Object* valObj = dvmDecodeIndirectRef(ts.self(), (jobject)(u4)value);
                dvmSetFieldObject(obj, field->byteOffset, valObj);
            } else {
                dvmSetField##_jname(obj,
                    field->byteOffset, (_ctype2)value);
            }
        }
    }
SET_TYPE_FIELD(jobject, Object*, Object, true);
SET_TYPE_FIELD(jboolean, bool, Boolean, false);
SET_TYPE_FIELD(jbyte, s1, Byte, false);
SET_TYPE_FIELD(jchar, u2, Char, false);
SET_TYPE_FIELD(jshort, s2, Short, false);
SET_TYPE_FIELD(jint, s4, Int, false);
SET_TYPE_FIELD(jlong, s8, Long, false);
SET_TYPE_FIELD(jfloat, float, Float, false);
SET_TYPE_FIELD(jdouble, double, Double, false);

#define CALL_VIRTUAL(_ctype, _jname, _retfail, _retok, _isref)
    static _ctype Call##_jname##Method(JNIEnv* env, jobject jobj,
        jmethodID methodID, ...)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        const Method* meth;
        va_list args;
        JValue result;
        meth = dvmGetVirtualizedMethod(obj->clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        va_start(args, methodID);
        dvmCallMethodV(ts.self(), meth, obj, true, &result, args);
        va_end(args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
    static _ctype Call##_jname##MethodV(JNIEnv* env, jobject jobj,
        jmethodID methodID, va_list args)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        const Method* meth;
        JValue result;
        meth = dvmGetVirtualizedMethod(obj->clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        dvmCallMethodV(ts.self(), meth, obj, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
    static _ctype Call##_jname##MethodA(JNIEnv* env, jobject jobj,
        jmethodID methodID, jvalue* args)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        const Method* meth;
        JValue result;
        meth = dvmGetVirtualizedMethod(obj->clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        dvmCallMethodA(ts.self(), meth, obj, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
CALL_VIRTUAL(jobject, Object, NULL, (jobject) result.l, true);
CALL_VIRTUAL(jboolean, Boolean, 0, result.z, false);
CALL_VIRTUAL(jbyte, Byte, 0, result.b, false);
CALL_VIRTUAL(jchar, Char, 0, result.c, false);
CALL_VIRTUAL(jshort, Short, 0, result.s, false);
CALL_VIRTUAL(jint, Int, 0, result.i, false);
CALL_VIRTUAL(jlong, Long, 0, result.j, false);
CALL_VIRTUAL(jfloat, Float, 0.0f, result.f, false);
CALL_VIRTUAL(jdouble, Double, 0.0, result.d, false);
CALL_VIRTUAL(void, Void, , , false);

#define CALL_NONVIRTUAL(_ctype, _jname, _retfail, _retok, _isref)
    static _ctype CallNonvirtual##_jname##Method(JNIEnv* env, jobject jobj,
        jclass jclazz, jmethodID methodID, ...)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
        const Method* meth;
        va_list args;
        JValue result;
        meth = dvmGetVirtualizedMethod(clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        va_start(args, methodID);
        dvmCallMethodV(ts.self(), meth, obj, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        va_end(args);
        return _retok;
    }
    static _ctype CallNonvirtual##_jname##MethodV(JNIEnv* env, jobject jobj,
        jclass jclazz, jmethodID methodID, va_list args)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
        const Method* meth;
        JValue result;
        meth = dvmGetVirtualizedMethod(clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        dvmCallMethodV(ts.self(), meth, obj, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
    static _ctype CallNonvirtual##_jname##MethodA(JNIEnv* env, jobject jobj,
        jclass jclazz, jmethodID methodID, jvalue* args)
    {
        ScopedJniThreadState ts(env);
        Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
        ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
        const Method* meth;
        JValue result;
        meth = dvmGetVirtualizedMethod(clazz, (Method*)methodID);
        if (meth == NULL) {
            return _retfail;
        }
        dvmCallMethodA(ts.self(), meth, obj, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
CALL_NONVIRTUAL(jobject, Object, NULL, (jobject) result.l, true);
CALL_NONVIRTUAL(jboolean, Boolean, 0, result.z, false);
CALL_NONVIRTUAL(jbyte, Byte, 0, result.b, false);
CALL_NONVIRTUAL(jchar, Char, 0, result.c, false);
CALL_NONVIRTUAL(jshort, Short, 0, result.s, false);
CALL_NONVIRTUAL(jint, Int, 0, result.i, false);
CALL_NONVIRTUAL(jlong, Long, 0, result.j, false);
CALL_NONVIRTUAL(jfloat, Float, 0.0f, result.f, false);
CALL_NONVIRTUAL(jdouble, Double, 0.0, result.d, false);
CALL_NONVIRTUAL(void, Void, , , false);

#define CALL_STATIC(_ctype, _jname, _retfail, _retok, _isref)
    static _ctype CallStatic##_jname##Method(JNIEnv* env, jclass jclazz,
        jmethodID methodID, ...)
    {
        UNUSED_PARAMETER(jclazz);
        ScopedJniThreadState ts(env);
        JValue result;
        va_list args;
        va_start(args, methodID);
        dvmCallMethodV(ts.self(), (Method*)methodID, NULL, true, &result, args);
        va_end(args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
    static _ctype CallStatic##_jname##MethodV(JNIEnv* env, jclass jclazz,
        jmethodID methodID, va_list args)
    {
        UNUSED_PARAMETER(jclazz);
        ScopedJniThreadState ts(env);
        JValue result;
        dvmCallMethodV(ts.self(), (Method*)methodID, NULL, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
    static _ctype CallStatic##_jname##MethodA(JNIEnv* env, jclass jclazz,
        jmethodID methodID, jvalue* args)
    {
        UNUSED_PARAMETER(jclazz);
        ScopedJniThreadState ts(env);
        JValue result;
        dvmCallMethodA(ts.self(), (Method*)methodID, NULL, true, &result, args);
        if (_isref && !dvmCheckException(ts.self()))
            result.l = (Object*)addLocalReference(ts.self(), result.l);
        return _retok;
    }
CALL_STATIC(jobject, Object, NULL, (jobject) result.l, true);
CALL_STATIC(jboolean, Boolean, 0, result.z, false);
CALL_STATIC(jbyte, Byte, 0, result.b, false);
CALL_STATIC(jchar, Char, 0, result.c, false);
CALL_STATIC(jshort, Short, 0, result.s, false);
CALL_STATIC(jint, Int, 0, result.i, false);
CALL_STATIC(jlong, Long, 0, result.j, false);
CALL_STATIC(jfloat, Float, 0.0f, result.f, false);
CALL_STATIC(jdouble, Double, 0.0, result.d, false);
CALL_STATIC(void, Void, , , false);

static jstring NewString(JNIEnv* env, const jchar* unicodeChars, jsize len) {
    ScopedJniThreadState ts(env);
    StringObject* jstr = dvmCreateStringFromUnicode(unicodeChars, len);
    if (jstr == NULL) {
        return NULL;
    }
    dvmReleaseTrackedAlloc((Object*) jstr, NULL);
    return (jstring) addLocalReference(ts.self(), (Object*) jstr);
}

static jsize GetStringLength(JNIEnv* env, jstring jstr) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    return strObj->length();
}

static const jchar* GetStringChars(JNIEnv* env, jstring jstr, jboolean* isCopy) {
    ScopedJniThreadState ts(env);

    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    ArrayObject* strChars = strObj->array();

    pinPrimitiveArray(strChars);

    const u2* data = strObj->chars();
    if (isCopy != NULL) {
        *isCopy = JNI_FALSE;
    }
    return (jchar*) data;
}

static void ReleaseStringChars(JNIEnv* env, jstring jstr, const jchar* chars) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    ArrayObject* strChars = strObj->array();
    unpinPrimitiveArray(strChars);
}

static jstring NewStringUTF(JNIEnv* env, const char* bytes) {
    ScopedJniThreadState ts(env);
    if (bytes == NULL) {
        return NULL;
    }
    StringObject* newStr = dvmCreateStringFromCstr(bytes);
    jstring result = (jstring) addLocalReference(ts.self(), (Object*) newStr);
    dvmReleaseTrackedAlloc((Object*)newStr, NULL);
    return result;
}

static jsize GetStringUTFLength(JNIEnv* env, jstring jstr) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    if (strObj == NULL) {
        return 0; // Should we throw something or assert?
    }
    return strObj->utfLength();
}
static const char* GetStringUTFChars(JNIEnv* env, jstring jstr, jboolean* isCopy) {
    ScopedJniThreadState ts(env);
    if (jstr == NULL) {
        return NULL;
    }
    if (isCopy != NULL) {
        *isCopy = JNI_TRUE;
    }
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    char* newStr = dvmCreateCstrFromString(strObj);
    if (newStr == NULL) {
        dvmThrowOutOfMemoryError("native heap string alloc failed");
    }
    return newStr;
}

static void ReleaseStringUTFChars(JNIEnv* env, jstring jstr, const char* utf) {
    ScopedJniThreadState ts(env);
    free((char*) utf);
}

static jsize GetArrayLength(JNIEnv* env, jarray jarr) {
    ScopedJniThreadState ts(env);
    ArrayObject* arrObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
    return arrObj->length;
}

static jobjectArray NewObjectArray(JNIEnv* env, jsize length,
    jclass jelementClass, jobject jinitialElement)
{
    ScopedJniThreadState ts(env);

    if (jelementClass == NULL) {
        dvmThrowNullPointerException("JNI NewObjectArray elementClass == NULL");
        return NULL;
    }

    ClassObject* elemClassObj = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jelementClass);
    ClassObject* arrayClass = dvmFindArrayClassForElement(elemClassObj);
    ArrayObject* newObj = dvmAllocArrayByClass(arrayClass, length, ALLOC_DEFAULT);
    if (newObj == NULL) {
        assert(dvmCheckException(ts.self()));
        return NULL;
    }
    jobjectArray newArray = (jobjectArray) addLocalReference(ts.self(), (Object*) newObj);
    dvmReleaseTrackedAlloc((Object*) newObj, NULL);

    if (jinitialElement != NULL) {
        Object* initialElement = dvmDecodeIndirectRef(ts.self(), jinitialElement);
        Object** arrayData = (Object**) (void*) newObj->contents;
        for (jsize i = 0; i < length; ++i) {
            arrayData[i] = initialElement;
        }
    }

    return newArray;
}

static bool checkArrayElementBounds(ArrayObject* arrayObj, jsize index) {
    assert(arrayObj != NULL);
    if (index < 0 || index >= (int) arrayObj->length) {
        dvmThrowArrayIndexOutOfBoundsException(arrayObj->length, index);
        return false;
    }
    return true;
}

static jobject GetObjectArrayElement(JNIEnv* env, jobjectArray jarr, jsize index) {
    ScopedJniThreadState ts(env);

    ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
    if (!checkArrayElementBounds(arrayObj, index)) {
        return NULL;
    }

    Object* value = ((Object**) (void*) arrayObj->contents)[index];
    return addLocalReference(ts.self(), value);
}

static void SetObjectArrayElement(JNIEnv* env, jobjectArray jarr, jsize index, jobject jobj) {
    ScopedJniThreadState ts(env);

    ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
    if (!checkArrayElementBounds(arrayObj, index)) {
        return;
    }

    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);

    if (obj != NULL && !dvmCanPutArrayElement(obj->clazz, arrayObj->clazz)) {
      ALOGV("Can't put a '%s'(%p) into array type='%s'(%p)",
            obj->clazz->descriptor, obj,
            arrayObj->clazz->descriptor, arrayObj);
      dvmThrowArrayStoreExceptionIncompatibleElement(obj->clazz, arrayObj->clazz);
      return;
    }

    ALOGV("JNI: set element %d in array %p to %p", index, array, value);

    dvmSetObjectArrayElement(arrayObj, index, obj);
}

#define NEW_PRIMITIVE_ARRAY(_artype, _jname, _typechar)
    static _artype New##_jname##Array(JNIEnv* env, jsize length) {
        ScopedJniThreadState ts(env);
        ArrayObject* arrayObj = dvmAllocPrimitiveArray(_typechar, length, ALLOC_DEFAULT);
        if (arrayObj == NULL) {
            return NULL;
        }
        _artype result = (_artype) addLocalReference(ts.self(), (Object*) arrayObj);
        dvmReleaseTrackedAlloc((Object*) arrayObj, NULL);
        return result;
    }
NEW_PRIMITIVE_ARRAY(jbooleanArray, Boolean, 'Z');
NEW_PRIMITIVE_ARRAY(jbyteArray, Byte, 'B');
NEW_PRIMITIVE_ARRAY(jcharArray, Char, 'C');
NEW_PRIMITIVE_ARRAY(jshortArray, Short, 'S');
NEW_PRIMITIVE_ARRAY(jintArray, Int, 'I');
NEW_PRIMITIVE_ARRAY(jlongArray, Long, 'J');
NEW_PRIMITIVE_ARRAY(jfloatArray, Float, 'F');
NEW_PRIMITIVE_ARRAY(jdoubleArray, Double, 'D');

#define GET_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname)
    static _ctype* Get##_jname##ArrayElements(JNIEnv* env,
        _ctype##Array jarr, jboolean* isCopy)
    {
        ScopedJniThreadState ts(env);
        ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
        pinPrimitiveArray(arrayObj);
        _ctype* data = (_ctype*) (void*) arrayObj->contents;
        if (isCopy != NULL) {
            *isCopy = JNI_FALSE;
        }
        return data;
    }

#define RELEASE_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname)
    static void Release##_jname##ArrayElements(JNIEnv* env,
        _ctype##Array jarr, _ctype* elems, jint mode)
    {
        UNUSED_PARAMETER(elems);
        if (mode != JNI_COMMIT) {
            ScopedJniThreadState ts(env);
            ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
            unpinPrimitiveArray(arrayObj);
        }
    }

static void throwArrayRegionOutOfBounds(ArrayObject* arrayObj, jsize start,
    jsize len, const char* arrayIdentifier)
{
    dvmThrowExceptionFmt(gDvm.exArrayIndexOutOfBoundsException,
        "%s offset=%d length=%d %s.length=%d",
        arrayObj->clazz->descriptor, start, len, arrayIdentifier,
        arrayObj->length);
}

#define GET_PRIMITIVE_ARRAY_REGION(_ctype, _jname)
    static void Get##_jname##ArrayRegion(JNIEnv* env,
        _ctype##Array jarr, jsize start, jsize len, _ctype* buf)
    {
        ScopedJniThreadState ts(env);
        ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
        _ctype* data = (_ctype*) (void*) arrayObj->contents;
        if (start < 0 || len < 0 || start + len > (int) arrayObj->length) {
            throwArrayRegionOutOfBounds(arrayObj, start, len, "src");
        } else {
            memcpy(buf, data + start, len * sizeof(_ctype));
        }
    }

#define SET_PRIMITIVE_ARRAY_REGION(_ctype, _jname)
    static void Set##_jname##ArrayRegion(JNIEnv* env,
        _ctype##Array jarr, jsize start, jsize len, const _ctype* buf)
    {
        ScopedJniThreadState ts(env);
        ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
        _ctype* data = (_ctype*) (void*) arrayObj->contents;
        if (start < 0 || len < 0 || start + len > (int) arrayObj->length) {
            throwArrayRegionOutOfBounds(arrayObj, start, len, "dst");
        } else {
            memcpy(data + start, buf, len * sizeof(_ctype));
        }
    }

#define PRIMITIVE_ARRAY_FUNCTIONS(_ctype, _jname)
    GET_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname);
    RELEASE_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname);
    GET_PRIMITIVE_ARRAY_REGION(_ctype, _jname);
    SET_PRIMITIVE_ARRAY_REGION(_ctype, _jname);

PRIMITIVE_ARRAY_FUNCTIONS(jboolean, Boolean);
PRIMITIVE_ARRAY_FUNCTIONS(jbyte, Byte);
PRIMITIVE_ARRAY_FUNCTIONS(jchar, Char);
PRIMITIVE_ARRAY_FUNCTIONS(jshort, Short);
PRIMITIVE_ARRAY_FUNCTIONS(jint, Int);
PRIMITIVE_ARRAY_FUNCTIONS(jlong, Long);
PRIMITIVE_ARRAY_FUNCTIONS(jfloat, Float);
PRIMITIVE_ARRAY_FUNCTIONS(jdouble, Double);

static jint RegisterNatives(JNIEnv* env, jclass jclazz,
    const JNINativeMethod* methods, jint nMethods)
{
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);

    if (gDvm.verboseJni) {
        ALOGI("[Registering JNI native methods for class %s]",
            clazz->descriptor);
    }

    for (int i = 0; i < nMethods; i++) {
        if (!dvmRegisterJNIMethod(clazz, methods[i].name,
                methods[i].signature, methods[i].fnPtr))
        {
            return JNI_ERR;
        }
    }
    return JNI_OK;
}

static jint UnregisterNatives(JNIEnv* env, jclass jclazz) {
    ScopedJniThreadState ts(env);

    ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(ts.self(), jclazz);
    if (gDvm.verboseJni) {
        ALOGI("[Unregistering JNI native methods for class %s]",
            clazz->descriptor);
    }
    dvmUnregisterJNINativeMethods(clazz);
    return JNI_OK;
}

static jint MonitorEnter(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    dvmLockObject(ts.self(), obj);
    trackMonitorEnter(ts.self(), obj);
    return JNI_OK;
}

static jint MonitorExit(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    Object* obj = dvmDecodeIndirectRef(ts.self(), jobj);
    bool success = dvmUnlockObject(ts.self(), obj);
    if (success) {
        trackMonitorExit(ts.self(), obj);
    }
    return success ? JNI_OK : JNI_ERR;
}

static jint GetJavaVM(JNIEnv* env, JavaVM** vm) {
    ScopedJniThreadState ts(env);
    *vm = gDvmJni.jniVm;
    return (*vm == NULL) ? JNI_ERR : JNI_OK;
}

static void GetStringRegion(JNIEnv* env, jstring jstr, jsize start, jsize len, jchar* buf) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    int strLen = strObj->length();
    if (((start|len) < 0) || (start + len > strLen)) {
        dvmThrowStringIndexOutOfBoundsExceptionWithRegion(strLen, start, len);
        return;
    }
    memcpy(buf, strObj->chars() + start, len * sizeof(u2));
}

static void GetStringUTFRegion(JNIEnv* env, jstring jstr, jsize start, jsize len, char* buf) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    int strLen = strObj->length();
    if (((start|len) < 0) || (start + len > strLen)) {
        dvmThrowStringIndexOutOfBoundsExceptionWithRegion(strLen, start, len);
        return;
    }
    dvmGetStringUtfRegion(strObj, start, len, buf);
}

static void* GetPrimitiveArrayCritical(JNIEnv* env, jarray jarr, jboolean* isCopy) {
    ScopedJniThreadState ts(env);
    ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
    pinPrimitiveArray(arrayObj);
    void* data = arrayObj->contents;
    if (UNLIKELY(isCopy != NULL)) {
        *isCopy = JNI_FALSE;
    }
    return data;
}

static void ReleasePrimitiveArrayCritical(JNIEnv* env, jarray jarr, void* carray, jint mode) {
    if (mode != JNI_COMMIT) {
        ScopedJniThreadState ts(env);
        ArrayObject* arrayObj = (ArrayObject*) dvmDecodeIndirectRef(ts.self(), jarr);
        unpinPrimitiveArray(arrayObj);
    }
}

static const jchar* GetStringCritical(JNIEnv* env, jstring jstr, jboolean* isCopy) {
    ScopedJniThreadState ts(env);

    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    ArrayObject* strChars = strObj->array();

    pinPrimitiveArray(strChars);

    const u2* data = strObj->chars();
    if (isCopy != NULL) {
        *isCopy = JNI_FALSE;
    }
    return (jchar*) data;
}

static void ReleaseStringCritical(JNIEnv* env, jstring jstr, const jchar* carray) {
    ScopedJniThreadState ts(env);
    StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(ts.self(), jstr);
    ArrayObject* strChars = strObj->array();
    unpinPrimitiveArray(strChars);
}

static jweak NewWeakGlobalRef(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    Object *obj = dvmDecodeIndirectRef(ts.self(), jobj);
    return (jweak) addWeakGlobalReference(obj);
}

static void DeleteWeakGlobalRef(JNIEnv* env, jweak wref) {
    ScopedJniThreadState ts(env);
    deleteWeakGlobalReference(wref);
}

static jboolean ExceptionCheck(JNIEnv* env) {
    ScopedJniThreadState ts(env);
    return dvmCheckException(ts.self());
}

static jobjectRefType GetObjectRefType(JNIEnv* env, jobject jobj) {
    ScopedJniThreadState ts(env);
    return dvmGetJNIRefType(ts.self(), jobj);
}

static jobject NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity) {
    ScopedJniThreadState ts(env);

    if (capacity < 0) {
        ALOGE("JNI ERROR (app bug): negative buffer capacity: %lld", capacity);
        ReportJniError();
    }
    if (address == NULL && capacity != 0) {
        ALOGE("JNI ERROR (app bug): non-zero capacity for NULL pointer: %lld", capacity);
        ReportJniError();
    }

    ClassObject* bufferClazz = gDvm.classJavaNioDirectByteBuffer;
    if (!dvmIsClassInitialized(bufferClazz) && !dvmInitClass(bufferClazz)) {
        return NULL;
    }
    Object* newObj = dvmAllocObject(bufferClazz, ALLOC_DONT_TRACK);
    if (newObj == NULL) {
        return NULL;
    }
    jobject result = addLocalReference(ts.self(), newObj);
    JValue unused;
    dvmCallMethod(ts.self(), gDvm.methJavaNioDirectByteBuffer_init,
            newObj, &unused, (jlong) address, (jint) capacity);
    if (dvmGetException(ts.self()) != NULL) {
        deleteLocalReference(ts.self(), result);
        return NULL;
    }
    return result;
}

static void* GetDirectBufferAddress(JNIEnv* env, jobject jbuf) {
    ScopedJniThreadState ts(env);

    Object* bufObj = dvmDecodeIndirectRef(ts.self(), jbuf);
    return (void*) dvmGetFieldLong(bufObj, gDvm.offJavaNioBuffer_effectiveDirectAddress);
}

static jlong GetDirectBufferCapacity(JNIEnv* env, jobject jbuf) {
    ScopedJniThreadState ts(env);

    Object* buf = dvmDecodeIndirectRef(ts.self(), jbuf);
    return dvmGetFieldInt(buf, gDvm.offJavaNioBuffer_capacity);
}

static jint attachThread(JavaVM* vm, JNIEnv** p_env, void* thr_args, bool isDaemon) {
    JavaVMAttachArgs* args = (JavaVMAttachArgs*) thr_args;

    Thread* self = dvmThreadSelf();
    if (self != NULL) {
        *p_env = self->jniEnv;
        return JNI_OK;
    }

    if (gDvm.zygote) {
        return JNI_ERR;
    }

    dvmLockThreadList(NULL);
    if (gDvm.nonDaemonThreadCount == 0) {
        // dead or dying
        ALOGV("Refusing to attach thread '%s' -- VM is shutting down",
            (thr_args == NULL) ? "(unknown)" : args->name);
        dvmUnlockThreadList();
        return JNI_ERR;
    }
    gDvm.nonDaemonThreadCount++;
    dvmUnlockThreadList();

    JavaVMAttachArgs argsCopy;
    if (args == NULL) {
        argsCopy.version = JNI_VERSION_1_2;
        argsCopy.name = NULL;
        argsCopy.group = (jobject) dvmGetMainThreadGroup();
    } else {
        if (dvmIsBadJniVersion(args->version)) {
            ALOGE("Bad JNI version passed to %s: %d",
                  (isDaemon ? "AttachCurrentThreadAsDaemon" : "AttachCurrentThread"),
                  args->version);
            return JNI_EVERSION;
        }

        argsCopy.version = args->version;
        argsCopy.name = args->name;
        if (args->group != NULL) {
            argsCopy.group = (jobject) dvmDecodeIndirectRef(NULL, args->group);
        } else {
            argsCopy.group = (jobject) dvmGetMainThreadGroup();
        }
    }

    bool result = dvmAttachCurrentThread(&argsCopy, isDaemon);

    /* restore the count */
    dvmLockThreadList(NULL);
    gDvm.nonDaemonThreadCount--;
    dvmUnlockThreadList();

    if (result) {
        self = dvmThreadSelf();
        assert(self != NULL);
        dvmChangeStatus(self, THREAD_NATIVE);
        *p_env = self->jniEnv;
        return JNI_OK;
    } else {
        return JNI_ERR;
    }
}

static jint AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    return attachThread(vm, p_env, thr_args, false);
}

static jint AttachCurrentThreadAsDaemon(JavaVM* vm, JNIEnv** p_env, void* thr_args)
{
    return attachThread(vm, p_env, thr_args, true);
}

static jint DetachCurrentThread(JavaVM* vm) {
    Thread* self = dvmThreadSelf();
    if (self == NULL) {
        return JNI_ERR;
    }

    dvmChangeStatus(self, THREAD_RUNNING);

    dvmDetachCurrentThread();

    return JNI_OK;
}

static jint GetEnv(JavaVM* vm, void** env, jint version) {
    Thread* self = dvmThreadSelf();

    if (dvmIsBadJniVersion(version) && version != JNI_VERSION_1_1) {
        ALOGE("Bad JNI version passed to GetEnv: %d", version);
        return JNI_EVERSION;
    }

    if (self == NULL) {
        *env = NULL;
    } else {
        dvmChangeStatus(self, THREAD_RUNNING);
        *env = (void*) dvmGetThreadJNIEnv(self);
        dvmChangeStatus(self, THREAD_NATIVE);
    }
    return (*env != NULL) ? JNI_OK : JNI_EDETACHED;
}

static jint DestroyJavaVM(JavaVM* vm) {
    JavaVMExt* ext = (JavaVMExt*) vm;
    if (ext == NULL) {
        return JNI_ERR;
    }

    if (gDvm.verboseShutdown) {
        ALOGD("DestroyJavaVM waiting for non-daemon threads to exit");
    }

    Thread* self = dvmThreadSelf();
    if (self == NULL) {
        JNIEnv* tmpEnv;
        if (AttachCurrentThread(vm, &tmpEnv, NULL) != JNI_OK) {
            ALOGV("Unable to reattach main for Destroy; assuming VM is shutting down (count=%d)",
                gDvm.nonDaemonThreadCount);
            goto shutdown;
        } else {
            ALOGV("Attached to wait for shutdown in Destroy");
        }
    }
    dvmChangeStatus(self, THREAD_VMWAIT);

    dvmLockThreadList(self);
    gDvm.nonDaemonThreadCount--;

    while (gDvm.nonDaemonThreadCount > 0) {
        pthread_cond_wait(&gDvm.vmExitCond, &gDvm.threadListLock);
    }

    dvmUnlockThreadList();
    self = NULL;

shutdown:

    if (gDvm.verboseShutdown) {
        ALOGD("DestroyJavaVM shutting VM down");
    }
    dvmShutdown();

    free(ext->envList);
    free(ext);

    return JNI_OK;
}

static const struct JNINativeInterface gNativeInterface = {
    NULL,
    NULL,
    NULL,
    NULL,

    GetVersion,

    DefineClass,
    FindClass,

    FromReflectedMethod,
    FromReflectedField,
    ToReflectedMethod,

    GetSuperclass,
    IsAssignableFrom,

    ToReflectedField,

    Throw,
    ThrowNew,
    ExceptionOccurred,
    ExceptionDescribe,
    ExceptionClear,
    FatalError,

    PushLocalFrame,
    PopLocalFrame,

    NewGlobalRef,
    DeleteGlobalRef,
    DeleteLocalRef,
    IsSameObject,
    NewLocalRef,
    EnsureLocalCapacity,

    AllocObject,
    NewObject,
    NewObjectV,
    NewObjectA,

    GetObjectClass,
    IsInstanceOf,

    GetMethodID,

    CallObjectMethod,
    CallObjectMethodV,
    CallObjectMethodA,
    CallBooleanMethod,
    CallBooleanMethodV,
    CallBooleanMethodA,
    CallByteMethod,
    CallByteMethodV,
    CallByteMethodA,
    CallCharMethod,
    CallCharMethodV,
    CallCharMethodA,
    CallShortMethod,
    CallShortMethodV,
    CallShortMethodA,
    CallIntMethod,
    CallIntMethodV,
    CallIntMethodA,
    CallLongMethod,
    CallLongMethodV,
    CallLongMethodA,
    CallFloatMethod,
    CallFloatMethodV,
    CallFloatMethodA,
    CallDoubleMethod,
    CallDoubleMethodV,
    CallDoubleMethodA,
    CallVoidMethod,
    CallVoidMethodV,
    CallVoidMethodA,

    CallNonvirtualObjectMethod,
    CallNonvirtualObjectMethodV,
    CallNonvirtualObjectMethodA,
    CallNonvirtualBooleanMethod,
    CallNonvirtualBooleanMethodV,
    CallNonvirtualBooleanMethodA,
    CallNonvirtualByteMethod,
    CallNonvirtualByteMethodV,
    CallNonvirtualByteMethodA,
    CallNonvirtualCharMethod,
    CallNonvirtualCharMethodV,
    CallNonvirtualCharMethodA,
    CallNonvirtualShortMethod,
    CallNonvirtualShortMethodV,
    CallNonvirtualShortMethodA,
    CallNonvirtualIntMethod,
    CallNonvirtualIntMethodV,
    CallNonvirtualIntMethodA,
    CallNonvirtualLongMethod,
    CallNonvirtualLongMethodV,
    CallNonvirtualLongMethodA,
    CallNonvirtualFloatMethod,
    CallNonvirtualFloatMethodV,
    CallNonvirtualFloatMethodA,
    CallNonvirtualDoubleMethod,
    CallNonvirtualDoubleMethodV,
    CallNonvirtualDoubleMethodA,
    CallNonvirtualVoidMethod,
    CallNonvirtualVoidMethodV,
    CallNonvirtualVoidMethodA,

    GetFieldID,

    GetObjectField,
    GetBooleanField,
    GetByteField,
    GetCharField,
    GetShortField,
    GetIntField,
    GetLongField,
    GetFloatField,
    GetDoubleField,
    SetObjectField,
    SetBooleanField,
    SetByteField,
    SetCharField,
    SetShortField,
    SetIntField,
    SetLongField,
    SetFloatField,
    SetDoubleField,

    GetStaticMethodID,

    CallStaticObjectMethod,
    CallStaticObjectMethodV,
    CallStaticObjectMethodA,
    CallStaticBooleanMethod,
    CallStaticBooleanMethodV,
    CallStaticBooleanMethodA,
    CallStaticByteMethod,
    CallStaticByteMethodV,
    CallStaticByteMethodA,
    CallStaticCharMethod,
    CallStaticCharMethodV,
    CallStaticCharMethodA,
    CallStaticShortMethod,
    CallStaticShortMethodV,
    CallStaticShortMethodA,
    CallStaticIntMethod,
    CallStaticIntMethodV,
    CallStaticIntMethodA,
    CallStaticLongMethod,
    CallStaticLongMethodV,
    CallStaticLongMethodA,
    CallStaticFloatMethod,
    CallStaticFloatMethodV,
    CallStaticFloatMethodA,
    CallStaticDoubleMethod,
    CallStaticDoubleMethodV,
    CallStaticDoubleMethodA,
    CallStaticVoidMethod,
    CallStaticVoidMethodV,
    CallStaticVoidMethodA,

    GetStaticFieldID,

    GetStaticObjectField,
    GetStaticBooleanField,
    GetStaticByteField,
    GetStaticCharField,
    GetStaticShortField,
    GetStaticIntField,
    GetStaticLongField,
    GetStaticFloatField,
    GetStaticDoubleField,

    SetStaticObjectField,
    SetStaticBooleanField,
    SetStaticByteField,
    SetStaticCharField,
    SetStaticShortField,
    SetStaticIntField,
    SetStaticLongField,
    SetStaticFloatField,
    SetStaticDoubleField,

    NewString,

    GetStringLength,
    GetStringChars,
    ReleaseStringChars,

    NewStringUTF,
    GetStringUTFLength,
    GetStringUTFChars,
    ReleaseStringUTFChars,

    GetArrayLength,
    NewObjectArray,
    GetObjectArrayElement,
    SetObjectArrayElement,

    NewBooleanArray,
    NewByteArray,
    NewCharArray,
    NewShortArray,
    NewIntArray,
    NewLongArray,
    NewFloatArray,
    NewDoubleArray,

    GetBooleanArrayElements,
    GetByteArrayElements,
    GetCharArrayElements,
    GetShortArrayElements,
    GetIntArrayElements,
    GetLongArrayElements,
    GetFloatArrayElements,
    GetDoubleArrayElements,

    ReleaseBooleanArrayElements,
    ReleaseByteArrayElements,
    ReleaseCharArrayElements,
    ReleaseShortArrayElements,
    ReleaseIntArrayElements,
    ReleaseLongArrayElements,
    ReleaseFloatArrayElements,
    ReleaseDoubleArrayElements,

    GetBooleanArrayRegion,
    GetByteArrayRegion,
    GetCharArrayRegion,
    GetShortArrayRegion,
    GetIntArrayRegion,
    GetLongArrayRegion,
    GetFloatArrayRegion,
    GetDoubleArrayRegion,
    SetBooleanArrayRegion,
    SetByteArrayRegion,
    SetCharArrayRegion,
    SetShortArrayRegion,
    SetIntArrayRegion,
    SetLongArrayRegion,
    SetFloatArrayRegion,
    SetDoubleArrayRegion,

    RegisterNatives,
    UnregisterNatives,

    MonitorEnter,
    MonitorExit,

    GetJavaVM,

    GetStringRegion,
    GetStringUTFRegion,

    GetPrimitiveArrayCritical,
    ReleasePrimitiveArrayCritical,

    GetStringCritical,
    ReleaseStringCritical,

    NewWeakGlobalRef,
    DeleteWeakGlobalRef,

    ExceptionCheck,

    NewDirectByteBuffer,
    GetDirectBufferAddress,
    GetDirectBufferCapacity,

    GetObjectRefType
};

static const struct JNIInvokeInterface gInvokeInterface = {
    NULL,
    NULL,
    NULL,

    DestroyJavaVM,
    AttachCurrentThread,
    DetachCurrentThread,

    GetEnv,

    AttachCurrentThreadAsDaemon,
};

JNIEnv* dvmCreateJNIEnv(Thread* self) {
    JavaVMExt* vm = (JavaVMExt*) gDvmJni.jniVm;

    if (self != NULL)
        ALOGI("Ent CreateJNIEnv: threadid=%d %p", self->threadId, self);

    assert(vm != NULL);

    JNIEnvExt* newEnv = (JNIEnvExt*) calloc(1, sizeof(JNIEnvExt));
    newEnv->funcTable = &gNativeInterface;
    if (self != NULL) {
        dvmSetJniEnvThreadId((JNIEnv*) newEnv, self);
        assert(newEnv->envThreadId != 0);
    } else {
        newEnv->envThreadId = 0x77777775;
        newEnv->self = (Thread*) 0x77777779;
    }
    if (gDvmJni.useCheckJni) {
        dvmUseCheckedJniEnv(newEnv);
    }

    ScopedPthreadMutexLock lock(&vm->envListLock);

    newEnv->next = vm->envList;
    assert(newEnv->prev == NULL);
    if (vm->envList == NULL) {
        // rare, but possible
        vm->envList = newEnv;
    } else {
        vm->envList->prev = newEnv;
    }
    vm->envList = newEnv;

    if (self != NULL)
        ALOGI("Xit CreateJNIEnv: threadid=%d %p", self->threadId, self);
    return (JNIEnv*) newEnv;
}

void dvmDestroyJNIEnv(JNIEnv* env) {
    if (env == NULL) {
        return;
    }

    //ALOGI("Ent DestroyJNIEnv: threadid=%d %p", self->threadId, self);

    JNIEnvExt* extEnv = (JNIEnvExt*) env;
    JavaVMExt* vm = (JavaVMExt*) gDvmJni.jniVm;

    ScopedPthreadMutexLock lock(&vm->envListLock);

    if (extEnv == vm->envList) {
        assert(extEnv->prev == NULL);
        vm->envList = extEnv->next;
    } else {
        assert(extEnv->prev != NULL);
        extEnv->prev->next = extEnv->next;
    }
    if (extEnv->next != NULL) {
        extEnv->next->prev = extEnv->prev;
    }

    free(env);
    //ALOGI("Xit DestroyJNIEnv: threadid=%d %p", self->threadId, self);
}

void dvmLateEnableCheckedJni() {
    JNIEnvExt* extEnv = dvmGetJNIEnvForThread();
    if (extEnv == NULL) {
        ALOGE("dvmLateEnableCheckedJni: thread has no JNIEnv");
        return;
    }
    JavaVMExt* extVm = (JavaVMExt*) gDvmJni.jniVm;
    assert(extVm != NULL);

    if (!gDvmJni.useCheckJni) {
        ALOGD("Late-enabling CheckJNI");
        dvmUseCheckedJniVm(extVm);
        dvmUseCheckedJniEnv(extEnv);
    } else {
        ALOGD("Not late-enabling CheckJNI (already on)");
    }
}

jint JNI_GetDefaultJavaVMInitArgs(void* vm_args) {
    return JNI_ERR;
}

jint JNI_GetCreatedJavaVMs(JavaVM** vmBuf, jsize bufLen, jsize* nVMs) {
    if (gDvmJni.jniVm != NULL) {
        *nVMs = 1;
        if (bufLen > 0) {
            *vmBuf++ = gDvmJni.jniVm;
        }
    } else {
        *nVMs = 0;
    }
    return JNI_OK;
}

jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
    const JavaVMInitArgs* args = (JavaVMInitArgs*) vm_args;
    if (dvmIsBadJniVersion(args->version)) {
        ALOGE("Bad JNI version passed to CreateJavaVM: %d", args->version);
        return JNI_EVERSION;
    }

    memset(&gDvm, 0, sizeof(gDvm));

    JavaVMExt* pVM = (JavaVMExt*) calloc(1, sizeof(JavaVMExt));
    pVM->funcTable = &gInvokeInterface;
    pVM->envList = NULL;
    dvmInitMutex(&pVM->envListLock);

    UniquePtr<const char*[]> argv(new const char*[args->nOptions]);
    memset(argv.get(), 0, sizeof(char*) * (args->nOptions));

    int argc = 0;
    for (int i = 0; i < args->nOptions; i++) {
        const char* optStr = args->options[i].optionString;
        if (optStr == NULL) {
            dvmFprintf(stderr, "ERROR: CreateJavaVM failed: argument %d was NULL\n", i);
            return JNI_ERR;
        } else if (strcmp(optStr, "vfprintf") == 0) {
            gDvm.vfprintfHook = (int (*)(FILE *, const char*, va_list))args->options[i].extraInfo;
        } else if (strcmp(optStr, "exit") == 0) {
            gDvm.exitHook = (void (*)(int)) args->options[i].extraInfo;
        } else if (strcmp(optStr, "abort") == 0) {
            gDvm.abortHook = (void (*)(void))args->options[i].extraInfo;
        } else if (strcmp(optStr, "sensitiveThread") == 0) {
            gDvm.isSensitiveThreadHook = (bool (*)(void))args->options[i].extraInfo;
        } else if (strcmp(optStr, "-Xcheck:jni") == 0) {
            gDvmJni.useCheckJni = true;
        } else if (strncmp(optStr, "-Xjniopts:", 10) == 0) {
            char* jniOpts = strdup(optStr + 10);
            size_t jniOptCount = 1;
            for (char* p = jniOpts; *p != 0; ++p) {
                if (*p == ',') {
                    ++jniOptCount;
                    *p = 0;
                }
            }
            char* jniOpt = jniOpts;
            for (size_t i = 0; i < jniOptCount; ++i) {
                if (strcmp(jniOpt, "warnonly") == 0) {
                    gDvmJni.warnOnly = true;
                } else if (strcmp(jniOpt, "forcecopy") == 0) {
                    gDvmJni.forceCopy = true;
                } else if (strcmp(jniOpt, "logThirdPartyJni") == 0) {
                    gDvmJni.logThirdPartyJni = true;
                } else {
                    dvmFprintf(stderr, "ERROR: CreateJavaVM failed: unknown -Xjniopts option '%s'\n",
                            jniOpt);
                    free(pVM);
                    free(jniOpts);
                    return JNI_ERR;
                }
                jniOpt += strlen(jniOpt) + 1;
            }
            free(jniOpts);
        } else {
            argv[argc++] = optStr;
        }
    }

    if (gDvmJni.useCheckJni) {
        dvmUseCheckedJniVm(pVM);
    }

    if (gDvmJni.jniVm != NULL) {
        dvmFprintf(stderr, "ERROR: Dalvik only supports one VM per process\n");
        free(pVM);
        return JNI_ERR;
    }
    gDvmJni.jniVm = (JavaVM*) pVM;

    JNIEnvExt* pEnv = (JNIEnvExt*) dvmCreateJNIEnv(NULL);

    gDvm.initializing = true;
    std::string status =
            dvmStartup(argc, argv.get(), args->ignoreUnrecognized, (JNIEnv*)pEnv);
    gDvm.initializing = false;

    if (!status.empty()) {
        free(pEnv);
        free(pVM);
        ALOGW("CreateJavaVM failed: %s", status.c_str());
        return JNI_ERR;
    }

    dvmChangeStatus(NULL, THREAD_NATIVE);
    *p_env = (JNIEnv*) pEnv;
    *p_vm = (JavaVM*) pVM;
    ALOGV("CreateJavaVM succeeded");
    return JNI_OK;
}
