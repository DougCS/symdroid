#ifndef DALVIK_BITS_H_
#define DALVIK_BITS_H_

#include "Common.h"
#include "Inlines.h"

#include <stdlib.h>
#include <string.h>

INLINE u1 get1(unsigned const char* pSrc)
{
    return *pSrc;
}

INLINE u2 get2BE(unsigned char const* pSrc)
{
    return (pSrc[0] << 8) | pSrc[1];
}

INLINE u4 get4BE(unsigned char const* pSrc)
{
    return (pSrc[0] << 24) | (pSrc[1] << 16) | (pSrc[2] << 8) | pSrc[3];
}

INLINE u8 get8BE(unsigned char const* pSrc)
{
    u4 low, high;

    high = pSrc[0];
    high = (high << 8) | pSrc[1];
    high = (high << 8) | pSrc[2];
    high = (high << 8) | pSrc[3];
    low = pSrc[4];
    low = (low << 8) | pSrc[5];
    low = (low << 8) | pSrc[6];
    low = (low << 8) | pSrc[7];

    return ((u8) high << 32) | (u8) low;
}

INLINE u2 get2LE(unsigned char const* pSrc)
{
    return pSrc[0] | (pSrc[1] << 8);
}

INLINE u4 get4LE(unsigned char const* pSrc)
{
    u4 result;

    result = pSrc[0];
    result |= pSrc[1] << 8;
    result |= pSrc[2] << 16;
    result |= pSrc[3] << 24;

    return result;
}

INLINE u8 get8LE(unsigned char const* pSrc)
{
    u4 low, high;

    low = pSrc[0];
    low |= pSrc[1] << 8;
    low |= pSrc[2] << 16;
    low |= pSrc[3] << 24;
    high = pSrc[4];
    high |= pSrc[5] << 8;
    high |= pSrc[6] << 16;
    high |= pSrc[7] << 24;
    return ((u8) high << 32) | (u8) low;
}

INLINE u1 read1(unsigned const char** ppSrc)
{
    return *(*ppSrc)++;
}

INLINE u2 read2BE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;

    *ppSrc = pSrc + 2;
    return pSrc[0] << 8 | pSrc[1];
}

INLINE u4 read4BE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;
    u4 result;

    result = pSrc[0] << 24;
    result |= pSrc[1] << 16;
    result |= pSrc[2] << 8;
    result |= pSrc[3];

    *ppSrc = pSrc + 4;
    return result;
}

INLINE u8 read8BE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;
    u4 low, high;

    high = pSrc[0];
    high = (high << 8) | pSrc[1];
    high = (high << 8) | pSrc[2];
    high = (high << 8) | pSrc[3];
    low = pSrc[4];
    low = (low << 8) | pSrc[5];
    low = (low << 8) | pSrc[6];
    low = (low << 8) | pSrc[7];

    *ppSrc = pSrc + 8;
    return ((u8) high << 32) | (u8) low;
}

INLINE u2 read2LE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;
    *ppSrc = pSrc + 2;
    return pSrc[0] | pSrc[1] << 8;
}

INLINE u4 read4LE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;
    u4 result;

    result = pSrc[0];
    result |= pSrc[1] << 8;
    result |= pSrc[2] << 16;
    result |= pSrc[3] << 24;

    *ppSrc = pSrc + 4;
    return result;
}

INLINE u8 read8LE(unsigned char const** ppSrc)
{
    const unsigned char* pSrc = *ppSrc;
    u4 low, high;

    low = pSrc[0];
    low |= pSrc[1] << 8;
    low |= pSrc[2] << 16;
    low |= pSrc[3] << 24;
    high = pSrc[4];
    high |= pSrc[5] << 8;
    high |= pSrc[6] << 16;
    high |= pSrc[7] << 24;

    *ppSrc = pSrc + 8;
    return ((u8) high << 32) | (u8) low;
}

INLINE void skipUtf8String(unsigned char const** ppSrc)
{
    u4 length = read4BE(ppSrc);

    (*ppSrc) += length;
}

INLINE int readUtf8String(unsigned char const** ppSrc, char* buf, size_t bufLen)
{
    u4 length = read4BE(ppSrc);
    size_t copyLen = (length < bufLen) ? length : bufLen-1;

    memcpy(buf, *ppSrc, copyLen);
    buf[copyLen] = '\0';

    (*ppSrc) += length;
    return length;
}

INLINE char* readNewUtf8String(unsigned char const** ppSrc, size_t* pLength)
{
    u4 length = read4BE(ppSrc);
    char* buf;

    buf = (char*) malloc(length+1);

    memcpy(buf, *ppSrc, length);
    buf[length] = '\0';

    (*ppSrc) += length;

    *pLength = length;
    return buf;
}


INLINE void set1(u1* buf, u1 val)
{
    *buf = (u1)(val);
}

INLINE void set2BE(u1* buf, u2 val)
{
    *buf++ = (u1)(val >> 8);
    *buf = (u1)(val);
}

INLINE void set4BE(u1* buf, u4 val)
{
    *buf++ = (u1)(val >> 24);
    *buf++ = (u1)(val >> 16);
    *buf++ = (u1)(val >> 8);
    *buf = (u1)(val);
}

INLINE void set8BE(u1* buf, u8 val)
{
    *buf++ = (u1)(val >> 56);
    *buf++ = (u1)(val >> 48);
    *buf++ = (u1)(val >> 40);
    *buf++ = (u1)(val >> 32);
    *buf++ = (u1)(val >> 24);
    *buf++ = (u1)(val >> 16);
    *buf++ = (u1)(val >> 8);
    *buf = (u1)(val);
}

INLINE void set2LE(u1* buf, u2 val)
{
    *buf++ = (u1)(val);
    *buf = (u1)(val >> 8);
}

INLINE void set4LE(u1* buf, u4 val)
{
    *buf++ = (u1)(val);
    *buf++ = (u1)(val >> 8);
    *buf++ = (u1)(val >> 16);
    *buf = (u1)(val >> 24);
}

INLINE void set8LE(u1* buf, u8 val)
{
    *buf++ = (u1)(val);
    *buf++ = (u1)(val >> 8);
    *buf++ = (u1)(val >> 16);
    *buf++ = (u1)(val >> 24);
    *buf++ = (u1)(val >> 32);
    *buf++ = (u1)(val >> 40);
    *buf++ = (u1)(val >> 48);
    *buf = (u1)(val >> 56);
}

INLINE void setUtf8String(u1* buf, const u1* str)
{
    u4 strLen = strlen((const char*)str);

    set4BE(buf, strLen);
    memcpy(buf + sizeof(u4), str, strLen);
}

#endif
