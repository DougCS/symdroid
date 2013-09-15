#define CPU_CACHE_WIDTH         32
#define CPU_CACHE_WIDTH_1       (CPU_CACHE_WIDTH-1)

#define ATOMIC_LOCK_FLAG        (1 << 31)

AtomicCache* dvmAllocAtomicCache(int numEntries)
{
    AtomicCache* newCache;

    newCache = (AtomicCache*) calloc(1, sizeof(AtomicCache));
    if (newCache == NULL)
        return NULL;

    newCache->numEntries = numEntries;

    newCache->entryAlloc = calloc(1,
        sizeof(AtomicCacheEntry) * numEntries + CPU_CACHE_WIDTH);
    if (newCache->entryAlloc == NULL) {
        free(newCache);
        return NULL;
    }

    assert(sizeof(AtomicCacheEntry) == 16);
    newCache->entries = (AtomicCacheEntry*)
        (((int) newCache->entryAlloc + CPU_CACHE_WIDTH_1) & ~CPU_CACHE_WIDTH_1);

    return newCache;
}

void dvmFreeAtomicCache(AtomicCache* cache)
{
    if (cache != NULL) {
        free(cache->entryAlloc);
        free(cache);
    }
}


void dvmUpdateAtomicCache(u4 key1, u4 key2, u4 value, AtomicCacheEntry* pEntry,
    u4 firstVersion
#if CALC_CACHE_STATS > 0
    , AtomicCache* pCache
#endif
    )
{
    if ((firstVersion & ATOMIC_LOCK_FLAG) != 0 ||
        android_atomic_release_cas(
                firstVersion, firstVersion | ATOMIC_LOCK_FLAG,
                (volatile s4*) &pEntry->version) != 0)
    {
#if CALC_CACHE_STATS > 0
        pCache->fail++;
#endif
        return;
    }

    assert((firstVersion & 0x01) == 0);

#if CALC_CACHE_STATS > 0
    if (pEntry->key1 == 0)
        pCache->fills++;
    else
        pCache->misses++;
#endif

    u4 newVersion = (firstVersion | ATOMIC_LOCK_FLAG) + 1;
    assert((newVersion & 0x01) == 1);

    pEntry->version = newVersion;

    android_atomic_release_store(key1, (int32_t*) &pEntry->key1);
    pEntry->key2 = key2;
    pEntry->value = value;

    newVersion++;
    android_atomic_release_store(newVersion, (int32_t*) &pEntry->version);

    assert(newVersion == ((firstVersion + 2) | ATOMIC_LOCK_FLAG));
    if (android_atomic_release_cas(
            newVersion, newVersion & ~ATOMIC_LOCK_FLAG,
            (volatile s4*) &pEntry->version) != 0)
    {
        //ALOGE("unable to reset the instanceof cache ownership");
        dvmAbort();
    }
}


void dvmDumpAtomicCacheStats(const AtomicCache* pCache)
{
    if (pCache == NULL)
        return;
    dvmFprintf(stdout,
        "Cache stats: trv=%d fai=%d hit=%d mis=%d fil=%d %d%% (size=%d)\n",
        pCache->trivial, pCache->fail, pCache->hits,
        pCache->misses, pCache->fills,
        (pCache->hits == 0) ? 0 :
            pCache->hits * 100 /
                (pCache->fail + pCache->hits + pCache->misses + pCache->fills),
        pCache->numEntries);
}
