typedef u4 (*HashCompute)(const void* item);

typedef int (*HashCompareFunc)(const void* tableItem, const void* looseItem);

typedef void (*HashFreeFunc)(void* ptr);

typedef int (*HashForeachFunc)(void* data, void* arg);

typedef int (*HashForeachRemoveFunc)(void* data);

struct HashEntry {
    u4 hashValue;
    void* data;
};

#define HASH_TOMBSTONE ((void*) 0xcbcacccd)     // invalid ptr value

struct HashTable {
    int         tableSize;
    int         numEntries;
    int         numDeadEntries;
    HashEntry*  pEntries;
    HashFreeFunc freeFunc;
    pthread_mutex_t lock;
};

HashTable* dvmHashTableCreate(size_t initialSize, HashFreeFunc freeFunc);

size_t dvmHashSize(size_t size);

void dvmHashTableClear(HashTable* pHashTable);

void dvmHashTableFree(HashTable* pHashTable);

INLINE void dvmHashTableLock(HashTable* pHashTable) {
    dvmLockMutex(&pHashTable->lock);
}
INLINE void dvmHashTableUnlock(HashTable* pHashTable) {
    dvmUnlockMutex(&pHashTable->lock);
}

INLINE int dvmHashTableNumEntries(HashTable* pHashTable) {
    return pHashTable->numEntries;
}

INLINE int dvmHashTableMemUsage(HashTable* pHashTable) {
    return sizeof(HashTable) + pHashTable->tableSize * sizeof(HashEntry);
}

void* dvmHashTableLookup(HashTable* pHashTable, u4 itemHash, void* item,
    HashCompareFunc cmpFunc, bool doAdd);

bool dvmHashTableRemove(HashTable* pHashTable, u4 hash, void* item);

int dvmHashForeach(HashTable* pHashTable, HashForeachFunc func, void* arg);

int dvmHashForeachRemove(HashTable* pHashTable, HashForeachRemoveFunc func);

Use like this:
    HashIter iter;
for (dvmHashIterBegin(hashTable, &iter); !dvmHashIterDone(&iter);
    dvmHashIterNext(&iter))
{
    MyData* data = (MyData*)dvmHashIterData(&iter);
}

struct HashIter {
    void*       data;
    HashTable*  pHashTable;
    int         idx;
};
INLINE void dvmHashIterNext(HashIter* pIter) {
    int i = pIter->idx +1;
    int lim = pIter->pHashTable->tableSize;
    for ( ; i < lim; i++) {
        void* data = pIter->pHashTable->pEntries[i].data;
        if (data != NULL && data != HASH_TOMBSTONE)
            break;
    }
    pIter->idx = i;
}
INLINE void dvmHashIterBegin(HashTable* pHashTable, HashIter* pIter) {
    pIter->pHashTable = pHashTable;
    pIter->idx = -1;
    dvmHashIterNext(pIter);
}
INLINE bool dvmHashIterDone(HashIter* pIter) {
    return (pIter->idx >= pIter->pHashTable->tableSize);
}
INLINE void* dvmHashIterData(HashIter* pIter) {
    assert(pIter->idx >= 0 && pIter->idx < pIter->pHashTable->tableSize);
    return pIter->pHashTable->pEntries[pIter->idx].data;
}


typedef u4 (*HashCalcFunc)(const void* item);
void dvmHashTableProbeCount(HashTable* pHashTable, HashCalcFunc calcFunc,
    HashCompareFunc cmpFunc);

#endif
