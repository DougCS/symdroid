#ifndef DALVIK_INIT_H_
#define DALVIK_INIT_H_

std::string dvmStartup(int argc, const char* const argv[],
        bool ignoreUnrecognized, JNIEnv* pEnv);
void dvmShutdown(void);
bool dvmInitAfterZygote(void);

void dvmLateEnableAssertions(void);

int dvmPrepForDexOpt(const char* bootClassPath, DexOptimizerMode dexOptMode,
    DexClassVerifyMode verifyMode, int dexoptFlags);

bool dvmFindRequiredClassesAndMembers(void);

bool dvmFindReferenceMembers(ClassObject* classReference);

int dvmFprintf(FILE* fp, const char* format, ...)
#if defined(__GNUC__)
    __attribute__ ((format(printf, 2, 3)))
#endif

#endif
