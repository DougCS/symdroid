static TGT_LIR *genRegImmCheck(CompilationUnit *cUnit,
                               ArmConditionCode cond, int reg,
                               int checkValue, int dOffset,
                               TGT_LIR *pcrLabel)
{
    TGT_LIR *branch = genCmpImmBranch(cUnit, cond, reg, checkValue);
    if (cUnit->jitMode == kJitMethod) {
        BasicBlock *bb = cUnit->curBlock;
        if (bb->taken) {
            ArmLIR  *exceptionLabel = (ArmLIR *) cUnit->blockLabelList;
            exceptionLabel += bb->taken->id;
            branch->generic.target = (LIR *) exceptionLabel;
            return exceptionLabel;
        } else {
            ALOGE("Catch blocks not handled yet");
            dvmAbort();
            return NULL;
        }
    } else {
        return genCheckCommon(cUnit, dOffset, branch, pcrLabel);
    }
}

static TGT_LIR *genNullCheck(CompilationUnit *cUnit, int sReg, int mReg,
                             int dOffset, TGT_LIR *pcrLabel)
{
    if (dvmIsBitSet(cUnit->regPool->nullCheckedRegs, sReg)) {
        return pcrLabel;
    }
    dvmSetBit(cUnit->regPool->nullCheckedRegs, sReg);
    return genRegImmCheck(cUnit, kArmCondEq, mReg, 0, dOffset, pcrLabel);
}

static TGT_LIR *genRegRegCheck(CompilationUnit *cUnit,
                               ArmConditionCode cond,
                               int reg1, int reg2, int dOffset,
                               TGT_LIR *pcrLabel)
{
    TGT_LIR *res;
    res = opRegReg(cUnit, kOpCmp, reg1, reg2);
    TGT_LIR *branch = opCondBranch(cUnit, cond);
    genCheckCommon(cUnit, dOffset, branch, pcrLabel);
    return res;
}

static TGT_LIR *genZeroCheck(CompilationUnit *cUnit, int mReg,
                             int dOffset, TGT_LIR *pcrLabel)
{
    return genRegImmCheck(cUnit, kArmCondEq, mReg, 0, dOffset, pcrLabel);
}

static TGT_LIR *genBoundsCheck(CompilationUnit *cUnit, int rIndex,
                               int rBound, int dOffset, TGT_LIR *pcrLabel)
{
    return genRegRegCheck(cUnit, kArmCondCs, rIndex, rBound, dOffset,
                          pcrLabel);
}

static void genDispatchToHandler(CompilationUnit *cUnit, TemplateOpcode opcode)
{
    dvmCompilerClobberHandlerRegs(cUnit);
    newLIR2(cUnit, kThumbBlx1,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
    newLIR2(cUnit, kThumbBlx2,
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode],
            (int) gDvmJit.codeCache + templateEntryOffsets[opcode]);
}
