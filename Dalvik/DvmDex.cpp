static DvmDex* allocateAuxStructures(DexFile* pDexFile)
{
    DvmDex* pDvmDex;
    const DexHeader* pHeader;
    u4 stringSize, classSize, methodSize, fieldSize;

    pHeader = pDexFile->pHeader;

    stringSize = pHeader->stringIdsSize * sizeof(struct StringObject*);
    classSize  = pHeader->typeIdsSize * sizeof(struct ClassObject*);
    methodSize = pHeader->methodIdsSize * sizeof(struct Method*);
    fieldSize  = pHeader->fieldIdsSize * sizeof(struct Field*);

    u4 totalSize = sizeof(DvmDex) +
                   stringSize + classSize + methodSize + fieldSize;

    u1 *blob = (u1 *)dvmAllocRegion(totalSize,
                              PROT_READ | PROT_WRITE, "dalvik-aux-structure");
    if ((void *)blob == MAP_FAILED)
        return NULL;

    pDvmDex = (DvmDex*)blob;
    blob += sizeof(DvmDex);

    pDvmDex->pDexFile = pDexFile;
    pDvmDex->pHeader = pHeader;

    pDvmDex->pResStrings = (struct StringObject**)blob;
    blob += stringSize;
    pDvmDex->pResClasses = (struct ClassObject**)blob;
    blob += classSize;
    pDvmDex->pResMethods = (struct Method**)blob;
    blob += methodSize;
    pDvmDex->pResFields = (struct Field**)blob;

    ALOGV("+++ DEX %p: allocateAux (%d+%d+%d+%d)*4 = %d bytes",
        pDvmDex, stringSize/4, classSize/4, methodSize/4, fieldSize/4,
        stringSize + classSize + methodSize + fieldSize);

    pDvmDex->pInterfaceCache = dvmAllocAtomicCache(DEX_INTERFACE_CACHE_SIZE);

    dvmInitMutex(&pDvmDex->modLock);

    return pDvmDex;
}

int dvmDexFileOpenFromFd(int fd, DvmDex** ppDvmDex)
{
    DvmDex* pDvmDex;
    DexFile* pDexFile;
    MemMapping memMap;
    int parseFlags = kDexParseDefault;
    int result = -1;

    if (gDvm.verifyDexChecksum)
        parseFlags |= kDexParseVerifyChecksum;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        ALOGE("lseek rewind failed");
        goto bail;
    }

    if (sysMapFileInShmemWritableReadOnly(fd, &memMap) != 0) {
        ALOGE("Unable to map file");
        goto bail;
    }

    pDexFile = dexFileParse((u1*)memMap.addr, memMap.length, parseFlags);
    if (pDexFile == NULL) {
        ALOGE("DEX parse failed");
        sysReleaseShmem(&memMap);
        goto bail;
    }

    pDvmDex = allocateAuxStructures(pDexFile);
    if (pDvmDex == NULL) {
        dexFileFree(pDexFile);
        sysReleaseShmem(&memMap);
        goto bail;
    }

    sysCopyMap(&pDvmDex->memMap, &memMap);
    pDvmDex->isMappedReadOnly = true;
    *ppDvmDex = pDvmDex;
    result = 0;

bail:
    return result;
}

int dvmDexFileOpenPartial(const void* addr, int len, DvmDex** ppDvmDex)
{
    DvmDex* pDvmDex;
    DexFile* pDexFile;
    int parseFlags = kDexParseDefault;
    int result = -1;

    if (gDvm.verifyDexChecksum)
        parseFlags |= kDexParseVerifyChecksum;

    pDexFile = dexFileParse((u1*)addr, len, parseFlags);
    if (pDexFile == NULL) {
        ALOGE("DEX parse failed");
        goto bail;
    }
    pDvmDex = allocateAuxStructures(pDexFile);
    if (pDvmDex == NULL) {
        dexFileFree(pDexFile);
        goto bail;
    }

    pDvmDex->isMappedReadOnly = false;
    *ppDvmDex = pDvmDex;
    result = 0;

bail:
    return result;
}

void dvmDexFileFree(DvmDex* pDvmDex)
{
    u4 totalSize;

    if (pDvmDex == NULL)
        return;

    dvmDestroyMutex(&pDvmDex->modLock);

    totalSize  = pDvmDex->pHeader->stringIdsSize * sizeof(struct StringObject*);
    totalSize += pDvmDex->pHeader->typeIdsSize * sizeof(struct ClassObject*);
    totalSize += pDvmDex->pHeader->methodIdsSize * sizeof(struct Method*);
    totalSize += pDvmDex->pHeader->fieldIdsSize * sizeof(struct Field*);
    totalSize += sizeof(DvmDex);

    dexFileFree(pDvmDex->pDexFile);

    ALOGV("+++ DEX %p: freeing aux structs", pDvmDex);
    dvmFreeAtomicCache(pDvmDex->pInterfaceCache);
    sysReleaseShmem(&pDvmDex->memMap);
    munmap(pDvmDex, totalSize);
}


bool dvmDexChangeDex1(DvmDex* pDvmDex, u1* addr, u1 newVal)
{
    if (*addr == newVal) {
        ALOGV("+++ byte at %p is already 0x%02x", addr, newVal);
        return true;
    }

    dvmLockMutex(&pDvmDex->modLock);

    ALOGV("+++ change byte at %p from 0x%02x to 0x%02x", addr, *addr, newVal);
    if (sysChangeMapAccess(addr, 1, true, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RW) failed");
    }

    *addr = newVal;

    if (sysChangeMapAccess(addr, 1, false, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RO) failed");
    }

    dvmUnlockMutex(&pDvmDex->modLock);

    return true;
}

bool dvmDexChangeDex2(DvmDex* pDvmDex, u2* addr, u2 newVal)
{
    if (*addr == newVal) {
        ALOGV("+++ value at %p is already 0x%04x", addr, newVal);
        return true;
    }

    dvmLockMutex(&pDvmDex->modLock);

    ALOGV("+++ change 2byte at %p from 0x%04x to 0x%04x", addr, *addr, newVal);
    if (sysChangeMapAccess(addr, 2, true, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RW) failed");
    }

    *addr = newVal;

    if (sysChangeMapAccess(addr, 2, false, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RO) failed");
    }

    dvmUnlockMutex(&pDvmDex->modLock);

    return true;
}
