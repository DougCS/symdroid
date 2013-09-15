#ifndef DALVIK_BITVECTOR_H_
#define DALVIK_BITVECTOR_H_

struct BitVector {
    bool    expandable;
    u4      storageSize;
    u4*     storage;
};

struct BitVectorIterator {
    BitVector *pBits;
    u4 idx;
    u4 bitSize;
};

BitVector* dvmAllocBitVector(unsigned int startBits, bool expandable);
void dvmFreeBitVector(BitVector* pBits);

int dvmAllocBit(BitVector* pBits);
void dvmSetBit(BitVector* pBits, unsigned int num);
void dvmClearBit(BitVector* pBits, unsigned int num);
void dvmClearAllBits(BitVector* pBits);
void dvmSetInitialBits(BitVector* pBits, unsigned int numBits);
bool dvmIsBitSet(const BitVector* pBits, unsigned int num);

int dvmCountSetBits(const BitVector* pBits);

void dvmCopyBitVector(BitVector *dest, const BitVector *src);

bool dvmIntersectBitVectors(BitVector *dest, const BitVector *src1,
                            const BitVector *src2);

bool dvmUnifyBitVectors(BitVector *dest, const BitVector *src1,
                        const BitVector *src2);

bool dvmCheckMergeBitVectors(BitVector* dst, const BitVector* src);

bool dvmCompareBitVectors(const BitVector *src1, const BitVector *src2);

void dvmBitVectorIteratorInit(BitVector* pBits, BitVectorIterator* iterator);

int dvmBitVectorIteratorNext(BitVectorIterator* iterator);

#endif
