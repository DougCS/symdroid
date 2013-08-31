#ifndef _DALVIK_INTERP_DEFS
#define _DALVIK_INTERP_DEFS


typedef enum InterpEntry {
    kInterpEntryInstr = 0,      // continue to next instruction
    kInterpEntryReturn = 1,     // jump to method return
    kInterpEntryThrow = 2,      // jump to exception throw
#if defined(WITH_JIT)
    kInterpEntryResume = 3,     // Resume after single-step
#endif
} InterpEntry;

#if defined(WITH_JIT)
struct JitToInterpEntries {
    void *dvmJitToInterpNormal;
    void *dvmJitToInterpNoChain;
    void *dvmJitToInterpPunt;
    void *dvmJitToInterpSingleStep;
    void *dvmJitToInterpTraceSelectNoChain;
    void *dvmJitToInterpTraceSelect;
    void *dvmJitToPatchPredictedChain;
#if defined(WITH_SELF_VERIFICATION)
    void *dvmJitToInterpBackwardBranch;
#endif
};

#define JIT_CALLEE_SAVE_DOUBLE_COUNT 8

#define JIT_TRACE_THRESH_FILTER_SIZE 32
#define JIT_TRACE_THRESH_FILTER_PC_BITS 4
#endif

typedef struct InterpState {
    const u2*   pc; //program counter
    u4*         fp;

    JValue      retval;
    const Method* method;


    DvmDex*         methodClassDex;
    Thread*         self;

    void*           bailPtr;

    const u1*       interpStackEnd;
    volatile int*   pSelfSuspendCount;
    u1*             cardTable;
    volatile u1*    pDebuggerActive;
    volatile int*   pActiveProfilers;

    InterpEntry entryPoint;             // what to do when we start
    int         nextMode;               // INTERP_STD, INTERP_DBG

#if defined(WITH_JIT)
    unsigned char*     pJitProfTable;
    JitState           jitState;
    const void*        jitResumeNPC;    // Native PC of compiled code
    const u2*          jitResumeDPC;    // Dalvik PC corresponding to NPC
    int                jitThreshold;
    unsigned char**    ppJitProfTable; // Used to refresh pJitProfTable
    int                icRechainCount; // Count down to next rechain request
#endif

    bool        debugIsMethodEntry;     // used for method entry event triggers
#if defined(WITH_TRACKREF_CHECKS)
    int         debugTrackedRefStart;   // tracked refs from prior invocations
#endif

#if defined(WITH_JIT)
    struct JitToInterpEntries jitToInterpEntries;

    int currTraceRun;
    int totalTraceLen;        // Number of Dalvik insts in trace
    const u2* currTraceHead;  // Start of the trace we're building
    const u2* currRunHead;    // Start of run we're building
    int currRunLen;           // Length of run in 16-bit words
    int lastThreshFilter;
    const u2* lastPC;         // Stage the PC first for the threaded interpreter
    intptr_t threshFilter[JIT_TRACE_THRESH_FILTER_SIZE];
    JitTraceRun trace[MAX_JIT_RUN_LEN];
    double calleeSave[JIT_CALLEE_SAVE_DOUBLE_COUNT];
#endif

} InterpState;

extern bool dvmInterpretDbg(Thread* self, InterpState* interpState);
extern bool dvmInterpretStd(Thread* self, InterpState* interpState);
#define INTERP_STD 0
#define INTERP_DBG 1

extern bool dvmMterpStd(Thread* self, InterpState* interpState);

Object* dvmGetThisPtr(const Method* method, const u4* fp);

void dvmInterpCheckTrackedRefs(Thread* self, const Method* method,
    int debugTrackedRefStart);

s4 dvmInterpHandlePackedSwitch(const u2* switchData, s4 testVal);
s4 dvmInterpHandleSparseSwitch(const u2* switchData, s4 testVal);

bool dvmInterpHandleFillArrayData(ArrayObject* arrayObject,
                                  const u2* arrayData);

Method* dvmInterpFindInterfaceMethod(ClassObject* thisClass, u4 methodIdx,
    const Method* method, DvmDex* methodClassDex);

static inline bool dvmDebuggerOrProfilerActive(void)
{
    return gDvm.debuggerActive || gDvm.activeProfilers != 0;
}

#if defined(WITH_JIT)
static inline bool dvmJitDebuggerOrProfilerActive()
{
    return gDvmJit.pProfTable != NULL
        || gDvm.activeProfilers != 0
        || gDvm.debuggerActive;
}

static inline bool dvmJitHideTranslation()
{
    return (gDvm.sumThreadSuspendCount != 0) ||
           (gDvmJit.codeCacheFull == true) ||
           (gDvmJit.pProfTable == NULL);
}

static inline bool dvmJitStayInPortableInterpreter()
{
    return dvmJitHideTranslation() ||
           (gDvmJit.compilerQueueLength >= gDvmJit.compilerHighWater);
}
#endif

#endif
