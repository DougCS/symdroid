#ifndef DALVIK_ATOMICCACHE_H_
#define DALVIK_ATOMICCACHE_H_

#define CALC_CACHE_STATS 0


struct AtomicCacheEntry {
    u4          key1;
    u4          key2;
    u4          value;
    volatile u4 version;
};

struct AtomicCache {
    AtomicCacheEntry*   entries;
    int         numEntries;

    void*       entryAlloc;

    int         trivial;
    int         fail;
    int         hits;
    int         misses;
    int         fills;
};

#if CALC_CACHE_STATS > 0
# define CACHE_XARG(_value) ,_value
#else
# define CACHE_XARG(_value)
#endif
#define ATOMIC_CACHE_LOOKUP(_cache, _cacheSize, _key1, _key2) ({
    AtomicCacheEntry* pEntry;
    int hash;
    u4 firstVersion, secondVersion;
    u4 value;
    hash = (((u4)(_key1) >> 2) ^ (u4)(_key2)) & ((_cacheSize)-1);
    pEntry = (_cache)->entries + hash;
    firstVersion = android_atomic_acquire_load((int32_t*)&pEntry->version);
    if (pEntry->key1 == (u4)(_key1) && pEntry->key2 == (u4)(_key2)) {
        value = android_atomic_acquire_load((int32_t*) &pEntry->value);
        secondVersion = pEntry->version;
        if ((firstVersion & 0x01) != 0 || firstVersion != secondVersion) {
            if (CALC_CACHE_STATS)
                (_cache)->fail++;
            value = (u4) ATOMIC_CACHE_CALC;
        } else {
            if (CALC_CACHE_STATS)
                (_cache)->hits++;
        }
    } else {
        value = (u4) ATOMIC_CACHE_CALC;
        if (value == 0 && ATOMIC_CACHE_NULL_ALLOWED) {
            dvmUpdateAtomicCache((u4) (_key1), (u4) (_key2), value, pEntry,
                        firstVersion CACHE_XARG(_cache) );
        }
    }
    value;
})

AtomicCache* dvmAllocAtomicCache(int numEntries);

void dvmFreeAtomicCache(AtomicCache* cache);

void dvmUpdateAtomicCache(u4 key1, u4 key2, u4 value, AtomicCacheEntry* pEntry,
    u4 firstVersion
#if CALC_CACHE_STATS > 0
    , AtomicCache* pCache
#endif
    );

void dvmDumpAtomicCacheStats(const AtomicCache* pCache);

#endif  // DALVIK_ATOMICCACHE_H_
