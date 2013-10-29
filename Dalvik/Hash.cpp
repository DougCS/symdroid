size_t dvmHashSize(size_t size) {
    return (size * LOAD_DENOM) / LOAD_NUMER +1;
}

HashTable* dvmHashTableCreate(size_t initialSize, HashFreeFunc freeFunc)
{
    HashTable* pHashTable;

    assert(initialSize > 0);

    pHashTable = (HashTable*) malloc(sizeof(*pHashTable));
    if (pHashTable == NULL)
        return NULL;

    dvmInitMutex(&pHashTable->lock);

    pHashTable->tableSize = dexRoundUpPower2(initialSize);
    pHashTable->numEntries = pHashTable->numDeadEntries = 0;
    pHashTable->freeFunc = freeFunc;
    pHashTable->pEntries =
        (HashEntry*) calloc(pHashTable->tableSize, sizeof(HashEntry));
    if (pHashTable->pEntries == NULL) {
        free(pHashTable);
        return NULL;
    }

    return pHashTable;
}

void dvmHashTableClear(HashTable* pHashTable)
{
    HashEntry* pEnt;
    int i;

    pEnt = pHashTable->pEntries;
    for (i = 0; i < pHashTable->tableSize; i++, pEnt++) {
        if (pEnt->data == HASH_TOMBSTONE) {
            pEnt->data = NULL;
        } else if (pEnt->data != NULL) {
            if (pHashTable->freeFunc != NULL)
                (*pHashTable->freeFunc)(pEnt->data);
            pEnt->data = NULL;
        }
    }

    pHashTable->numEntries = 0;
    pHashTable->numDeadEntries = 0;
}

void dvmHashTableFree(HashTable* pHashTable)
{
    if (pHashTable == NULL)
        return;
    dvmHashTableClear(pHashTable);
    free(pHashTable->pEntries);
    free(pHashTable);
}

#ifndef NDEBUG
static int countTombStones(HashTable* pHashTable)
{
    int i, count;

    for (count = i = 0; i < pHashTable->tableSize; i++) {
        if (pHashTable->pEntries[i].data == HASH_TOMBSTONE)
            count++;
    }
    return count;
}
#endif

static bool resizeHash(HashTable* pHashTable, int newSize)
{
    HashEntry* pNewEntries;
    int i;

    assert(countTombStones(pHashTable) == pHashTable->numDeadEntries);
    ALOGI("before: dead=%d", pHashTable->numDeadEntries);

    pNewEntries = (HashEntry*) calloc(newSize, sizeof(HashEntry));
    if (pNewEntries == NULL)
        return false;

    for (i = 0; i < pHashTable->tableSize; i++) {
        void* data = pHashTable->pEntries[i].data;
        if (data != NULL && data != HASH_TOMBSTONE) {
            int hashValue = pHashTable->pEntries[i].hashValue;
            int newIdx;

            newIdx = hashValue & (newSize-1);
            while (pNewEntries[newIdx].data != NULL)
                newIdx = (newIdx + 1) & (newSize-1);

            pNewEntries[newIdx].hashValue = hashValue;
            pNewEntries[newIdx].data = data;
        }
    }

    free(pHashTable->pEntries);
    pHashTable->pEntries = pNewEntries;
    pHashTable->tableSize = newSize;
    pHashTable->numDeadEntries = 0;

    assert(countTombStones(pHashTable) == 0);
    return true;
}

void* dvmHashTableLookup(HashTable* pHashTable, u4 itemHash, void* item,
    HashCompareFunc cmpFunc, bool doAdd)
{
    HashEntry* pEntry;
    HashEntry* pEnd;
    void* result = NULL;

    assert(pHashTable->tableSize > 0);
    assert(item != HASH_TOMBSTONE);
    assert(item != NULL);

    /* jump to the first entry and probe for a match */
    pEntry = &pHashTable->pEntries[itemHash & (pHashTable->tableSize-1)];
    pEnd = &pHashTable->pEntries[pHashTable->tableSize];
    while (pEntry->data != NULL) {
        if (pEntry->data != HASH_TOMBSTONE &&
            pEntry->hashValue == itemHash &&
            (*cmpFunc)(pEntry->data, item) == 0)
        {
            //ALOGD("+++ match on entry %d", pEntry - pHashTable->pEntries);
            break;
        }

        pEntry++;
        if (pEntry == pEnd) {     /* wrap around to start */
            if (pHashTable->tableSize == 1)
                break;      /* edge case - single-entry table */
            pEntry = pHashTable->pEntries;
        }

        ALOGI("+++ look probing %d...", pEntry - pHashTable->pEntries);
    }

    if (pEntry->data == NULL) {
        if (doAdd) {
            pEntry->hashValue = itemHash;
            pEntry->data = item;
            pHashTable->numEntries++;

            if ((pHashTable->numEntries+pHashTable->numDeadEntries) * LOAD_DENOM
                > pHashTable->tableSize * LOAD_NUMER)
            {
                if (!resizeHash(pHashTable, pHashTable->tableSize * 2)) {
                    ALOGE("Dalvik hash resize failure");
                    dvmAbort();
                }
            } else {
                ALOGW("okay %d/%d/%d",
                pHashTable->numEntries, pHashTable->tableSize,
                (pHashTable->tableSize * LOAD_NUMER) / LOAD_DENOM);
            }

            assert(pHashTable->numEntries < pHashTable->tableSize);
            result = item;
        } else {
            assert(result == NULL);
        }
    } else {
        result = pEntry->data;
    }

    return result;
}

bool dvmHashTableRemove(HashTable* pHashTable, u4 itemHash, void* item)
{
    HashEntry* pEntry;
    HashEntry* pEnd;

    assert(pHashTable->tableSize > 0);

    pEntry = &pHashTable->pEntries[itemHash & (pHashTable->tableSize-1)];
    pEnd = &pHashTable->pEntries[pHashTable->tableSize];
    while (pEntry->data != NULL) {
        if (pEntry->data == item) {
            //ALOGI("+++ stepping on entry %d", pEntry - pHashTable->pEntries);
            pEntry->data = HASH_TOMBSTONE;
            pHashTable->numEntries--;
            pHashTable->numDeadEntries++;
            return true;
        }

        pEntry++;
        if (pEntry == pEnd) {     /* wrap around to start */
            if (pHashTable->tableSize == 1)
                break;      /* edge case - single-entry table */
            pEntry = pHashTable->pEntries;
        }

        ALOGI("+++ del probing %d...", pEntry - pHashTable->pEntries);
    }

    return false;
}

int dvmHashForeachRemove(HashTable* pHashTable, HashForeachRemoveFunc func)
{
    int i, val, tableSize;

    tableSize = pHashTable->tableSize;

    for (i = 0; i < tableSize; i++) {
        HashEntry* pEnt = &pHashTable->pEntries[i];

        if (pEnt->data != NULL && pEnt->data != HASH_TOMBSTONE) {
            val = (*func)(pEnt->data);
            if (val == 1) {
                pEnt->data = HASH_TOMBSTONE;
                pHashTable->numEntries--;
                pHashTable->numDeadEntries++;
            }
            else if (val != 0) {
                return val;
            }
        }
    }
    return 0;
}


int dvmHashForeach(HashTable* pHashTable, HashForeachFunc func, void* arg)
{
    int i, val, tableSize;

    tableSize = pHashTable->tableSize;

    for (i = 0; i < tableSize; i++) {
        HashEntry* pEnt = &pHashTable->pEntries[i];

        if (pEnt->data != NULL && pEnt->data != HASH_TOMBSTONE) {
            val = (*func)(pEnt->data, arg);
            if (val != 0)
                return val;
        }
    }

    return 0;
}


static int countProbes(HashTable* pHashTable, u4 itemHash, const void* item,
    HashCompareFunc cmpFunc)
{
    HashEntry* pEntry;
    HashEntry* pEnd;
    int count = 0;

    assert(pHashTable->tableSize > 0);
    assert(item != HASH_TOMBSTONE);
    assert(item != NULL);

    /* jump to the first entry and probe for a match */
    pEntry = &pHashTable->pEntries[itemHash & (pHashTable->tableSize-1)];
    pEnd = &pHashTable->pEntries[pHashTable->tableSize];
    while (pEntry->data != NULL) {
        if (pEntry->data != HASH_TOMBSTONE &&
            pEntry->hashValue == itemHash &&
            (*cmpFunc)(pEntry->data, item) == 0)
        {
            /* match */
            break;
        }

        pEntry++;
        if (pEntry == pEnd) {     /* wrap around to start */
            if (pHashTable->tableSize == 1)
                break;      /* edge case - single-entry table */
            pEntry = pHashTable->pEntries;
        }

        count++;
    }
    if (pEntry->data == NULL)
        return -1;

    return count;
}

void dvmHashTableProbeCount(HashTable* pHashTable, HashCalcFunc calcFunc,
    HashCompareFunc cmpFunc)
{
    int numEntries, minProbe, maxProbe, totalProbe;
    HashIter iter;

    numEntries = maxProbe = totalProbe = 0;
    minProbe = 65536*32767;

    for (dvmHashIterBegin(pHashTable, &iter); !dvmHashIterDone(&iter);
        dvmHashIterNext(&iter))
    {
        const void* data = (const void*)dvmHashIterData(&iter);
        int count;

        count = countProbes(pHashTable, (*calcFunc)(data), data, cmpFunc);

        numEntries++;

        if (count < minProbe)
            minProbe = count;
        if (count > maxProbe)
            maxProbe = count;
        totalProbe += count;
    }

    ALOGI("Probe: min=%d max=%d, total=%d in %d (%d), avg=%.3f",
        minProbe, maxProbe, totalProbe, numEntries, pHashTable->tableSize,
        (float) totalProbe / (float) numEntries);
}
