#include "dalvik/Dalvik.h"
#include "dalvik/OptInvocation.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <fcntl.h>
#include <errno.h>

static const char* kDexInJarName = "classes.dex";

static int openAlternateSuffix(const char *fileName, const char *suffix,
    int flags, char **pCachedName)
{
    char *buf, *c;
    size_t fileNameLen = strlen(fileName);
    size_t suffixLen = strlen(suffix);
    size_t bufLen = fileNameLen + suffixLen + 1;
    int fd = -1;

    buf = (char*)malloc(bufLen);
    if (buf == NULL) {
        errno = ENOMEM;
        return -1;
    }

    memcpy(buf, fileName, fileNameLen + 1);
    c = strrchr(buf, '.');
    if (c == NULL) {
        errno = ENOENT;
        goto bail;
    }
    memcpy(c + 1, suffix, suffixLen + 1);

    fd = open(buf, flags);
    if (fd >= 0) {
        *pCachedName = buf;
        return fd;
    }
    ALOGV("Couldn't open %s: %s", buf, strerror(errno));
bail:
    free(buf);
    return -1;
}

DexCacheStatus dvmDexCacheStatus(const char *fileName)
{
    ZipArchive archive;
    char* cachedName = NULL;
    int fd;
    DexCacheStatus result = DEX_CACHE_ERROR;
    ZipEntry entry;

    if (dvmClassPathContains(gDvm.bootClassPath, fileName)) {
        return DEX_CACHE_OK;
    }


    if (dexZipOpenArchive(fileName, &archive) != 0) {
        return DEX_CACHE_BAD_ARCHIVE;
    }
    entry = dexZipFindEntry(&archive, kDexInJarName);
    if (entry != NULL) {
        bool newFile = false;

        ALOGV("dvmDexCacheStatus: Checking cache for %s", fileName);
        cachedName = dexOptGenerateCacheFileName(fileName, kDexInJarName);
        if (cachedName == NULL)
            return DEX_CACHE_BAD_ARCHIVE;

        fd = dvmOpenCachedDexFile(fileName, cachedName,
                dexGetZipEntryModTime(&archive, entry),
                dexGetZipEntryCrc32(&archive, entry),
                /*isBootstrap=*/false, &newFile, /*createIfMissing=*/false);
        ALOGV("dvmOpenCachedDexFile returned fd %d", fd);
        if (fd < 0) {
            result = DEX_CACHE_STALE;
            goto bail;
        }

        if (!dvmUnlockCachedDexFile(fd)) {
            /* uh oh -- this process needs to exit or we'll wedge the system */
            ALOGE("Unable to unlock DEX file");
            goto bail;
        }

    } else {
        fd = openAlternateSuffix(fileName, "odex", O_RDONLY, &cachedName);
        if (fd < 0) {
            ALOGI("Zip is good, but no %s inside, and no .odex "
                    "file in the same directory", kDexInJarName);
            result = DEX_CACHE_BAD_ARCHIVE;
            goto bail;
        }

        ALOGV("Using alternate file (odex) for %s ...", fileName);
        if (!dvmCheckOptHeaderAndDependencies(fd, false, 0, 0, true, true)) {
            ALOGE("%s odex has stale dependencies", fileName);
            ALOGE("odex source not available -- failing");
            result = DEX_CACHE_STALE_ODEX;
            goto bail;
        } else {
            ALOGV("%s odex has good dependencies", fileName);
        }
    }
    result = DEX_CACHE_OK;

bail:
    dexZipCloseArchive(&archive);
    free(cachedName);
    if (fd >= 0) {
        close(fd);
    }
    return result;
}

int dvmJarFileOpen(const char* fileName, const char* odexOutputName,
    JarFile** ppJarFile, bool isBootstrap)
{

    ZipArchive archive;
    DvmDex* pDvmDex = NULL;
    char* cachedName = NULL;
    bool archiveOpen = false;
    bool locked = false;
    int fd = -1;
    int result = -1;

    if (dexZipOpenArchive(fileName, &archive) != 0)
        goto bail;
    archiveOpen = true;

    dvmSetCloseOnExec(dexZipGetArchiveFd(&archive));

    fd = openAlternateSuffix(fileName, "odex", O_RDONLY, &cachedName);
    if (fd >= 0) {
        ALOGV("Using alternate file (odex) for %s ...", fileName);
        if (!dvmCheckOptHeaderAndDependencies(fd, false, 0, 0, true, true)) {
            ALOGE("%s odex has stale dependencies", fileName);
            free(cachedName);
            cachedName = NULL;
            close(fd);
            fd = -1;
            goto tryArchive;
        } else {
            ALOGV("%s odex has good dependencies", fileName);
        }
    } else {
        ZipEntry entry;

tryArchive:
        entry = dexZipFindEntry(&archive, kDexInJarName);
        if (entry != NULL) {
            bool newFile = false;

            if (odexOutputName == NULL) {
                cachedName = dexOptGenerateCacheFileName(fileName,
                                kDexInJarName);
                if (cachedName == NULL)
                    goto bail;
            } else {
                cachedName = strdup(odexOutputName);
            }
            ALOGV("dvmJarFileOpen: Checking cache for %s (%s)",
                fileName, cachedName);
            fd = dvmOpenCachedDexFile(fileName, cachedName,
                    dexGetZipEntryModTime(&archive, entry),
                    dexGetZipEntryCrc32(&archive, entry),
                    isBootstrap, &newFile, /*createIfMissing=*/true);
            if (fd < 0) {
                ALOGI("Unable to open or create cache for %s (%s)",
                    fileName, cachedName);
                goto bail;
            }
            locked = true;

            /*
            Generate the optimized DEX.
             */
            if (newFile) {
                u8 startWhen, extractWhen, endWhen;
                bool result;
                off_t dexOffset;

                dexOffset = lseek(fd, 0, SEEK_CUR);
                result = (dexOffset > 0);

                if (result) {
                    startWhen = dvmGetRelativeTimeUsec();
                    result = dexZipExtractEntryToFile(&archive, entry, fd) == 0;
                    extractWhen = dvmGetRelativeTimeUsec();
                }
                if (result) {
                    result = dvmOptimizeDexFile(fd, dexOffset,
                                dexGetZipEntryUncompLen(&archive, entry),
                                fileName,
                                dexGetZipEntryModTime(&archive, entry),
                                dexGetZipEntryCrc32(&archive, entry),
                                isBootstrap);
                }

                if (!result) {
                    ALOGE("Unable to extract+optimize DEX from '%s'",
                        fileName);
                    goto bail;
                }

                endWhen = dvmGetRelativeTimeUsec();
                ALOGD("DEX prep '%s': unzip in %dms, rewrite %dms",
                    fileName,
                    (int) (extractWhen - startWhen) / 1000,
                    (int) (endWhen - extractWhen) / 1000);
            }
        } else {
            ALOGI("Zip is good, but no %s inside, and no valid .odex "
                    "file in the same directory", kDexInJarName);
            goto bail;
        }
    }

    if (dvmDexFileOpenFromFd(fd, &pDvmDex) != 0) {
        ALOGI("Unable to map %s in %s", kDexInJarName, fileName);
        goto bail;
    }

    if (locked) {
        /* unlock the fd */
        if (!dvmUnlockCachedDexFile(fd)) {
            /* This process needs to exit or we'll wedge the system O_O */
            ALOGE("Unable to unlock DEX file");
            goto bail;
        }
        locked = false;
    }

    ALOGV("Successfully opened '%s' in '%s'", kDexInJarName, fileName);

    *ppJarFile = (JarFile*) calloc(1, sizeof(JarFile));
    (*ppJarFile)->archive = archive;
    (*ppJarFile)->cacheFileName = cachedName;
    (*ppJarFile)->pDvmDex = pDvmDex;
    cachedName = NULL;      // don't free it below
    result = 0;

bail:
    if (archiveOpen && result != 0)
        dexZipCloseArchive(&archive);
    free(cachedName);
    if (fd >= 0) {
        if (locked)
            (void) dvmUnlockCachedDexFile(fd);
        close(fd);
    }
    return result;
}

void dvmJarFileFree(JarFile* pJarFile)
{
    if (pJarFile == NULL)
        return;

    dvmDexFileFree(pJarFile->pDvmDex);
    dexZipCloseArchive(&pJarFile->archive);
    free(pJarFile->cacheFileName);
    free(pJarFile);
}
