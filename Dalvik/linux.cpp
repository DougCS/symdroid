#include "dalvik/os.h"

#include "dalvik/Dalvik.h"

int os_raiseThreadPriority()
{
    return 0;
}

void os_lowerThreadPriority(int oldThreadPriority)
{
    // Does nothing
}

void os_changeThreadPriority(Thread* thread, int newPriority)
{
    // Does nothing
}

int os_getThreadPriorityFromSystem()
{
    return THREAD_NORM_PRIORITY;
}
