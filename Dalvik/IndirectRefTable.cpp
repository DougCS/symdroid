#include "dalvik/Dalvik.h"

static void abortMaybe() {
    if (!gDvmJni.useCheckJni) {
        dvmAbort();
    }
}

bool IndirectRefTable::init(size_t initialCount,
        size_t maxCount, IndirectRefKind desiredKind)
{
    assert(initialCount > 0);
    assert(initialCount <= maxCount);
    assert(desiredKind != kIndirectKindInvalid);

    table_ = (IndirectRefSlot*) malloc(initialCount * sizeof(IndirectRefSlot));
    if (table_ == NULL) {
        return false;
    }
    memset(table_, 0xd1, initialCount * sizeof(IndirectRefSlot));

    segmentState.all = IRT_FIRST_SEGMENT;
    alloc_entries_ = initialCount;
    max_entries_ = maxCount;
    kind_ = desiredKind;

    return true;
}

void IndirectRefTable::destroy()
{
    free(table_);
    table_ = NULL;
    alloc_entries_ = max_entries_ = -1;
}

IndirectRef IndirectRefTable::add(u4 cookie, Object* obj)
{
    IRTSegmentState prevState;
    prevState.all = cookie;
    size_t topIndex = segmentState.parts.topIndex;

    assert(obj != NULL);
    assert(dvmIsHeapAddress(obj));
    assert(table_ != NULL);
    assert(alloc_entries_ <= max_entries_);
    assert(segmentState.parts.numHoles >= prevState.parts.numHoles);

    IndirectRef result;
    IndirectRefSlot* slot;
    int numHoles = segmentState.parts.numHoles - prevState.parts.numHoles;
    if (numHoles > 0) {
        assert(topIndex > 1);
        slot = &table_[topIndex - 1];
        assert(slot->obj != NULL);
        while ((--slot)->obj != NULL) {
            assert(slot >= table_ + prevState.parts.topIndex);
        }
        segmentState.parts.numHoles--;
    } else {
        /* add to the end, grow if needed */
        if (topIndex == alloc_entries_) {
            if (topIndex == max_entries_) {
                ALOGE("JNI ERROR (app bug): %s reference table overflow (max=%d)",
                        indirectRefKindToString(kind_), max_entries_);
                return NULL;
            }

            size_t newSize = alloc_entries_ * 2;
            if (newSize > max_entries_) {
                newSize = max_entries_;
            }
            assert(newSize > alloc_entries_);

            IndirectRefSlot* newTable =
                    (IndirectRefSlot*) realloc(table_, newSize * sizeof(IndirectRefSlot));
            if (table_ == NULL) {
                ALOGE("JNI ERROR (app bug): unable to expand %s reference table "
                        "(from %d to %d, max=%d)",
                        indirectRefKindToString(kind_),
                        alloc_entries_, newSize, max_entries_);
                return NULL;
            }

            memset(newTable + alloc_entries_, 0xd1,
                   (newSize - alloc_entries_) * sizeof(IndirectRefSlot));

            alloc_entries_ = newSize;
            table_ = newTable;
        }
        slot = &table_[topIndex++];
        segmentState.parts.topIndex = topIndex;
    }

    slot->obj = obj;
    slot->serial = nextSerial(slot->serial);
    result = toIndirectRef(slot - table_, slot->serial, kind_);

    assert(result != NULL);
    return result;
}

Object* IndirectRefTable::get(IndirectRef iref) const {
    IndirectRefKind kind = indirectRefKind(iref);
    if (kind != kind_) {
        if (iref == NULL) {
            ALOGW("Attempt to look up NULL %s reference", indirectRefKindToString(kind_));
            return kInvalidIndirectRefObject;
        }
        if (kind == kIndirectKindInvalid) {
            ALOGE("JNI ERROR (app bug): invalid %s reference %p",
                    indirectRefKindToString(kind_), iref);
            abortMaybe();
            return kInvalidIndirectRefObject;
        }
        return kInvalidIndirectRefObject;
    }

    u4 topIndex = segmentState.parts.topIndex;
    u4 index = extractIndex(iref);
    if (index >= topIndex) {
        ALOGE("JNI ERROR (app bug): accessed stale %s reference %p (index %d in a table of size %d)",
                indirectRefKindToString(kind_), iref, index, topIndex);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    Object* obj = table_[index].obj;
    if (obj == NULL) {
        ALOGI("JNI ERROR (app bug): accessed deleted %s reference %p",
                indirectRefKindToString(kind_), iref);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    u4 serial = extractSerial(iref);
    if (serial != table_[index].serial) {
        ALOGE("JNI ERROR (app bug): attempt to use stale %s reference %p",
                indirectRefKindToString(kind_), iref);
        abortMaybe();
        return kInvalidIndirectRefObject;
    }

    return obj;
}

static int findObject(const Object* obj, int bottomIndex, int topIndex,
        const IndirectRefSlot* table) {
    for (int i = bottomIndex; i < topIndex; ++i) {
        if (table[i].obj == obj) {
            return i;
        }
    }
    return -1;
}

bool IndirectRefTable::contains(const Object* obj) const {
    return findObject(obj, 0, segmentState.parts.topIndex, table_) >= 0;
}

bool IndirectRefTable::remove(u4 cookie, IndirectRef iref)
{
    IRTSegmentState prevState;
    prevState.all = cookie;
    u4 topIndex = segmentState.parts.topIndex;
    u4 bottomIndex = prevState.parts.topIndex;

    assert(table_ != NULL);
    assert(alloc_entries_ <= max_entries_);
    assert(segmentState.parts.numHoles >= prevState.parts.numHoles);

    IndirectRefKind kind = indirectRefKind(iref);
    u4 index;
    if (kind == kind_) {
        index = extractIndex(iref);
        if (index < bottomIndex) {
            ALOGV("Attempt to remove index outside index area (%ud vs %ud-%ud)",
                    index, bottomIndex, topIndex);
            return false;
        }
        if (index >= topIndex) {
            ALOGD("Attempt to remove invalid index %ud (bottom=%ud top=%ud)",
                    index, bottomIndex, topIndex);
            return false;
        }
        if (table_[index].obj == NULL) {
            ALOGD("Attempt to remove cleared %s reference %p",
                    indirectRefKindToString(kind_), iref);
            return false;
        }
        u4 serial = extractSerial(iref);
        if (table_[index].serial != serial) {
            ALOGD("Attempt to remove stale %s reference %p",
                    indirectRefKindToString(kind_), iref);
            return false;
        }
    } else if (kind == kIndirectKindInvalid && gDvmJni.workAroundAppJniBugs) {
        int i = findObject(reinterpret_cast<Object*>(iref), bottomIndex, topIndex, table_);
        if (i < 0) {
            ALOGW("trying to work around app JNI bugs, but didn't find %p in table!", iref);
            return false;
        }
        index = i;
    } else {
        return false;
    }

    if (index == topIndex - 1) {
        int numHoles = segmentState.parts.numHoles - prevState.parts.numHoles;
        if (numHoles != 0) {
            while (--topIndex > bottomIndex && numHoles != 0) {
                ALOGV("+++ checking for hole at %d (cookie=0x%08x) val=%p",
                    topIndex-1, cookie, table_[topIndex-1].obj);
                if (table_[topIndex-1].obj != NULL) {
                    break;
                }
                ALOGV("+++ ate hole at %d", topIndex-1);
                numHoles--;
            }
            segmentState.parts.numHoles = numHoles + prevState.parts.numHoles;
            segmentState.parts.topIndex = topIndex;
        } else {
            segmentState.parts.topIndex = topIndex-1;
            ALOGV("+++ ate last entry %d", topIndex-1);
        }
    } else {
        table_[index].obj = NULL;
        segmentState.parts.numHoles++;
        ALOGV("+++ left hole at %d, holes=%d", index, segmentState.parts.numHoles);
    }

    return true;
}

const char* indirectRefKindToString(IndirectRefKind kind)
{
    switch (kind) {
    case kIndirectKindInvalid:      return "invalid";
    case kIndirectKindLocal:        return "local";
    case kIndirectKindGlobal:       return "global";
    case kIndirectKindWeakGlobal:   return "weak global";
    default:                        return "UNKNOWN";
    }
}

void IndirectRefTable::dump(const char* descr) const
{
    size_t count = capacity();
    Object** copy = new Object*[count];
    for (size_t i = 0; i < count; i++) {
        copy[i] = table_[i].obj;
    }
    dvmDumpReferenceTableContents(copy, count, descr);
    delete[] copy;
}
