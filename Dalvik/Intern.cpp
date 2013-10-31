#include "dalvik/Dalvik.h"

bool dvmStringInternStartup()
{
    dvmInitMutex(&gDvm.internLock);
    gDvm.internedStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.internedStrings == NULL)
        return false;
    gDvm.literalStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.literalStrings == NULL)
        return false;
    return true;
}

void dvmStringInternShutdown()
{
    if (gDvm.internedStrings != NULL || gDvm.literalStrings != NULL) {
        dvmDestroyMutex(&gDvm.internLock);
    }
    dvmHashTableFree(gDvm.internedStrings);
    gDvm.internedStrings = NULL;
    dvmHashTableFree(gDvm.literalStrings);
    gDvm.literalStrings = NULL;
}

static StringObject* lookupString(HashTable* table, u4 key, StringObject* value)
{
    void* entry = dvmHashTableLookup(table, key, (void*)value,
                                     dvmHashcmpStrings, false);
    return (StringObject*)entry;
}

static StringObject* insertString(HashTable* table, u4 key, StringObject* value)
{
    if (dvmIsNonMovingObject(value) == false) {
        value = (StringObject*)dvmCloneObject(value, ALLOC_NON_MOVING);
    }
    void* entry = dvmHashTableLookup(table, key, (void*)value,
                                     dvmHashcmpStrings, true);
    return (StringObject*)entry;
}

static StringObject* lookupInternedString(StringObject* strObj, bool isLiteral)
{
    StringObject* found;

    assert(strObj != NULL);
    u4 key = dvmComputeStringHash(strObj);
    dvmLockMutex(&gDvm.internLock);
    if (isLiteral) {
        StringObject* literal = lookupString(gDvm.literalStrings, key, strObj);
        if (literal != NULL) {
            found = literal;
        } else {
            StringObject* interned = lookupString(gDvm.internedStrings, key, strObj);
            if (interned != NULL) {
                dvmHashTableRemove(gDvm.internedStrings, key, interned);
                found = insertString(gDvm.literalStrings, key, interned);
                assert(found == interned);
            } else {
                found = insertString(gDvm.literalStrings, key, strObj);
                assert(found == strObj);
            }
        }
    } else {
        found = lookupString(gDvm.literalStrings, key, strObj);
        if (found == NULL) {
            found = insertString(gDvm.internedStrings, key, strObj);
        }
    }
    assert(found != NULL);
    dvmUnlockMutex(&gDvm.internLock);
    return found;
}

StringObject* dvmLookupInternedString(StringObject* strObj)
{
    return lookupInternedString(strObj, false);
}

StringObject* dvmLookupImmortalInternedString(StringObject* strObj)
{
    return lookupInternedString(strObj, true);
}

bool dvmIsWeakInternedString(StringObject* strObj)
{
    assert(strObj != NULL);
    if (gDvm.internedStrings == NULL) {
        return false;
    }
    dvmLockMutex(&gDvm.internLock);
    u4 key = dvmComputeStringHash(strObj);
    StringObject* found = lookupString(gDvm.internedStrings, key, strObj);
    dvmUnlockMutex(&gDvm.internLock);
    return found == strObj;
}

void dvmGcDetachDeadInternedStrings(int (*isUnmarkedObject)(void *))
{
    if (gDvm.internedStrings != NULL) {
        dvmLockMutex(&gDvm.internLock);
        dvmHashForeachRemove(gDvm.internedStrings, isUnmarkedObject);
        dvmUnlockMutex(&gDvm.internLock);
    }
}
