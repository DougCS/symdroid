#ifndef DALVIK_VM_COMPILER_H_
#define DALVIK_VM_COMPILER_H_

#define COMPILER_WORK_QUEUE_SIZE        100
#define COMPILER_IC_PATCH_QUEUE_SIZE    64
#define COMPILER_PC_OFFSET_SIZE         100

#define PREDICTED_CHAIN_CLAZZ_INIT       0
#define PREDICTED_CHAIN_METHOD_INIT      0
#define PREDICTED_CHAIN_COUNTER_INIT     0
#define PREDICTED_CHAIN_FAKE_CLAZZ       0xdeadc001
#define PREDICTED_CHAIN_COUNTER_AVOID    0x7fffffff
#define PREDICTED_CHAIN_COUNTER_RECHAIN  8192

#define COMPILER_TRACED(X)
#define COMPILER_TRACEE(X)
#define COMPILER_TRACE_CHAINING(X)

#define PROTECT_CODE_CACHE_ATTRS       (PROT_READ | PROT_EXEC)
#define UNPROTECT_CODE_CACHE_ATTRS     (PROT_READ | PROT_EXEC | PROT_WRITE)

#define UNPROTECT_CODE_CACHE(addr, size)
    {
        dvmLockMutex(&gDvmJit.codeCacheProtectionLock);
        mprotect((void *) (((intptr_t) (addr)) & ~gDvmJit.pageSizeMask),
                 (size) + (((intptr_t) (addr)) & gDvmJit.pageSizeMask),
                 (UNPROTECT_CODE_CACHE_ATTRS));
    }


#define PROTECT_CODE_CACHE(addr, size)
    {
        mprotect((void *) (((intptr_t) (addr)) & ~gDvmJit.pageSizeMask),
                 (size) + (((intptr_t) (addr)) & gDvmJit.pageSizeMask),
                 (PROTECT_CODE_CACHE_ATTRS));
        dvmUnlockMutex(&gDvmJit.codeCacheProtectionLock);
    }

#define SINGLE_STEP_OP(opcode)
    (gDvmJit.includeSelectedOp !=
     ((gDvmJit.opList[opcode >> 3] & (1 << (opcode & 0x7))) != 0))

typedef enum JitInstructionSetType {
    DALVIK_JIT_NONE = 0,
    DALVIK_JIT_ARM,
    DALVIK_JIT_THUMB,
    DALVIK_JIT_THUMB2,
    DALVIK_JIT_IA32,
    DALVIK_JIT_MIPS
} JitInstructionSetType;

typedef struct JitTranslationInfo {
    void *codeAddress;
    JitInstructionSetType instructionSet;
    int profileCodeSize;
    bool discardResult;
    bool methodCompilationAborted;
    Thread *requestingThread;
    int cacheVersion;
} JitTranslationInfo;

typedef enum WorkOrderKind {
    kWorkOrderInvalid = 0,
    kWorkOrderMethod = 1,
    kWorkOrderTrace = 2,
    kWorkOrderTraceDebug = 3,
    kWorkOrderProfileMode = 4,
} WorkOrderKind;

typedef struct CompilerWorkOrder {
    const u2* pc;
    WorkOrderKind kind;
    void* info;
    JitTranslationInfo result;
    jmp_buf *bailPtr;
} CompilerWorkOrder;

/* Chain cell for predicted method invocation */
typedef struct PredictedChainingCell {
    u4 branch;
#ifdef __mips__
    u4 delay_slot;
#elif defined(ARCH_IA32)
    u4 branch2;
#endif
    const ClassObject *clazz;
    const Method *method;
    const ClassObject *stagedClazz;
} PredictedChainingCell;

typedef struct ICPatchWorkOrder {
    PredictedChainingCell *cellAddr;
    PredictedChainingCell cellContent;
    const char *classDescriptor;
    Object *classLoader;
    u4 serialNumber;
} ICPatchWorkOrder;

typedef struct {
    const Method* method;
    JitTraceRun trace[0];
} JitTraceDescription;

typedef enum JitMethodAttributes {
    kIsCallee = 0,
    kIsHot,
    kIsLeaf,
    kIsEmpty,
    kIsThrowFree,
    kIsGetter,
    kIsSetter,
    kCannotCompile,
} JitMethodAttributes;

#define METHOD_IS_CALLEE        (1 << kIsCallee)
#define METHOD_IS_HOT           (1 << kIsHot)
#define METHOD_IS_LEAF          (1 << kIsLeaf)
#define METHOD_IS_EMPTY         (1 << kIsEmpty)
#define METHOD_IS_THROW_FREE    (1 << kIsThrowFree)
#define METHOD_IS_GETTER        (1 << kIsGetter)
#define METHOD_IS_SETTER        (1 << kIsSetter)
#define METHOD_CANNOT_COMPILE   (1 << kCannotCompile)

typedef enum JitOptimizationHints {
    kJitOptNoLoop = 0,
} JitOptimizationHints;

#define JIT_OPT_NO_LOOP         (1 << kJitOptNoLoop)

typedef enum DataFlowAnalysisMode {
    kAllNodes = 0,
    kReachableNodes,
    kPreOrderDFSTraversal,
    kPostOrderDFSTraversal,
    kPostOrderDOMTraversal,
} DataFlowAnalysisMode;

typedef struct CompilerMethodStats {
    const Method *method;
    int dalvikSize;
    int compiledDalvikSize;
    int nativeSize;
    int attributes;
} CompilerMethodStats;

struct CompilationUnit;
struct BasicBlock;
struct SSARepresentation;
struct GrowableList;
struct JitEntry;
struct MIR;

bool dvmCompilerSetupCodeCache(void);
bool dvmCompilerArchInit(void);
void dvmCompilerArchDump(void);
bool dvmCompilerStartup(void);
void dvmCompilerShutdown(void);
void dvmCompilerForceWorkEnqueue(const u2* pc, WorkOrderKind kind, void* info);
bool dvmCompilerWorkEnqueue(const u2* pc, WorkOrderKind kind, void* info);
void *dvmCheckCodeCache(void *method);
CompilerMethodStats *dvmCompilerAnalyzeMethodBody(const Method *method,
                                                  bool isCallee);
bool dvmCompilerCanIncludeThisInstruction(const Method *method,
                                          const DecodedInstruction *insn);
bool dvmCompileMethod(const Method *method, JitTranslationInfo *info);
bool dvmCompileTrace(JitTraceDescription *trace, int numMaxInsts,
                     JitTranslationInfo *info, jmp_buf *bailPtr, int optHints);
void dvmCompilerDumpStats(void);
void dvmCompilerDrainQueue(void);
void dvmJitUnchainAll(void);
void dvmJitScanAllClassPointers(void (*callback)(void *ptr));
void dvmCompilerSortAndPrintTraceProfiles(void);
void dvmCompilerPerformSafePointChecks(void);
void dvmCompilerInlineMIR(struct CompilationUnit *cUnit,
                          JitTranslationInfo *info);
void dvmInitializeSSAConversion(struct CompilationUnit *cUnit);
int dvmConvertSSARegToDalvik(const struct CompilationUnit *cUnit, int ssaReg);
bool dvmCompilerLoopOpt(struct CompilationUnit *cUnit);
void dvmCompilerInsertBackwardChaining(struct CompilationUnit *cUnit);
void dvmCompilerNonLoopAnalysis(struct CompilationUnit *cUnit);
bool dvmCompilerFindLocalLiveIn(struct CompilationUnit *cUnit,
                                struct BasicBlock *bb);
bool dvmCompilerDoSSAConversion(struct CompilationUnit *cUnit,
                                struct BasicBlock *bb);
bool dvmCompilerDoConstantPropagation(struct CompilationUnit *cUnit,
                                      struct BasicBlock *bb);
bool dvmCompilerFindInductionVariables(struct CompilationUnit *cUnit,
                                       struct BasicBlock *bb);
bool dvmCompilerClearVisitedFlag(struct CompilationUnit *cUnit,
                                 struct BasicBlock *bb);
char *dvmCompilerGetDalvikDisassembly(const DecodedInstruction *insn,
                                      const char *note);
char *dvmCompilerFullDisassembler(const struct CompilationUnit *cUnit,
                                  const struct MIR *mir);
char *dvmCompilerGetSSAString(struct CompilationUnit *cUnit,
                              struct SSARepresentation *ssaRep);
void dvmCompilerDataFlowAnalysisDispatcher(struct CompilationUnit *cUnit,
                bool (*func)(struct CompilationUnit *, struct BasicBlock *),
                DataFlowAnalysisMode dfaMode,
                bool isIterative);
void dvmCompilerMethodSSATransformation(struct CompilationUnit *cUnit);
bool dvmCompilerBuildLoop(struct CompilationUnit *cUnit);
void dvmCompilerUpdateGlobalState(void);
JitTraceDescription *dvmCopyTraceDescriptor(const u2 *pc,
                                            const struct JitEntry *desc);
extern "C" void *dvmCompilerGetInterpretTemplate();
JitInstructionSetType dvmCompilerGetInterpretTemplateSet();
u8 dvmGetRegResourceMask(int reg);
void dvmDumpCFG(struct CompilationUnit *cUnit, const char *dirPrefix);
bool dvmIsOpcodeSupportedByJit(Opcode opcode);

#endif
