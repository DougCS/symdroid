#ifndef _DALVIK_VM_LOOP
#define _DALVIK_VM_LOOP

#include "Dalvik.h"
#include "CompilerInternals.h"

typedef struct LoopAnalysis {
    BitVector *isIndVarV;
    GrowableList *ivList;
    GrowableList *arrayAccessInfo;
    int numBasicIV;
    int ssaBIV
    bool isCountUpLoop;
    OpCode loopBranchOpcode;
    int endConditionReg;
    LIR *branchToBody;
    LIR *branchToPCR;
    bool bodyIsClean;
} LoopAnalysis;
