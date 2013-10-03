struct Thread;

int os_raiseThreadPriority();

void os_lowerThreadPriority(int oldThreadPriority);

void os_changeThreadPriority(Thread* thread, int newPriority);

int os_getThreadPriorityFromSystem();
