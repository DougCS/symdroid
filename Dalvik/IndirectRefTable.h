#ifndef DALVIK_INDIRECTREFTABLE_H_
#define DALVIK_INDIRECTREFTABLE_H_

typedef void* IndirectRef;

#define kInvalidIndirectRefObject reinterpret_cast<Object*>(0xdead4321)

#define kClearedJniWeakGlobal reinterpret_cast<Object*>(0xdead1234)

enum IndirectRefKind {
    kIndirectKindInvalid    = 0,
    kIndirectKindLocal      = 1,
    kIndirectKindGlobal     = 2,
    kIndirectKindWeakGlobal = 3
};
const char* indirectRefKindToString(IndirectRefKind kind);

INLINE IndirectRefKind indirectRefKind(IndirectRef iref)
{
    return (IndirectRefKind)((u4) iref & 0x03);
}

struct IndirectRefSlot {
    Object* obj;
    u4      serial;
};

#define IRT_FIRST_SEGMENT   0

union IRTSegmentState {
    u4          all;
    struct {
        u4      topIndex:16;            /* index of first unused entry */
        u4      numHoles:16;            /* #of holes in entire table */
    } parts;
};

class iref_iterator {
public:
    explicit iref_iterator(IndirectRefSlot* table, size_t i, size_t capacity) :
            table_(table), i_(i), capacity_(capacity) {
        skipNullsAndTombstones();
    }

    iref_iterator& operator++() {
        ++i_;
        skipNullsAndTombstones();
        return *this;
    }

    Object** operator*() {
        return &table_[i_].obj;
    }

    bool equals(const iref_iterator& rhs) const {
        return (i_ == rhs.i_ && table_ == rhs.table_);
    }

private:
    void skipNullsAndTombstones() {
        // We skip NULLs and tombstones. Clients don't want to see implementation details.
        while (i_ < capacity_ && (table_[i_].obj == NULL
                || table_[i_].obj == kClearedJniWeakGlobal)) {
            ++i_;
        }
    }

    IndirectRefSlot* table_;
    size_t i_;
    size_t capacity_;
};

bool inline operator!=(const iref_iterator& lhs, const iref_iterator& rhs) {
    return !lhs.equals(rhs);
}

struct IndirectRefTable {
public:
    typedef iref_iterator iterator;

    IRTSegmentState segmentState;

    IndirectRefSlot* table_;
    IndirectRefKind kind_;
    size_t          alloc_entries_;
    size_t          max_entries_;


    IndirectRef add(u4 cookie, Object* obj);

    Object* get(IndirectRef iref) const;

    bool contains(const Object* obj) const;

    bool remove(u4 cookie, IndirectRef iref);

    bool init(size_t initialCount, size_t maxCount, IndirectRefKind kind);

    void destroy();

    void dump(const char* descr) const;

    size_t capacity() const {
        return segmentState.parts.topIndex;
    }

    iterator begin() {
        return iterator(table_, 0, capacity());
    }

    iterator end() {
        return iterator(table_, capacity(), capacity());
    }

private:
    static inline u4 extractIndex(IndirectRef iref) {
        u4 uref = (u4) iref;
        return (uref >> 2) & 0xffff;
    }

    static inline u4 extractSerial(IndirectRef iref) {
        u4 uref = (u4) iref;
        return uref >> 20;
    }

    static inline u4 nextSerial(u4 serial) {
        return (serial + 1) & 0xfff;
    }

    static inline IndirectRef toIndirectRef(u4 index, u4 serial, IndirectRefKind kind) {
        assert(index < 65536);
        return reinterpret_cast<IndirectRef>((serial << 20) | (index << 2) | kind);
    }
};

#endif
