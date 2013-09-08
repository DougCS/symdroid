#ifndef DALVIK_ALLOCTRACKER_H_
#define DALVIK_ALLOCTRACKER_H_

bool dvmAllocTrackerStartup(void);
void dvmAllocTrackerShutdown(void);

struct AllocRecord;

bool dvmEnableAllocTracker(void);

void dvmDisableAllocTracker(void);

#define dvmTrackAllocation(_clazz, _size)
    {
        if (gDvm.allocRecords != NULL)
            dvmDoTrackAllocation(_clazz, _size);
    }
void dvmDoTrackAllocation(ClassObject* clazz, size_t size);

bool dvmGenerateTrackedAllocationReport(u1** pData, size_t* pDataLen);

void dvmDumpTrackedAllocations(bool enable);

#endif  // DALVIK_ALLOCTRACKER_H_
