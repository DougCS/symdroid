#ifndef DALVIK_VM_COMPILER_IR_H_
#define DALVIK_VM_COMPILER_IR_H_

#include "codegen/Optimizer.h"
#ifdef ARCH_IA32
#include "CompilerUtility.h"
#endif

typedef enum RegisterClass {
    kCoreReg,
    kFPReg,
    kAnyReg,
} RegisterClass;

typedef enum RegLocationType {
    kLocDalvikFrame = 0,
    kLocPhysReg,
    kLocRetval,
    kLocSpill,
} RegLocationType;

typedef struct RegLocation {
    RegLocationType location:2;
    unsigned wide:1;
    unsigned fp:1;
    u1 lowReg:6;
    u1 highReg:6;
    s2 sRegLow;
} RegLocation;

#define INVALID_SREG (-1)
#define INVALID_REG (0x3F)

typedef enum BBType {
    kChainingCellNormal = 0,
    kChainingCellHot,
    kChainingCellInvokeSingleton,
    kChainingCellInvokePredicted,
    kChainingCellBackwardBranch,
    kChainingCellGap,
    kChainingCellLast = kChainingCellGap + 1,
    kEntryBlock,
    kDalvikByteCode,
    kExitBlock,
    kPCReconstruction,
    kExceptionHandling,
    kCatchEntry,
} BBType;

typedef enum JitMode {
    kJitTrace = 0,
    kJitLoop,
    kJitMethod,
} JitMode;

typedef struct ChainCellCounts {
    union {
        u1 count[kChainingCellLast];
        u4 dummyForAlignment;
    } u;
} ChainCellCounts;

typedef struct LIR {
    int offset;
    struct LIR *next;
    struct LIR *prev;
    struct LIR *target;
} LIR;

enum ExtendedMIROpcode {
    kMirOpFirst = kNumPackedOpcodes,
    kMirOpPhi = kMirOpFirst,
    kMirOpNullNRangeUpCheck,
    kMirOpNullNRangeDownCheck,
    kMirOpLowerBound,
    kMirOpPunt,
    kMirOpCheckInlinePrediction,
    kMirOpLast,
};

struct SSARepresentation;

typedef enum {
    kMIRIgnoreNullCheck = 0,
    kMIRNullCheckOnly,
    kMIRIgnoreRangeCheck,
    kMIRRangeCheckOnly,
    kMIRInlined,
    kMIRInlinedPred,
    kMIRCallee,
    kMIRInvokeMethodJIT,
} MIROptimizationFlagPositons;

#define MIR_IGNORE_NULL_CHECK           (1 << kMIRIgnoreNullCheck)
#define MIR_NULL_CHECK_ONLY             (1 << kMIRNullCheckOnly)
#define MIR_IGNORE_RANGE_CHECK          (1 << kMIRIgnoreRangeCheck)
#define MIR_RANGE_CHECK_ONLY            (1 << kMIRRangeCheckOnly)
#define MIR_INLINED                     (1 << kMIRInlined)
#define MIR_INLINED_PRED                (1 << kMIRInlinedPred)
#define MIR_CALLEE                      (1 << kMIRCallee)
#define MIR_INVOKE_METHOD_JIT           (1 << kMIRInvokeMethodJIT)

typedef struct CallsiteInfo {
    const char *classDescriptor;
    Object *classLoader;
    const Method *method;
    LIR *misPredBranchOver;
} CallsiteInfo;

typedef struct MIR {
    DecodedInstruction dalvikInsn;
    unsigned int width;
    unsigned int offset;
    struct MIR *prev;
    struct MIR *next;
    struct SSARepresentation *ssaRep;
    int OptimizationFlags;
    int seqNum;
    union {
        const Method *calleeMethod;
        CallsiteInfo *callsiteInfo;
    } meta;
} MIR;

struct BasicBlockDataFlow;

typedef enum BlockListType {
    kNotUsed = 0,
    kCatch,
    kPackedSwitch,
    kSparseSwitch,
} BlockListType;

typedef struct BasicBlock {
    int id;
    bool visited;
    bool hidden;
    unsigned int startOffset;
    const Method *containingMethod;
    BBType blockType;
    bool needFallThroughBranch;
    bool isFallThroughFromInvoke;
    MIR *firstMIRInsn;
    MIR *lastMIRInsn;
    struct BasicBlock *fallThrough;
    struct BasicBlock *taken;
    struct BasicBlock *iDom;
    struct BasicBlockDataFlow *dataFlowInfo;
    BitVector *predecessors;
    BitVector *dominators;
    BitVector *iDominated;
    BitVector *domFrontier;
    struct {
        BlockListType blockListType;
        GrowableList blocks;
    } successorBlockList;
} BasicBlock;

typedef struct SuccessorBlockInfo {
    BasicBlock *block;
    int key;
} SuccessorBlockInfo;

struct LoopAnalysis;
struct RegisterPool;

typedef enum AssemblerStatus {
    kSuccess,
    kRetryAll,
    kRetryHalve
} AssemblerStatus;

typedef struct CompilationUnit {
    int numInsts;
    int numBlocks;
    GrowableList blockList;
    const Method *method;
#ifdef ARCH_IA32
    int exceptionBlockId;
#endif
    const JitTraceDescription *traceDesc;
    LIR *firstLIRInsn;
    LIR *lastLIRInsn;
    LIR *literalList;
    LIR *classPointerList;
    int numClassPointers;
    LIR *chainCellOffsetLIR;
    GrowableList pcReconstructionList;
    int headerSize;
    int dataOffset;
    int totalSize;
    AssemblerStatus assemblerStatus;
    int assemblerRetries;
    unsigned char *codeBuffer;
    void *baseAddr;
    bool printMe;
    bool allSingleStep;
    bool hasClassLiterals;
    bool hasLoop;
    bool hasInvoke;
    bool heapMemOp;
    bool usesLinkRegister;
    int profileCodeSize;
    int numChainingCells[kChainingCellGap];
    LIR *firstChainingLIR[kChainingCellGap];
    LIR *chainingCellBottom;
    struct RegisterPool *regPool;
    int optRound;
    jmp_buf *bailPtr;
    JitInstructionSetType instructionSet;
    int numSSARegs;
    GrowableList *ssaToDalvikMap;

    int *dalvikToSSAMap;
    BitVector *isConstantV;
    int *constantValues;

    struct LoopAnalysis *loopAnalysis;

    RegLocation *regLocation;
    int sequenceNumber;

    const u2 *switchOverflowPad;

    JitMode jitMode;
    int numReachableBlocks;
    int numDalvikRegisters;
    BasicBlock *entryBlock;
    BasicBlock *exitBlock;
    BasicBlock *puntBlock;
    BasicBlock *backChainBlock;
    BasicBlock *curBlock;
    BasicBlock *nextCodegenBlock;
    GrowableList dfsOrder;
    GrowableList domPostOrderTraversal;
    BitVector *tryBlockAddr;
    BitVector **defBlockMatrix;
    BitVector *tempBlockV;
    BitVector *tempDalvikRegisterV;
    BitVector *tempSSARegisterV;
    bool printSSANames;
    void *blockLabelList;
    bool quitLoopMode;
} CompilationUnit;

#if defined(WITH_SELF_VERIFICATION)
#define HEAP_ACCESS_SHADOW(_state) cUnit->heapMemOp = _state
#else
#define HEAP_ACCESS_SHADOW(_state)
#endif

BasicBlock *dvmCompilerNewBB(BBType blockType, int blockId);

void dvmCompilerAppendMIR(BasicBlock *bb, MIR *mir);

void dvmCompilerPrependMIR(BasicBlock *bb, MIR *mir);

void dvmCompilerInsertMIRAfter(BasicBlock *bb, MIR *currentMIR, MIR *newMIR);

void dvmCompilerAppendLIR(CompilationUnit *cUnit, LIR *lir);

void dvmCompilerInsertLIRBefore(LIR *currentLIR, LIR *newLIR);

void dvmCompilerInsertLIRAfter(LIR *currentLIR, LIR *newLIR);

void dvmCompilerAbort(CompilationUnit *cUnit);

void dvmCompilerDumpCompilationUnit(CompilationUnit *cUnit);

#endif
