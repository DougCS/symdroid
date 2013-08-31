#ifndef _GLOBALS
#define GLOBALS

#include <stdarg.h>
#include <pthread.h>

#define MAX_BREAKPOINTS 20

typedef struct GcHeap GcHeap;
typedef struct BreakpointSet BreakpointSet;
typedef struct InlineSub InlineSub;

typedef struct AssertionControl {
    char*   pkgOrClass;
    int     pkgOrClassLen;
    bool    enable;
    bool    isPackage;
} AssertionControl;

typedef enum ExecutionMode {
    kExecutionModeUnknown = 0,
    kExecutionModeInterpPortable,
    kExecutionModeInterpFast,
#if defined(WITH_JIT)
    kExecutionModeJit,
#endif
} ExecutionMode;

struct DvmGlobals {
    char*       bootClassPathStr;
    char*       classPathStr;

    unsigned int    heapSizeStart;
    unsigned int    heapSizeMax;
    unsigned int    stackSize;

    bool        verboseGc;
    bool        verboseJni;
    bool        verboseClass;
    bool        verboseShutdown;

    bool        jdwpAllowed;
    bool        jdwpConfigured;
    int         jdwpTransport;
    bool        jdwpServer;
    char*       jdwpHost;
    int         jdwpPort;
    bool        jdwpSuspend;

    bool        profilerWallClock;

    u4          lockProfThreshold;

    int         (*vfprintfHook)(FILE*, const char*, va_list);
    void        (*exitHook)(int);
    void        (*abortHook)(void);

    int         jniGrefLimit;       // 0 means no limit
    char*       jniTrace;
    bool        reduceSignals;
    bool        noQuitHandler;
    bool        verifyDexChecksum;
    char*       stackTraceFile;

    bool        logStdio;

    DexOptimizerMode    dexOptMode;
    DexClassVerifyMode  classVerifyMode;

    bool        dexOptForSmp;

    bool        preciseGc;
    bool        preVerify;
    bool        postVerify;
    bool        generateRegisterMaps;
    bool        concurrentMarkSweep;
    bool        verifyCardTable;

    int         assertionCtrlCount;
    AssertionControl*   assertionCtrl;

    ExecutionMode   executionMode;

    bool        initializing;
    int         initExceptionCount;
    bool        optimizing;

    /*
     * java.lang.System properties set from the command line.
     */
    int         numProps;
    int         maxProps;
    char**      propList;

    ClassPathEntry* bootClassPath;
    /* used by the DEX optimizer to load classes from an unfinished DEX */
    DvmDex*     bootClassPathOptExtra;
    bool        optimizingBootstrapClass;

    HashTable*  loadedClasses;

    volatile int classSerialNumber;

    InitiatingLoaderList* initiatingLoaderList;


    pthread_mutex_t internLock;

    HashTable*  internedStrings;

    HashTable*  literalStrings;

    ClassObject* classJavaLangClass;
    ClassObject* classJavaLangClassArray;
    ClassObject* classJavaLangError;
    ClassObject* classJavaLangObject;
    ClassObject* classJavaLangObjectArray;
    ClassObject* classJavaLangRuntimeException;
    ClassObject* classJavaLangString;
    ClassObject* classJavaLangThread;
    ClassObject* classJavaLangVMThread;
    ClassObject* classJavaLangThreadGroup;
    ClassObject* classJavaLangThrowable;
    ClassObject* classJavaLangStackOverflowError;
    ClassObject* classJavaLangStackTraceElement;
    ClassObject* classJavaLangStackTraceElementArray;
    ClassObject* classJavaLangAnnotationAnnotationArray;
    ClassObject* classJavaLangAnnotationAnnotationArrayArray;
    ClassObject* classJavaLangReflectAccessibleObject;
    ClassObject* classJavaLangReflectConstructor;
    ClassObject* classJavaLangReflectConstructorArray;
    ClassObject* classJavaLangReflectField;
    ClassObject* classJavaLangReflectFieldArray;
    ClassObject* classJavaLangReflectMethod;
    ClassObject* classJavaLangReflectMethodArray;
    ClassObject* classJavaLangReflectProxy;
    ClassObject* classJavaLangExceptionInInitializerError;
    ClassObject* classJavaLangRefPhantomReference;
    ClassObject* classJavaLangRefReference;
    ClassObject* classJavaNioReadWriteDirectByteBuffer;
    ClassObject* classJavaSecurityAccessController;
    ClassObject* classOrgApacheHarmonyLangAnnotationAnnotationFactory;
    ClassObject* classOrgApacheHarmonyLangAnnotationAnnotationMember;
    ClassObject* classOrgApacheHarmonyLangAnnotationAnnotationMemberArray;
    ClassObject* classOrgApacheHarmonyNioInternalDirectBuffer;
    jclass      jclassOrgApacheHarmonyNioInternalDirectBuffer;

    ClassObject* classArrayBoolean;
    ClassObject* classArrayChar;
    ClassObject* classArrayFloat;
    ClassObject* classArrayDouble;
    ClassObject* classArrayByte;
    ClassObject* classArrayShort;
    ClassObject* classArrayInt;
    ClassObject* classArrayLong;

    int         voffJavaLangObject_equals;
    int         voffJavaLangObject_hashCode;
    int         voffJavaLangObject_toString;
    int         voffJavaLangObject_finalize;

    int         offJavaLangClass_pd;

    int         javaLangStringReady;
    int         offJavaLangString_value;
    int         offJavaLangString_count;
    int         offJavaLangString_offset;
    int         offJavaLangString_hashCode;

    int         offJavaLangThread_vmThread;
    int         offJavaLangThread_group;
    int         offJavaLangThread_daemon;
    int         offJavaLangThread_name;
    int         offJavaLangThread_priority;

    int         voffJavaLangThread_run;

    int         offJavaLangVMThread_thread;
    int         offJavaLangVMThread_vmData;

    int         voffJavaLangThreadGroup_removeThread;

    int         offJavaLangThrowable_stackState;
    int         offJavaLangThrowable_message;
    int         offJavaLangThrowable_cause;

    int         offJavaLangReflectAccessibleObject_flag;
    int         offJavaLangReflectConstructor_slot;
    int         offJavaLangReflectConstructor_declClass;
    int         offJavaLangReflectField_slot;
    int         offJavaLangReflectField_declClass;
    int         offJavaLangReflectMethod_slot;
    int         offJavaLangReflectMethod_declClass;

    int         offJavaLangRefReference_referent;
    int         offJavaLangRefReference_queue;
    int         offJavaLangRefReference_queueNext;
    int         offJavaLangRefReference_pendingNext;

    Method*     methJavaLangRefReference_enqueueInternal;

    //int         offJavaNioBuffer_capacity;
    //int         offJavaNioDirectByteBufferImpl_pointer;

    volatile int javaSecurityAccessControllerReady;
    Method*     methJavaSecurityAccessController_doPrivileged[4];

    Method*     methJavaLangStackTraceElement_init;
    Method*     methJavaLangExceptionInInitializerError_init;
    Method*     methJavaLangRefPhantomReference_init;
    Method*     methJavaLangReflectConstructor_init;
    Method*     methJavaLangReflectField_init;
    Method*     methJavaLangReflectMethod_init;
    Method*     methOrgApacheHarmonyLangAnnotationAnnotationMember_init;

    Method*
        methOrgApacheHarmonyLangAnnotationAnnotationFactory_createAnnotation;

    Method*     methJavaLangReflectProxy_constructorPrototype;

    int         offJavaLangReflectProxy_h;

    /* fake native entry point method */
    Method*     methFakeNativeEntry;

    Method*     methJavaNioReadWriteDirectByteBuffer_init;
    Method*     methOrgApacheHarmonyLuniPlatformPlatformAddress_on;
    Method*     methOrgApacheHarmonyNioInternalDirectBuffer_getEffectiveAddress;
    int         offJavaNioBuffer_capacity;
    int         offJavaNioBuffer_effectiveDirectAddress;
    int         offOrgApacheHarmonyLuniPlatformPlatformAddress_osaddr;
    int         voffOrgApacheHarmonyLuniPlatformPlatformAddress_toLong;

    ClassObject* volatile primitiveClass[PRIM_MAX];

    Thread*     threadList;
    pthread_mutex_t threadListLock;

    pthread_cond_t threadStartCond;

    pthread_mutex_t _threadSuspendLock;

    pthread_mutex_t threadSuspendCountLock;

    pthread_cond_t  threadSuspendCountCond;

    int  sumThreadSuspendCount;



    BitVector*  threadIdMap;

    int         nonDaemonThreadCount;   /* must hold threadListLock to access */
    //pthread_mutex_t vmExitLock;
    pthread_cond_t  vmExitCond;

    HashTable*  userDexFiles;

    /*
     * JNI global reference table.
     */
#ifdef USE_INDIRECT_REF
    IndirectRefTable jniGlobalRefTable;
#else
    ReferenceTable  jniGlobalRefTable;
#endif
    pthread_mutex_t jniGlobalRefLock;
    int         jniGlobalRefHiMark;
    int         jniGlobalRefLoMark;

    ReferenceTable  jniPinRefTable;
    pthread_mutex_t jniPinRefLock;

    HashTable*  nativeLibs;

    pthread_mutex_t gcHeapLock;

    pthread_cond_t gcHeapCond;

    GcHeap*     gcHeap;

    u1*         biasedCardTableBase;

    Object*     outOfMemoryObj;
    Object*     internalErrorObj;
    Object*     noClassDefFoundErrorObj;

    /*volatile*/ Monitor* monitorList;

    Monitor*    threadSleepMon;

    bool        newZygoteHeapAllocated;

    pthread_key_t pthreadKeySelf;

    JavaVM*     vmList;

    AtomicCache* instanceofCache;

    InstructionWidth*   instrWidth;
    InstructionFlags*   instrFlags;
    InstructionFormat*  instrFormat;

    InlineSub*          inlineSubs;

    LinearAllocHdr* pBootLoaderAlloc;


    bool            heapWorkerInitialized;
    bool            heapWorkerReady;
    bool            haltHeapWorker;
    pthread_t       heapWorkerHandle;
    pthread_mutex_t heapWorkerLock;
    pthread_cond_t  heapWorkerCond;
    pthread_cond_t  heapWorkerIdleCond;
    pthread_mutex_t heapWorkerListLock;

    int         numLoadedClasses;
    int         numDeclaredMethods;
    int         numDeclaredInstFields;
    int         numDeclaredStaticFields;

    bool        nativeDebuggerActive;

    bool        debuggerConnected;      /* debugger or DDMS is connected */
    u1          debuggerActive;         /* debugger is making requests */
    JdwpState*  jdwpState;

    HashTable*  dbgRegistry;

    BreakpointSet*  breakpointSet;

    StepControl stepControl;

    bool        ddmThreadNotification;

    bool        zygote;

    pthread_mutex_t allocTrackerLock;
    AllocRecord*    allocRecords;
    int             allocRecordHead;        /* most-recently-added entry */
    int             allocRecordCount;       /* #of valid entries */

#ifdef WITH_ALLOC_LIMITS
    bool        checkAllocLimits;
    int         allocationLimit;
#endif

#ifdef WITH_DEADLOCK_PREDICTION
    pthread_mutex_t deadlockHistoryLock;

    enum { kDPOff=0, kDPWarn, kDPErr, kDPAbort } deadlockPredictMode;
#endif

    volatile int activeProfilers;

    MethodTraceState methodTrace;

    void*       emulatorTracePage;
    int         emulatorTraceEnableCount;

    AllocProfState allocProf;

    Method**    inlinedMethods;

    int*        executedInstrCounts;
    bool        instructionCountEnableCount;

    pthread_t   signalCatcherHandle;
    bool        haltSignalCatcher;

    bool            haltStdioConverter;
    bool            stdioConverterReady;
    pthread_t       stdioConverterHandle;
    pthread_mutex_t stdioConverterLock;
    pthread_cond_t  stdioConverterCond;

    pid_t systemServerPid;

    int kernelGroupScheduling;

//#define COUNT_PRECISE_METHODS
#ifdef COUNT_PRECISE_METHODS
    PointerSet* preciseMethods;
#endif

    /* some RegisterMap statistics, useful during development XD */
    void*       registerMapStats;
};

extern struct DvmGlobals gDvm;

#if defined(WITH_JIT)

typedef enum NoChainExits {
    kInlineCacheMiss = 0,
    kCallsiteInterpreted,
    kSwitchOverflow,
    kHeavyweightMonitor,
    kNoChainExitLast,
} NoChainExits;

struct DvmJitGlobals {
    pthread_mutex_t tableLock;

    struct JitEntry *pJitEntryTable;

    unsigned char *pProfTable;

    unsigned char *pProfTableCopy;

    unsigned int jitTableSize;

    unsigned int jitTableMask;

    unsigned int jitTableEntriesUsed;

    unsigned int codeCacheSize;

    unsigned short threshold;

    bool               haltCompilerThread;
    bool               blockingMode;
    pthread_t          compilerHandle;
    pthread_mutex_t    compilerLock;
    pthread_mutex_t    compilerICPatchLock;
    pthread_cond_t     compilerQueueActivity;
    pthread_cond_t     compilerQueueEmpty;
    volatile int       compilerQueueLength;
    int                compilerHighWater;
    int                compilerWorkEnqueueIndex;
    int                compilerWorkDequeueIndex;
    int                compilerICPatchIndex;

    int                compilerMaxQueued;
    int                translationChains;

    void* codeCache;

    unsigned int templateSize;

    unsigned int codeCacheByteUsed;

    unsigned int numCompilations;

    bool codeCacheFull;

    unsigned int pageSizeMask;

    pthread_mutex_t    codeCacheProtectionLock;

    int numCodeCacheReset;

    int numCodeCacheResetDelayed;

    bool includeSelectedOp;

    bool includeSelectedMethod;

    char opList[32];

    HashTable *methodTable;

    bool printMe;

    bool profile;

    int disableOpt;

    HashTable*  methodStatsTable;

    bool checkCallGraph;

    volatile bool hasNewChain;

#if defined(WITH_SELF_VERIFICATION)
    volatile bool selfVerificationSpin;
#endif

    bool runningInAndroidFramework;

    bool alreadyEnabledViaFramework;

    bool disableJit;

#if defined(SIGNATURE_BREAKPOINT)
    u4 signatureBreakpointSize;
    u4 *signatureBreakpoint;
#endif

#if defined(WITH_JIT_TUNING)
    int                addrLookupsFound;
    int                addrLookupsNotFound;
    int                noChainExit[kNoChainExitLast];
    int                normalExit;
    int                puntExit;
    int                invokeMonomorphic;
    int                invokePolymorphic;
    int                invokeNative;
    int                invokeMonoGetterInlined;
    int                invokeMonoSetterInlined;
    int                invokePolyGetterInlined;
    int                invokePolySetterInlined;
    int                returnOp;
    int                icPatchInit;
    int                icPatchLockFree;
    int                icPatchQueued;
    int                icPatchRejected;
    int                icPatchDropped;
    u8                 jitTime;
    int                codeCachePatches;
#endif


    CompilerWorkOrder compilerWorkQueue[COMPILER_WORK_QUEUE_SIZE];

    ICPatchWorkOrder compilerICPatchQueue[COMPILER_IC_PATCH_QUEUE_SIZE];
};

extern struct DvmJitGlobals gDvmJit;

#if defined(WITH_JIT_TUNING)
extern int gDvmICHitCount;
#endif

#endif

#endif
