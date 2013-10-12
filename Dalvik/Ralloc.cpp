static void inferTypes(CompilationUnit *cUnit, BasicBlock *bb)
{
    MIR *mir;
    if (bb->blockType != kDalvikByteCode && bb->blockType != kEntryBlock)
        return;

    for (mir = bb->firstMIRInsn; mir; mir = mir->next) {
        SSARepresentation *ssaRep = mir->ssaRep;
        if (ssaRep) {
            int i;
            for (i=0; ssaRep->fpUse && i< ssaRep->numUses; i++) {
                if (ssaRep->fpUse[i])
                    cUnit->regLocation[ssaRep->uses[i]].fp = true;
            }
            for (i=0; ssaRep->fpDef && i< ssaRep->numDefs; i++) {
                if (ssaRep->fpDef[i])
                    cUnit->regLocation[ssaRep->defs[i]].fp = true;
            }
        }
    }
}

static const RegLocation freshLoc = {kLocDalvikFrame, 0, 0, INVALID_REG,
                                     INVALID_REG, INVALID_SREG};

void dvmCompilerLocalRegAlloc(CompilationUnit *cUnit)
{
    int i;
    RegLocation *loc;

    /* Allocate the location map */
    loc = (RegLocation*)dvmCompilerNew(cUnit->numSSARegs * sizeof(*loc), true);
    for (i=0; i< cUnit->numSSARegs; i++) {
        loc[i] = freshLoc;
        loc[i].sRegLow = i;
    }
    cUnit->regLocation = loc;

    GrowableListIterator iterator;

    dvmGrowableListIteratorInit(&cUnit->blockList, &iterator);
    while (true) {
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        inferTypes(cUnit, bb);
    }

    for (i=0; i < cUnit->numSSARegs; i++) {
        cUnit->regLocation[i].sRegLow =
                DECODE_REG(dvmConvertSSARegToDalvik(cUnit, loc[i].sRegLow));
    }
}
