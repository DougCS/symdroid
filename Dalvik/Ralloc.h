static inline RegisterClass dvmCompilerRegClassBySize(OpSize size)
{
    return (size == kUnsignedHalf ||
            size == kSignedHalf ||
            size == kUnsignedByte ||
            size == kSignedByte ) ? kCoreReg : kAnyReg;
}

static inline int dvmCompilerS2VReg(CompilationUnit *cUnit, int sReg)
{
    assert(sReg != INVALID_SREG);
    return DECODE_REG(dvmConvertSSARegToDalvik(cUnit, sReg));
}

static inline void dvmCompilerResetNullCheck(CompilationUnit *cUnit)
{
    dvmClearAllBits(cUnit->regPool->nullCheckedRegs);
}


static inline int dvmCompilerSRegHi(int lowSreg) {
    return (lowSreg == INVALID_SREG) ? INVALID_SREG : lowSreg + 1;
}


static inline bool dvmCompilerLiveOut(CompilationUnit *cUnit, int sReg)
{
    return true;
}

static inline int dvmCompilerSSASrc(MIR *mir, int num)
{
    assert(mir->ssaRep->numUses > num);
    return mir->ssaRep->uses[num];
}

extern RegLocation dvmCompilerEvalLoc(CompilationUnit *cUnit, RegLocation loc,
                                      int regClass, bool update);
extern void dvmCompilerClobber(CompilationUnit *cUnit, int reg);

extern RegLocation dvmCompilerUpdateLoc(CompilationUnit *cUnit,
                                        RegLocation loc);

extern RegLocation dvmCompilerUpdateLocWide(CompilationUnit *cUnit,
                                            RegLocation loc);

extern void dvmCompilerClobberHandlerRegs(CompilationUnit *cUnit);

extern void dvmCompilerMarkLive(CompilationUnit *cUnit, int reg, int sReg);

extern void dvmCompilerMarkDirty(CompilationUnit *cUnit, int reg);

extern void dvmCompilerMarkPair(CompilationUnit *cUnit, int lowReg,
                                int highReg);

extern void dvmCompilerMarkClean(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDef(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDefLoc(CompilationUnit *cUnit, RegLocation rl);

extern void dvmCompilerInitPool(RegisterInfo *regs, int *regNums, int num);

extern void dvmCompilerMarkDef(CompilationUnit *cUnit, RegLocation rl,
                               LIR *start, LIR *finish);
extern void dvmCompilerMarkDefWide(CompilationUnit *cUnit, RegLocation rl,
                                   LIR *start, LIR *finish);

extern RegLocation dvmCompilerGetSrcWide(CompilationUnit *cUnit, MIR *mir,
                                         int low, int high);

extern RegLocation dvmCompilerGetDestWide(CompilationUnit *cUnit, MIR *mir,
                                          int low, int high);
extern RegLocation dvmCompilerGetSrc(CompilationUnit *cUnit, MIR *mir, int num);

extern RegLocation dvmCompilerGetDest(CompilationUnit *cUnit, MIR *mir,
                                      int num);

extern RegLocation dvmCompilerGetReturnWide(CompilationUnit *cUnit);

extern void dvmCompilerClobberCallRegs(CompilationUnit *cUnit);

extern RegisterInfo *dvmCompilerIsTemp(CompilationUnit *cUnit, int reg);

extern void dvmCompilerMarkInUse(CompilationUnit *cUnit, int reg);

extern int dvmCompilerAllocTemp(CompilationUnit *cUnit);

extern int dvmCompilerAllocTempFloat(CompilationUnit *cUnit);

extern int dvmCompilerAllocTempDouble(CompilationUnit *cUnit);

extern void dvmCompilerFreeTemp(CompilationUnit *cUnit, int reg);

extern void dvmCompilerResetDefLocWide(CompilationUnit *cUnit, RegLocation rl);

extern void dvmCompilerResetDefTracking(CompilationUnit *cUnit);

extern void dvmCompilerKillNullCheckedLoc(CompilationUnit *cUnit,
                                          RegLocation loc);

extern RegisterInfo *dvmCompilerIsLive(CompilationUnit *cUnit, int reg);

extern void dvmCompilerLockAllTemps(CompilationUnit *cUnit);

extern void dvmCompilerFlushAllRegs(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturnWideAlt(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturn(CompilationUnit *cUnit);

extern RegLocation dvmCompilerGetReturnAlt(CompilationUnit *cUnit);

extern void dvmCompilerClobberSReg(CompilationUnit *cUnit, int sReg);

extern int dvmCompilerAllocFreeTemp(CompilationUnit *cUnit);

extern void dvmCompilerLockTemp(CompilationUnit *cUnit, int reg);

extern RegLocation dvmCompilerWideToNarrow(CompilationUnit *cUnit,
                                           RegLocation rl);

extern void dvmCompilerResetRegPool(CompilationUnit *cUnit);

extern void dvmCompilerClobberAllRegs(CompilationUnit *cUnit);

extern void dvmCompilerFlushRegWide(CompilationUnit *cUnit, int reg1, int reg2);

extern void dvmCompilerFlushReg(CompilationUnit *cUnit, int reg);

extern int dvmCompilerAllocTypedTempPair(CompilationUnit *cUnit,
                                         bool fpHint, int regClass);

extern int dvmCompilerAllocTypedTemp(CompilationUnit *cUnit, bool fpHint,
                                     int regClass);

extern ArmLIR* dvmCompilerRegCopy(CompilationUnit *cUnit, int rDest, int rSrc);

extern void dvmCompilerRegCopyWide(CompilationUnit *cUnit, int destLo,
                                   int destHi, int srcLo, int srcHi);

extern void dvmCompilerFlushRegImpl(CompilationUnit *cUnit, int rBase,
                                    int displacement, int rSrc, OpSize size);

extern void dvmCompilerFlushRegWideImpl(CompilationUnit *cUnit, int rBase,
                                        int displacement, int rSrcLo,
                                        int rSrcHi);
