#ifndef LIBDEX_OPTINVOCATION_H_
#define LIBDEX_OPTINVOCATION_H_

char* dexOptGenerateCacheFileName(const char* fileName,
    const char* subFileName);
int dexOptCreateEmptyHeader(int fd);

#endif
