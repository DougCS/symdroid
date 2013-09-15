#include "Dalvik.h"

#include <stdlib.h>
#include <strings.h>

#define kBitVectorGrowth    4


BitVector* dvmAllocBitVector(unsigned int startBits, bool expandable)
{
    BitVector* bv;
    unsigned int count;

    assert(sizeof(bv->storage[0]) == 4);

    bv = (BitVector*) malloc(sizeof(BitVector));

    count = (startBits + 31) >> 5;

    bv->storageSize = count;
    bv->expandable = expandable;
    bv->storage = (u4*) calloc(count, sizeof(u4));
    return bv;
}

void dvmFreeBitVector(BitVector* pBits)
{
    if (pBits == NULL)
        return;

    free(pBits->storage);
    free(pBits);
}

int dvmAllocBit(BitVector* pBits)
{
    unsigned int word, bit;

retry:
    for (word = 0; word < pBits->storageSize; word++) {
        if (pBits->storage[word] != 0xffffffff) {
            bit = ffs(~(pBits->storage[word])) -1;
            assert(bit < 32);
            pBits->storage[word] |= 1 << bit;
            return (word << 5) | bit;
        }
    }

    if (!pBits->expandable)
        return -1;

    pBits->storage = (u4*)realloc(pBits->storage,
                    (pBits->storageSize + kBitVectorGrowth) * sizeof(u4));
    memset(&pBits->storage[pBits->storageSize], 0x00,
        kBitVectorGrowth * sizeof(u4));
    pBits->storageSize += kBitVectorGrowth;
    goto retry;
}

void dvmSetBit(BitVector* pBits, unsigned int num)
{
    if (num >= pBits->storageSize * sizeof(u4) * 8) {
        if (!pBits->expandable) {
            ALOGE("Attempt to set bit outside valid range (%d, limit is %d)",
                num, pBits->storageSize * sizeof(u4) * 8);
            dvmAbort();
        }

        unsigned int newSize = (num + 1 + 31) >> 5;
        assert(newSize > pBits->storageSize);
        pBits->storage = (u4*)realloc(pBits->storage, newSize * sizeof(u4));
        if (pBits->storage == NULL) {
            ALOGE("BitVector expansion to %d failed", newSize * sizeof(u4));
            dvmAbort();
        }
        memset(&pBits->storage[pBits->storageSize], 0x00,
            (newSize - pBits->storageSize) * sizeof(u4));
        pBits->storageSize = newSize;
    }

    pBits->storage[num >> 5] |= 1 << (num & 0x1f);
}

void dvmClearBit(BitVector* pBits, unsigned int num)
{
    assert(num < pBits->storageSize * sizeof(u4) * 8);

    pBits->storage[num >> 5] &= ~(1 << (num & 0x1f));
}

void dvmClearAllBits(BitVector* pBits)
{
    unsigned int count = pBits->storageSize;
    memset(pBits->storage, 0, count * sizeof(u4));
}

void dvmSetInitialBits(BitVector* pBits, unsigned int numBits)
{
    unsigned int idx;
    assert(((numBits + 31) >> 5) <= pBits->storageSize);
    for (idx = 0; idx < (numBits >> 5); idx++) {
        pBits->storage[idx] = -1;
    }
    unsigned int remNumBits = numBits & 0x1f;
    if (remNumBits) {
        pBits->storage[idx] = (1 << remNumBits) - 1;
    }
}

bool dvmIsBitSet(const BitVector* pBits, unsigned int num)
{
    assert(num < pBits->storageSize * sizeof(u4) * 8);

    unsigned int val = pBits->storage[num >> 5] & (1 << (num & 0x1f));
    return (val != 0);
}

int dvmCountSetBits(const BitVector* pBits)
{
    unsigned int word;
    unsigned int count = 0;

    for (word = 0; word < pBits->storageSize; word++) {
        u4 val = pBits->storage[word];

        if (val != 0) {
            if (val == 0xffffffff) {
                count += 32;
            } else {
                while (val != 0) {
                    val &= val - 1;
                    count++;
                }
            }
        }
    }

    return count;
}

static void checkSizes(const BitVector* bv1, const BitVector* bv2)
{
    if (bv1->storageSize != bv2->storageSize) {
        ALOGE("Mismatched vector sizes (%d, %d)",
            bv1->storageSize, bv2->storageSize);
        dvmAbort();
    }
}

void dvmCopyBitVector(BitVector *dest, const BitVector *src)
{
    checkSizes(dest, src);

    memcpy(dest->storage, src->storage, sizeof(u4) * dest->storageSize);
}

bool dvmIntersectBitVectors(BitVector *dest, const BitVector *src1,
                            const BitVector *src2)
{
    if (dest->storageSize != src1->storageSize ||
        dest->storageSize != src2->storageSize ||
        dest->expandable != src1->expandable ||
        dest->expandable != src2->expandable)
        return false;

    unsigned int idx;
    for (idx = 0; idx < dest->storageSize; idx++) {
        dest->storage[idx] = src1->storage[idx] & src2->storage[idx];
    }
    return true;
}

bool dvmUnifyBitVectors(BitVector *dest, const BitVector *src1,
                        const BitVector *src2)
{
    if (dest->storageSize != src1->storageSize ||
        dest->storageSize != src2->storageSize ||
        dest->expandable != src1->expandable ||
        dest->expandable != src2->expandable)
        return false;

    unsigned int idx;
    for (idx = 0; idx < dest->storageSize; idx++) {
        dest->storage[idx] = src1->storage[idx] | src2->storage[idx];
    }
    return true;
}

bool dvmCompareBitVectors(const BitVector *src1, const BitVector *src2)
{
    if (src1->storageSize != src2->storageSize ||
        src1->expandable != src2->expandable)
        return true;

    unsigned int idx;
    for (idx = 0; idx < src1->storageSize; idx++) {
        if (src1->storage[idx] != src2->storage[idx]) return true;
    }
    return false;
}

void dvmBitVectorIteratorInit(BitVector* pBits, BitVectorIterator* iterator)
{
    iterator->pBits = pBits;
    iterator->bitSize = pBits->storageSize * sizeof(u4) * 8;
    iterator->idx = 0;
}

int dvmBitVectorIteratorNext(BitVectorIterator* iterator)
{
    const BitVector* pBits = iterator->pBits;
    u4 bitIndex = iterator->idx;

    assert(iterator->bitSize == pBits->storageSize * sizeof(u4) * 8);
    if (bitIndex >= iterator->bitSize) return -1;

    for (; bitIndex < iterator->bitSize; bitIndex++) {
        unsigned int wordIndex = bitIndex >> 5;
        unsigned int mask = 1 << (bitIndex & 0x1f);
        if (pBits->storage[wordIndex] & mask) {
            iterator->idx = bitIndex+1;
            return bitIndex;
        }
    }
    return -1;
}


bool dvmCheckMergeBitVectors(BitVector* dst, const BitVector* src)
{
    bool changed = false;

    checkSizes(dst, src);

    unsigned int idx;
    for (idx = 0; idx < dst->storageSize; idx++) {
        u4 merged = src->storage[idx] | dst->storage[idx];
        if (dst->storage[idx] != merged) {
            dst->storage[idx] = merged;
            changed = true;
        }
    }

    return changed;
}
