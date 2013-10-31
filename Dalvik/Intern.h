#ifndef DALVIK_INTERN_H_
#define DALVIK_INTERN_H_

bool dvmStringInternStartup(void);
void dvmStringInternShutdown(void);
StringObject* dvmLookupInternedString(StringObject* strObj);
StringObject* dvmLookupImmortalInternedString(StringObject* strObj);
bool dvmIsWeakInternedString(StringObject* strObj);
void dvmGcDetachDeadInternedStrings(int (*isUnmarkedObject)(void *));

#endif
