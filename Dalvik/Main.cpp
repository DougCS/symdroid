#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <assert.h>


static void blockSigpipe()
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        fprintf(stderr, "WARNING: SIGPIPE not blocked\n");
}

static jobjectArray createStringArray(JNIEnv* env, char* const argv[], int argc)
{
    jclass stringClass = NULL;
    jobjectArray strArray = NULL;
    jobjectArray result = NULL;
    int i;

    stringClass = env->FindClass("java/lang/String");
    if (env->ExceptionCheck()) {
        fprintf(stderr, "Got exception while finding class String\n");
        goto bail;
    }
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(argc, stringClass, NULL);
    if (env->ExceptionCheck()) {
        fprintf(stderr, "Got exception while creating String array\n");
        goto bail;
    }
    assert(strArray != NULL);

    for (i = 0; i < argc; i++) {
        jstring argStr;

        argStr = env->NewStringUTF(argv[i]);
        if (env->ExceptionCheck()) {
            fprintf(stderr, "Got exception while allocating Strings\n");
            goto bail;
        }
        assert(argStr != NULL);
        env->SetObjectArrayElement(strArray, i, argStr);
        env->DeleteLocalRef(argStr);
    }

    result = strArray;
    strArray = NULL;

bail:
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(strArray);
    return result;
}

static int methodIsPublic(JNIEnv* env, jclass clazz, jmethodID methodId)
{
    static const int PUBLIC = 0x0001;   // java.lang.reflect.Modifiers.PUBLIC
    jobject refMethod = NULL;
    jclass methodClass = NULL;
    jmethodID getModifiersId;
    int modifiers;
    int result = JNI_FALSE;

    refMethod = env->ToReflectedMethod(clazz, methodId, JNI_FALSE);
    if (refMethod == NULL) {
        fprintf(stderr, "Dalvik VM unable to get reflected method\n");
        goto bail;
    }

    methodClass = env->FindClass("java/lang/reflect/Method");
    if (methodClass == NULL) {
        fprintf(stderr, "Dalvik VM unable to find class Method\n");
        goto bail;
    }
    getModifiersId = env->GetMethodID(methodClass,
                        "getModifiers", "()I");
    if (getModifiersId == NULL) {
        fprintf(stderr, "Dalvik VM unable to find reflect.Method.getModifiers\n");
        goto bail;
    }

    modifiers = env->CallIntMethod(refMethod, getModifiersId);
    if ((modifiers & PUBLIC) == 0) {
        fprintf(stderr, "Dalvik VM: main() is not public\n");
        goto bail;
    }

    result = JNI_TRUE;

bail:
    env->DeleteLocalRef(refMethod);
    env->DeleteLocalRef(methodClass);
    return result;
}

int main(int argc, char* const argv[])
{
    JavaVM* vm = NULL;
    JNIEnv* env = NULL;
    JavaVMInitArgs initArgs;
    JavaVMOption* options = NULL;
    char* slashClass = NULL;
    int optionCount, curOpt, i, argIdx;
    int needExtra = JNI_FALSE;
    int result = 1;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* ignore argv[0] */
    argv++;
    argc--;

    optionCount = argc;

    options = (JavaVMOption*) malloc(sizeof(JavaVMOption) * optionCount);
    memset(options, 0, sizeof(JavaVMOption) * optionCount);

    for (curOpt = argIdx = 0; argIdx < argc; argIdx++) {
        if (argv[argIdx][0] != '-' && !needExtra)
            break;
        options[curOpt++].optionString = strdup(argv[argIdx]);

        needExtra = JNI_FALSE;
        if (strcmp(argv[argIdx], "-classpath") == 0 ||
            strcmp(argv[argIdx], "-cp") == 0)
            /* others? */
        {
            needExtra = JNI_TRUE;
        }
    }

    if (needExtra) {
        fprintf(stderr, "Dalvik VM requires value after last option flag\n");
        goto bail;
    }


    assert(curOpt <= optionCount);

    initArgs.version = JNI_VERSION_1_4;
    initArgs.options = options;
    initArgs.nOptions = curOpt;
    initArgs.ignoreUnrecognized = JNI_FALSE;

    //printf("nOptions = %d\n", initArgs.nOptions);

    blockSigpipe();

    if (JNI_CreateJavaVM(&vm, &env, &initArgs) < 0) {
        fprintf(stderr, "Dalvik VM init failed (check log file)\n");
        goto bail;
    }

    if (argIdx == argc) {
        fprintf(stderr, "Dalvik VM requires a class name\n");
        goto bail;
    }

    jobjectArray strArray;
    strArray = createStringArray(env, &argv[argIdx+1], argc-argIdx-1);
    if (strArray == NULL)
        goto bail;

    jclass startClass;
    jmethodID startMeth;
    char* cp;

    slashClass = strdup(argv[argIdx]);
    for (cp = slashClass; *cp != '\0'; cp++)
        if (*cp == '.')
            *cp = '/';

    startClass = env->FindClass(slashClass);
    if (startClass == NULL) {
        fprintf(stderr, "Dalvik VM unable to locate class '%s'\n", slashClass);
        goto bail;
    }

    startMeth = env->GetStaticMethodID(startClass,
                    "main", "([Ljava/lang/String;)V");
    if (startMeth == NULL) {
        fprintf(stderr, "Dalvik VM unable to find static main(String[]) in '%s'\n",
            slashClass);
        goto bail;
    }

    if (!methodIsPublic(env, startClass, startMeth))
        goto bail;

    env->CallStaticVoidMethod(startClass, startMeth, strArray);

    if (!env->ExceptionCheck())
        result = 0;

bail:
    /*printf("Shutting down Dalvik VM\n");*/
    if (vm != NULL) {
        if (vm->DetachCurrentThread() != JNI_OK) {
            fprintf(stderr, "Warning: unable to detach main thread\n");
            result = 1;
        }

        if (vm->DestroyJavaVM() != 0)
            fprintf(stderr, "Warning: Dalvik VM did not shut down cleanly\n");
        /*printf("\nDalvik VM has exited\n");*/
    }

    for (i = 0; i < optionCount; i++)
        free((char*) options[i].optionString);
    free(options);
    free(slashClass);
    /*printf("--- VM is down, process exiting\n");*/
    return result;
}
