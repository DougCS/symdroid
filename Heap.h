#ifndef HEAP
#define HEAP

bool dvmHeapStartup(void);

bool dvmHeapStartupAfterZygote(void);

void dvmHeapShutdown(void);

void dvmHeapThreadShutdown(void);

#if 0
size_t dvmObjectSizeInHeap(const Object *obj);
#endif

typedef enum {
    GC_FULL,
    GC_PARTIAL
} GcMode;

typedef enum {
    GC_FOR_MALLOC,
    GC_CONCURRENT,
    GC_EXPLICIT,
    GC_EXTERNAL_ALLOC,
    GC_HPROF_DUMP_HEAP
} GcReason;

void dvmCollectGarbageInternal(bool clearSoftRefs, GcReason reason);

void dvmWaitForConcurrentGcToComplete(void);

#endif
