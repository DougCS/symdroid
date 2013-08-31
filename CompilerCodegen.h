#ifndef _DALVIK_VM_COMPILERCODEGEN_H_
#define _DALVIK_VM_COMPILERCODEGEN_H_

#include "CompilerIR.h"

#define MAX_CHAINED_SWITCH_CASES 64

bool dvmCompilerDoWork(CompilerWorkOrder *work);

void dvmCompilerMIR2LIR(CompilationUnit *cUnit);

void dvmCompilerAssembleLIR(CompilationUnit *cUnit, JitTranslationInfo *info);

bool dvmJitPatchInlineCache(void *cellPtr, void *contentPtr);

void dvmCompilerCodegenDump(CompilationUnit *cUnit);

void* dvmJitChain(void *tgtAddr, u4* branchAddr);
u4* dvmJitUnchain(void *codeAddr);
void dvmJitUnchainAll(void);
void dvmCompilerPatchInlineCache(void);

void dvmCompilerRegAlloc(CompilationUnit *cUnit);

void dvmCompilerInitializeRegAlloc(CompilationUnit *cUnit);

JitInstructionSetType dvmCompilerInstructionSet(void);

bool dvmCompilerArchVariantInit(void);

int dvmCompilerTargetOptHint(int key);

void dvmCompilerGenMemBarrier(CompilationUnit *cUnit);

#endif /* _DALVIK_VM_COMPILERCODEGEN_H_ */
