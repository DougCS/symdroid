#define __STDC_LIMIT_MACROS
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <linux/fs.h>
#include <cutils/fs.h>
#include <unistd.h>
#ifdef HAVE_ANDROID_OS
#include <sys/prctl.h>
#endif

#include "Dalvik.h"
#include "test/Test.h"
#include "mterp/Mterp.h"
#include "Hash.h"
#include "JniConstants.h"

#if defined(WITH_JIT)
#include "compiler/codegen/Optimizer.h"
#endif

#define kMinHeapStartSize   (1*1024*1024)
#define kMinHeapSize        (2*1024*1024)
#define kMaxHeapSize        (1*1024*1024*1024)

extern int jniRegisterSystemMethods(JNIEnv* env);

/* fwd */
static bool registerSystemNatives(JNIEnv* pEnv);
static bool initJdwp();
static bool initZygote();


/* global state */
struct DvmGlobals gDvm;
struct DvmJniGlobals gDvmJni;

/* JIT-specific global state */
#if defined(WITH_JIT)
struct DvmJitGlobals gDvmJit;

#if defined(WITH_JIT_TUNING)
int gDvmICHitCount;
#endif

#endif

static void usage(const char* progName)
{
    dvmFprintf(stderr, "%s: [options] class [argument ...]\n", progName);
    dvmFprintf(stderr, "%s: [options] -jar file.jar [argument ...]\n",progName);
    dvmFprintf(stderr, "\n");
    dvmFprintf(stderr, "The following standard options are recognized:\n");
    dvmFprintf(stderr, "  -classpath classpath\n");
    dvmFprintf(stderr, "  -Dproperty=value\n");
    dvmFprintf(stderr, "  -verbose:tag  ('gc', 'jni', or 'class')\n");
    dvmFprintf(stderr, "  -ea[:<package name>... |:<class name>]\n");
    dvmFprintf(stderr, "  -da[:<package name>... |:<class name>]\n");
    dvmFprintf(stderr, "   (-enableassertions, -disableassertions)\n");
    dvmFprintf(stderr, "  -esa\n");
    dvmFprintf(stderr, "  -dsa\n");
    dvmFprintf(stderr,
                "   (-enablesystemassertions, -disablesystemassertions)\n");
    dvmFprintf(stderr, "  -showversion\n");
    dvmFprintf(stderr, "  -help\n");
    dvmFprintf(stderr, "\n");
    dvmFprintf(stderr, "The following extended options are recognized:\n");
    dvmFprintf(stderr, "  -Xrunjdwp:<options>\n");
    dvmFprintf(stderr, "  -Xbootclasspath:bootclasspath\n");
    dvmFprintf(stderr, "  -Xcheck:tag  (e.g. 'jni')\n");
    dvmFprintf(stderr, "  -XmsN  (min heap, must be multiple of 1K, >= 1MB)\n");
    dvmFprintf(stderr, "  -XmxN  (max heap, must be multiple of 1K, >= 2MB)\n");
    dvmFprintf(stderr, "  -XssN  (stack size, >= %dKB, <= %dKB)\n",
        kMinStackSize / 1024, kMaxStackSize / 1024);
    dvmFprintf(stderr, "  -Xverify:{none,remote,all}\n");
    dvmFprintf(stderr, "  -Xrs\n");
#if defined(WITH_JIT)
    dvmFprintf(stderr,
                "  -Xint  (extended to accept ':portable', ':fast' and ':jit')\n");
#else
    dvmFprintf(stderr,
                "  -Xint  (extended to accept ':portable' and ':fast')\n");
#endif
    dvmFprintf(stderr, "\n");
    dvmFprintf(stderr, "These are unique to Dalvik:\n");
    dvmFprintf(stderr, "  -Xzygote\n");
    dvmFprintf(stderr, "  -Xdexopt:{none,verified,all,full}\n");
    dvmFprintf(stderr, "  -Xnoquithandler\n");
    dvmFprintf(stderr, "  -Xjniopts:{warnonly,forcecopy}\n");
    dvmFprintf(stderr, "  -Xjnitrace:substring (eg NativeClass or nativeMethod)\n");
    dvmFprintf(stderr, "  -Xstacktracefile:<filename>\n");
    dvmFprintf(stderr, "  -Xgc:[no]precise\n");
    dvmFprintf(stderr, "  -Xgc:[no]preverify\n");
    dvmFprintf(stderr, "  -Xgc:[no]postverify\n");
    dvmFprintf(stderr, "  -Xgc:[no]concurrent\n");
    dvmFprintf(stderr, "  -Xgc:[no]verifycardtable\n");
    dvmFprintf(stderr, "  -XX:+DisableExplicitGC\n");
    dvmFprintf(stderr, "  -X[no]genregmap\n");
    dvmFprintf(stderr, "  -Xverifyopt:[no]checkmon\n");
    dvmFprintf(stderr, "  -Xcheckdexsum\n");
#if defined(WITH_JIT)
    dvmFprintf(stderr, "  -Xincludeselectedop\n");
    dvmFprintf(stderr, "  -Xjitop:hexopvalue[-endvalue]"
                       "[,hexopvalue[-endvalue]]*\n");
    dvmFprintf(stderr, "  -Xincludeselectedmethod\n");
    dvmFprintf(stderr, "  -Xjitthreshold:decimalvalue\n");
    dvmFprintf(stderr, "  -Xjitblocking\n");
    dvmFprintf(stderr, "  -Xjitmethod:signature[,signature]* "
                       "(eg Ljava/lang/String\\;replace)\n");
    dvmFprintf(stderr, "  -Xjitclass:classname[,classname]*\n");
    dvmFprintf(stderr, "  -Xjitoffset:offset[,offset]\n");
    dvmFprintf(stderr, "  -Xjitconfig:filename\n");
    dvmFprintf(stderr, "  -Xjitcheckcg\n");
    dvmFprintf(stderr, "  -Xjitverbose\n");
    dvmFprintf(stderr, "  -Xjitprofile\n");
    dvmFprintf(stderr, "  -Xjitdisableopt\n");
    dvmFprintf(stderr, "  -Xjitsuspendpoll\n");
#endif
    dvmFprintf(stderr, "\n");
    dvmFprintf(stderr, "Configured with:"
        " debugger"
        " profiler"
        " hprof"
#ifdef WITH_TRACKREF_CHECKS
        " trackref_checks"
#endif
#ifdef WITH_INSTR_CHECKS
        " instr_checks"
#endif
#ifdef WITH_EXTRA_OBJECT_VALIDATION
        " extra_object_validation"
#endif
#ifdef WITH_EXTRA_GC_CHECKS
        " extra_gc_checks"
#endif
#if !defined(NDEBUG) && defined(WITH_DALVIK_ASSERT)
        " dalvik_assert"
#endif
#ifdef WITH_JNI_STACK_CHECK
        " jni_stack_check"
#endif
#ifdef EASY_GDB
        " easy_gdb"
#endif
#ifdef CHECK_MUTEX
        " check_mutex"
#endif
#if defined(WITH_JIT)
        " jit(" ARCH_VARIANT ")"
#endif
#if defined(WITH_SELF_VERIFICATION)
        " self_verification"
#endif
#if ANDROID_SMP != 0
        " smp"
#endif
    );
#ifdef DVM_SHOW_EXCEPTION
    dvmFprintf(stderr, " show_exception=%d", DVM_SHOW_EXCEPTION);
#endif
    dvmFprintf(stderr, "\n\n");
}

static void showJdwpHelp()
{
    dvmFprintf(stderr,
        "Example: -Xrunjdwp:transport=dt_socket,address=8000,server=y\n");
    dvmFprintf(stderr,
        "Example: -Xrunjdwp:transport=dt_socket,address=localhost:6500,server=n\n");
}

static void showVersion()
{
    dvmFprintf(stdout, "DalvikVM version %d.%d.%d\n",
        DALVIK_MAJOR_VERSION, DALVIK_MINOR_VERSION, DALVIK_BUG_VERSION);
    dvmFprintf(stdout,
        "Copyright (C) 2007 The Android Open Source Project\n\n"
        "This software is built from source code licensed under the "
        "Apache License,\n"
        "Version 2.0 (the \"License\"). You may obtain a copy of the "
        "License at\n\n"
        "     http://www.apache.org/licenses/LICENSE-2.0\n\n"
        "See the associated NOTICE file for this software for further "
        "details.\n");
}

static size_t parseMemOption(const char* s, size_t div)
{
    if (isdigit(*s)) {
        const char* s2;
        size_t val;

        val = strtoul(s, (char* *)&s2, 10);
        if (s2 != s) {
            if (*s2 != '\0') {
                char c;

                c = *s2++;
                if (*s2 == '\0') {
                    size_t mul;

                    if (c == '\0') {
                        mul = 1;
                    } else if (c == 'k' || c == 'K') {
                        mul = 1024;
                    } else if (c == 'm' || c == 'M') {
                        mul = 1024 * 1024;
                    } else if (c == 'g' || c == 'G') {
                        mul = 1024 * 1024 * 1024;
                    } else {
                        /* Unknown multiplier character.
                         */
                        return 0;
                    }

                    if (val <= SIZE_MAX / mul) {
                        val *= mul;
                    } else {
                        /* Clamp to a multiple of 1024.
                         */
                        val = SIZE_MAX & ~(1024-1);
                    }
                } else {
                    /* There's more than one character after the
                     * numeric part.
                     */
                    return 0;
                }
            }

            /* The man page says that a -Xm value must be
             * a multiple of 1024.
             */
            if (val % div == 0) {
                return val;
            }
        }
    }

    return 0;
}

static bool handleJdwpOption(const char* name, const char* value)
{
    if (strcmp(name, "transport") == 0) {
        if (strcmp(value, "dt_socket") == 0) {
            gDvm.jdwpTransport = kJdwpTransportSocket;
        } else if (strcmp(value, "dt_android_adb") == 0) {
            gDvm.jdwpTransport = kJdwpTransportAndroidAdb;
        } else {
            ALOGE("JDWP transport '%s' not supported", value);
            return false;
        }
    } else if (strcmp(name, "server") == 0) {
        if (*value == 'n')
            gDvm.jdwpServer = false;
        else if (*value == 'y')
            gDvm.jdwpServer = true;
        else {
            ALOGE("JDWP option 'server' must be 'y' or 'n'");
            return false;
        }
    } else if (strcmp(name, "suspend") == 0) {
        if (*value == 'n')
            gDvm.jdwpSuspend = false;
        else if (*value == 'y')
            gDvm.jdwpSuspend = true;
        else {
            ALOGE("JDWP option 'suspend' must be 'y' or 'n'");
            return false;
        }
    } else if (strcmp(name, "address") == 0) {
        /* this is either <port> or <host>:<port> */
        const char* colon = strchr(value, ':');
        char* end;
        long port;

        if (colon != NULL) {
            free(gDvm.jdwpHost);
            gDvm.jdwpHost = (char*) malloc(colon - value +1);
            strncpy(gDvm.jdwpHost, value, colon - value +1);
            gDvm.jdwpHost[colon-value] = '\0';
            value = colon + 1;
        }
        if (*value == '\0') {
            ALOGE("JDWP address missing port");
            return false;
        }
        port = strtol(value, &end, 10);
        if (*end != '\0') {
            ALOGE("JDWP address has junk in port field '%s'", value);
            return false;
        }
        gDvm.jdwpPort = port;
    } else if (strcmp(name, "launch") == 0 ||
               strcmp(name, "onthrow") == 0 ||
               strcmp(name, "oncaught") == 0 ||
               strcmp(name, "timeout") == 0)
    {
        /* valid but unsupported */
        ALOGI("Ignoring JDWP option '%s'='%s'", name, value);
    } else {
        ALOGI("Ignoring unrecognized JDWP option '%s'='%s'", name, value);
    }

    return true;
}

static bool parseJdwpOptions(const char* str)
{
    char* mangle = strdup(str);
    char* name = mangle;
    bool result = false;

    while (true) {
        char* value;
        char* comma;

        value = strchr(name, '=');
        if (value == NULL) {
            ALOGE("JDWP opts: garbage at '%s'", name);
            goto bail;
        }

        comma = strchr(name, ',');      // use name, not value, for safety
        if (comma != NULL) {
            if (comma < value) {
                ALOGE("JDWP opts: found comma before '=' in '%s'", mangle);
                goto bail;
            }
            *comma = '\0';
        }

        *value++ = '\0';        // stomp the '='

        if (!handleJdwpOption(name, value))
            goto bail;

        if (comma == NULL) {
            /* out of options */
            break;
        }
        name = comma+1;
    }

    /*
     * Make sure the combination of arguments makes sense.
     */
    if (gDvm.jdwpTransport == kJdwpTransportUnknown) {
        ALOGE("JDWP opts: must specify transport");
        goto bail;
    }
    if (!gDvm.jdwpServer && (gDvm.jdwpHost == NULL || gDvm.jdwpPort == 0)) {
        ALOGE("JDWP opts: when server=n, must specify host and port");
        goto bail;
    }
    // transport mandatory
    // outbound server address

    gDvm.jdwpConfigured = true;
    result = true;

bail:
    free(mangle);
    return result;
}

static bool enableAssertions(const char* pkgOrClass, bool enable)
{
    AssertionControl* pCtrl = &gDvm.assertionCtrl[gDvm.assertionCtrlCount++];
    pCtrl->enable = enable;

    if (pkgOrClass == NULL) {
        /* enable or disable for all system classes */
        pCtrl->isPackage = false;
        pCtrl->pkgOrClass = NULL;
        pCtrl->pkgOrClassLen = 0;
    } else {
        if (*pkgOrClass == '\0') {
            /* global enable/disable for all but system */
            pCtrl->isPackage = false;
            pCtrl->pkgOrClass = strdup("");
            pCtrl->pkgOrClassLen = 0;
        } else {
            pCtrl->pkgOrClass = dvmDotToSlash(pkgOrClass+1);    // skip ':'
            if (pCtrl->pkgOrClass == NULL) {
                /* can happen if class name includes an illegal '/' */
                ALOGW("Unable to process assertion arg '%s'", pkgOrClass);
                return false;
            }

            int len = strlen(pCtrl->pkgOrClass);
            if (len >= 3 && strcmp(pCtrl->pkgOrClass + len-3, "///") == 0) {
                /* mark as package, truncate two of the three slashes */
                pCtrl->isPackage = true;
                *(pCtrl->pkgOrClass + len-2) = '\0';
                pCtrl->pkgOrClassLen = len - 2;
            } else {
                /* just a class */
                pCtrl->isPackage = false;
                pCtrl->pkgOrClassLen = len;
            }
        }
    }

    return true;
}

void dvmLateEnableAssertions()
{
    if (gDvm.assertionCtrl == NULL) {
        ALOGD("Not late-enabling assertions: no assertionCtrl array");
        return;
    } else if (gDvm.assertionCtrlCount != 0) {
        ALOGD("Not late-enabling assertions: some asserts already configured");
        return;
    }
    ALOGD("Late-enabling assertions");

    /* global enable for all but system */
    AssertionControl* pCtrl = gDvm.assertionCtrl;
    pCtrl->pkgOrClass = strdup("");
    pCtrl->pkgOrClassLen = 0;
    pCtrl->isPackage = false;
    pCtrl->enable = true;
    gDvm.assertionCtrlCount = 1;
}


/*
 * Release memory associated with the AssertionCtrl array.
 */
static void freeAssertionCtrl()
{
    int i;

    for (i = 0; i < gDvm.assertionCtrlCount; i++)
        free(gDvm.assertionCtrl[i].pkgOrClass);
    free(gDvm.assertionCtrl);
}

#if defined(WITH_JIT)
/* Parse -Xjitop to selectively turn on/off certain opcodes for JIT */
static void processXjitop(const char* opt)
{
    if (opt[7] == ':') {
        const char* startPtr = &opt[8];
        char* endPtr = NULL;

        do {
            long startValue, endValue;

            startValue = strtol(startPtr, &endPtr, 16);
            if (startPtr != endPtr) {
                /* Just in case value is out of range */
                startValue %= kNumPackedOpcodes;

                if (*endPtr == '-') {
                    endValue = strtol(endPtr+1, &endPtr, 16);
                    endValue %= kNumPackedOpcodes;
                } else {
                    endValue = startValue;
                }

                for (; startValue <= endValue; startValue++) {
                    ALOGW("Dalvik opcode %x is selected for debugging",
                         (unsigned int) startValue);
                    /* Mark the corresponding bit to 1 */
                    gDvmJit.opList[startValue >> 3] |= 1 << (startValue & 0x7);
                }

                if (*endPtr == 0) {
                    break;
                }

                startPtr = endPtr + 1;

                continue;
            } else {
                if (*endPtr != 0) {
                    dvmFprintf(stderr,
                        "Warning: Unrecognized opcode value substring "
                        "%s\n", endPtr);
                }
                break;
            }
        } while (1);
    } else {
        int i;
        for (i = 0; i < (kNumPackedOpcodes+7)/8; i++) {
            gDvmJit.opList[i] = 0xff;
        }
        dvmFprintf(stderr, "Warning: select all opcodes\n");
    }
}

/* Parse -Xjitoffset to selectively turn on/off traces with certain offsets for JIT */
static void processXjitoffset(const char* opt) {
    gDvmJit.num_entries_pcTable = 0;
    char* buf = strdup(opt);
    char* start, *end;
    start = buf;
    int idx = 0;
    do {
        end = strchr(start, ',');
        if (end) {
            *end = 0;
        }

        dvmFprintf(stderr, "processXjitoffset start = %s\n", start);
        char* tmp = strdup(start);
        gDvmJit.pcTable[idx++] = atoi(tmp);
        free(tmp);
        if (idx >= COMPILER_PC_OFFSET_SIZE) {
            dvmFprintf(stderr, "processXjitoffset: ignore entries beyond %d\n", COMPILER_PC_OFFSET_SIZE);
            break;
        }
        if (end) {
            start = end + 1;
        } else {
            break;
        }
    } while (1);
    gDvmJit.num_entries_pcTable = idx;
    free(buf);
}

/* Parse -Xjitmethod to selectively turn on/off certain methods for JIT */
static void processXjitmethod(const char* opt, bool isMethod) {
    char* buf = strdup(opt);

    if (isMethod && gDvmJit.methodTable == NULL) {
        gDvmJit.methodTable = dvmHashTableCreate(8, NULL);
    }
    if (!isMethod && gDvmJit.classTable == NULL) {
        gDvmJit.classTable = dvmHashTableCreate(8, NULL);
    }

    char* start = buf;
    char* end;
    /*
     * Break comma-separated method signatures and enter them into the hash
     * table individually.
     */
    do {
        int hashValue;

        end = strchr(start, ',');
        if (end) {
            *end = 0;
        }

        hashValue = dvmComputeUtf8Hash(start);
        dvmHashTableLookup(isMethod ? gDvmJit.methodTable : gDvmJit.classTable,
                           hashValue, strdup(start), (HashCompareFunc) strcmp, true);

        if (end) {
            start = end + 1;
        } else {
            break;
        }
    } while (1);
    free(buf);
}

/* The format of jit_config.list:
   EXCLUDE or INCLUDE
   CLASS
   prefix1 ...
   METHOD
   prefix 1 ...
   OFFSET
   index ... //each pair is a range, if pcOff falls into a range, JIT
*/
static int processXjitconfig(const char* opt) {
   FILE* fp = fopen(opt, "r");
   if (fp == NULL) {
       return -1;
   }

   char fLine[500];
   bool startClass = false, startMethod = false, startOffset = false;
   gDvmJit.num_entries_pcTable = 0;
   int idx = 0;

   while (fgets(fLine, 500, fp) != NULL) {
       char* curLine = strtok(fLine, " \t\r\n");
       /* handles keyword CLASS, METHOD, INCLUDE, EXCLUDE */
       if (!strncmp(curLine, "CLASS", 5)) {
           startClass = true;
           startMethod = false;
           startOffset = false;
           continue;
       }
       if (!strncmp(curLine, "METHOD", 6)) {
           startMethod = true;
           startClass = false;
           startOffset = false;
           continue;
       }
       if (!strncmp(curLine, "OFFSET", 6)) {
           startOffset = true;
           startMethod = false;
           startClass = false;
           continue;
       }
       if (!strncmp(curLine, "EXCLUDE", 7)) {
          gDvmJit.includeSelectedMethod = false;
          continue;
       }
       if (!strncmp(curLine, "INCLUDE", 7)) {
          gDvmJit.includeSelectedMethod = true;
          continue;
       }
       if (!startMethod && !startClass && !startOffset) {
         continue;
       }

        int hashValue = dvmComputeUtf8Hash(curLine);
        if (startMethod) {
            if (gDvmJit.methodTable == NULL) {
                gDvmJit.methodTable = dvmHashTableCreate(8, NULL);
            }
            dvmHashTableLookup(gDvmJit.methodTable, hashValue,
                               strdup(curLine),
                               (HashCompareFunc) strcmp, true);
        } else if (startClass) {
            if (gDvmJit.classTable == NULL) {
                gDvmJit.classTable = dvmHashTableCreate(8, NULL);
            }
            dvmHashTableLookup(gDvmJit.classTable, hashValue,
                               strdup(curLine),
                               (HashCompareFunc) strcmp, true);
        } else if (startOffset) {
           int tmpInt = atoi(curLine);
           gDvmJit.pcTable[idx++] = tmpInt;
           if (idx >= COMPILER_PC_OFFSET_SIZE) {
               printf("processXjitoffset: ignore entries beyond %d\n", COMPILER_PC_OFFSET_SIZE);
               break;
           }
        }
   }
   gDvmJit.num_entries_pcTable = idx;
   fclose(fp);
   return 0;
}
#endif

static int processOptions(int argc, const char* const argv[],
    bool ignoreUnrecognized)
{
    int i;

    ALOGV("VM options (%d):", argc);
    for (i = 0; i < argc; i++)
        ALOGV("  %d: '%s'", i, argv[i]);

    /*
     * Over-allocate AssertionControl array for convenience.  If allocated,
     * the array must be able to hold at least one entry, so that the
     * zygote-time activation can do its business.
     */
    assert(gDvm.assertionCtrl == NULL);
    if (argc > 0) {
        gDvm.assertionCtrl =
            (AssertionControl*) malloc(sizeof(AssertionControl) * argc);
        if (gDvm.assertionCtrl == NULL)
            return -1;
        assert(gDvm.assertionCtrlCount == 0);
    }

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
            /* show usage and stop */
            return -1;

        } else if (strcmp(argv[i], "-version") == 0) {
            /* show version and stop */
            showVersion();
            return 1;
        } else if (strcmp(argv[i], "-showversion") == 0) {
            /* show version and continue */
            showVersion();

        } else if (strcmp(argv[i], "-classpath") == 0 ||
                   strcmp(argv[i], "-cp") == 0)
        {
            /* set classpath */
            if (i == argc-1) {
                dvmFprintf(stderr, "Missing classpath path list\n");
                return -1;
            }
            free(gDvm.classPathStr); /* in case we have compiled-in default */
            gDvm.classPathStr = strdup(argv[++i]);

        } else if (strncmp(argv[i], "-Xbootclasspath:",
                sizeof("-Xbootclasspath:")-1) == 0)
        {
            /* set bootclasspath */
            const char* path = argv[i] + sizeof("-Xbootclasspath:")-1;

            if (*path == '\0') {
                dvmFprintf(stderr, "Missing bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = strdup(path);

        } else if (strncmp(argv[i], "-Xbootclasspath/a:",
                sizeof("-Xbootclasspath/a:")-1) == 0) {
            const char* appPath = argv[i] + sizeof("-Xbootclasspath/a:")-1;

            if (*(appPath) == '\0') {
                dvmFprintf(stderr, "Missing appending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", gDvm.bootClassPathStr, appPath) < 0) {
                dvmFprintf(stderr, "Can't append to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;

        } else if (strncmp(argv[i], "-Xbootclasspath/p:",
                sizeof("-Xbootclasspath/p:")-1) == 0) {
            const char* prePath = argv[i] + sizeof("-Xbootclasspath/p:")-1;

            if (*(prePath) == '\0') {
                dvmFprintf(stderr, "Missing prepending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", prePath, gDvm.bootClassPathStr) < 0) {
                dvmFprintf(stderr, "Can't prepend to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;

        } else if (strncmp(argv[i], "-D", 2) == 0) {
            /* Properties are handled in managed code. We just check syntax. */
            if (strchr(argv[i], '=') == NULL) {
                dvmFprintf(stderr, "Bad system property setting: \"%s\"\n",
                    argv[i]);
                return -1;
            }
            gDvm.properties->push_back(argv[i] + 2);

        } else if (strcmp(argv[i], "-jar") == 0) {
            // TODO: handle this; name of jar should be in argv[i+1]
            dvmFprintf(stderr, "-jar not yet handled\n");
            assert(false);

        } else if (strncmp(argv[i], "-Xms", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1024);
            if (val != 0) {
                if (val >= kMinHeapStartSize && val <= kMaxHeapSize) {
                    gDvm.heapStartingSize = val;
                } else {
                    dvmFprintf(stderr,
                        "Invalid -Xms '%s', range is %dKB to %dKB\n",
                        argv[i], kMinHeapStartSize/1024, kMaxHeapSize/1024);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xms option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xmx", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1024);
            if (val != 0) {
                if (val >= kMinHeapSize && val <= kMaxHeapSize) {
                    gDvm.heapMaximumSize = val;
                } else {
                    dvmFprintf(stderr,
                        "Invalid -Xmx '%s', range is %dKB to %dKB\n",
                        argv[i], kMinHeapSize/1024, kMaxHeapSize/1024);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xmx option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapGrowthLimit=", 20) == 0) {
            size_t val = parseMemOption(argv[i] + 20, 1024);
            if (val != 0) {
                gDvm.heapGrowthLimit = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapGrowthLimit option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapMinFree=", 16) == 0) {
            size_t val = parseMemOption(argv[i] + 16, 1024);
            if (val != 0) {
                gDvm.heapMinFree = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapMinFree option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapMaxFree=", 16) == 0) {
            size_t val = parseMemOption(argv[i] + 16, 1024);
            if (val != 0) {
                gDvm.heapMaxFree = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapMaxFree option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapTargetUtilization=", 26) == 0) {
            const char* start = argv[i] + 26;
            const char* end = start;
            double val = strtod(start, const_cast<char**>(&end));
            // Ensure that we have a value, there was no cruft after it and it
            // satisfies a sensible range.
            bool sane_val = (start != end) && (end[0] == '\0') &&
                (val >= 0.1) && (val <= 0.9);
            if (sane_val) {
                gDvm.heapTargetUtilization = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapTargetUtilization option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xss", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1);
            if (val != 0) {
                if (val >= kMinStackSize && val <= kMaxStackSize) {
                    gDvm.stackSize = val;
                    if (val > gDvm.mainThreadStackSize) {
                        gDvm.mainThreadStackSize = val;
                    }
                } else {
                    dvmFprintf(stderr, "Invalid -Xss '%s', range is %d to %d\n",
                        argv[i], kMinStackSize, kMaxStackSize);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xss option '%s'\n", argv[i]);
                return -1;
            }

        } else if (strncmp(argv[i], "-XX:mainThreadStackSize=", strlen("-XX:mainThreadStackSize=")) == 0) {
            size_t val = parseMemOption(argv[i] + strlen("-XX:mainThreadStackSize="), 1);
            if (val != 0) {
                if (val >= kMinStackSize && val <= kMaxStackSize) {
                    gDvm.mainThreadStackSize = val;
                } else {
                    dvmFprintf(stderr, "Invalid -XX:mainThreadStackSize '%s', range is %d to %d\n",
                               argv[i], kMinStackSize, kMaxStackSize);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -XX:mainThreadStackSize option '%s'\n", argv[i]);
                return -1;
            }

        } else if (strncmp(argv[i], "-XX:+DisableExplicitGC", 22) == 0) {
            gDvm.disableExplicitGc = true;
        } else if (strcmp(argv[i], "-verbose") == 0 ||
            strcmp(argv[i], "-verbose:class") == 0)
        {
            // JNI spec says "-verbose:gc,class" is valid, but cmd line
            // doesn't work that way; may want to support.
            gDvm.verboseClass = true;
        } else if (strcmp(argv[i], "-verbose:jni") == 0) {
            gDvm.verboseJni = true;
        } else if (strcmp(argv[i], "-verbose:gc") == 0) {
            gDvm.verboseGc = true;
        } else if (strcmp(argv[i], "-verbose:shutdown") == 0) {
            gDvm.verboseShutdown = true;

        } else if (strncmp(argv[i], "-enableassertions", 17) == 0) {
            enableAssertions(argv[i] + 17, true);
        } else if (strncmp(argv[i], "-ea", 3) == 0) {
            enableAssertions(argv[i] + 3, true);
        } else if (strncmp(argv[i], "-disableassertions", 18) == 0) {
            enableAssertions(argv[i] + 18, false);
        } else if (strncmp(argv[i], "-da", 3) == 0) {
            enableAssertions(argv[i] + 3, false);
        } else if (strcmp(argv[i], "-enablesystemassertions") == 0 ||
                   strcmp(argv[i], "-esa") == 0)
        {
            enableAssertions(NULL, true);
        } else if (strcmp(argv[i], "-disablesystemassertions") == 0 ||
                   strcmp(argv[i], "-dsa") == 0)
        {
            enableAssertions(NULL, false);

        } else if (strncmp(argv[i], "-Xcheck:jni", 11) == 0) {
            /* nothing to do now -- was handled during JNI init */

        } else if (strcmp(argv[i], "-Xdebug") == 0) {
            /* accept but ignore */

        } else if (strncmp(argv[i], "-Xrunjdwp:", 10) == 0 ||
            strncmp(argv[i], "-agentlib:jdwp=", 15) == 0)
        {
            const char* tail;

            if (argv[i][1] == 'X')
                tail = argv[i] + 10;
            else
                tail = argv[i] + 15;

            if (strncmp(tail, "help", 4) == 0 || !parseJdwpOptions(tail)) {
                showJdwpHelp();
                return 1;
            }
        } else if (strcmp(argv[i], "-Xrs") == 0) {
            gDvm.reduceSignals = true;
        } else if (strcmp(argv[i], "-Xnoquithandler") == 0) {
            /* disables SIGQUIT handler thread while still blocking SIGQUIT */
            /* (useful if we don't want thread but system still signals us) */
            gDvm.noQuitHandler = true;
        } else if (strcmp(argv[i], "-Xzygote") == 0) {
            gDvm.zygote = true;
#if defined(WITH_JIT)
            gDvmJit.runningInAndroidFramework = true;
#endif
        } else if (strncmp(argv[i], "-Xdexopt:", 9) == 0) {
            if (strcmp(argv[i] + 9, "none") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_NONE;
            else if (strcmp(argv[i] + 9, "verified") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_VERIFIED;
            else if (strcmp(argv[i] + 9, "all") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_ALL;
            else if (strcmp(argv[i] + 9, "full") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_FULL;
            else {
                dvmFprintf(stderr, "Unrecognized dexopt option '%s'\n",argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xverify:", 9) == 0) {
            if (strcmp(argv[i] + 9, "none") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_NONE;
            else if (strcmp(argv[i] + 9, "remote") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_REMOTE;
            else if (strcmp(argv[i] + 9, "all") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_ALL;
            else {
                dvmFprintf(stderr, "Unrecognized verify option '%s'\n",argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xjnigreflimit:", 15) == 0) {
            // Ignored for backwards compatibility.
        } else if (strncmp(argv[i], "-Xjnitrace:", 11) == 0) {
            gDvm.jniTrace = strdup(argv[i] + 11);
        } else if (strcmp(argv[i], "-Xlog-stdio") == 0) {
            gDvm.logStdio = true;

        } else if (strncmp(argv[i], "-Xint", 5) == 0) {
            if (argv[i][5] == ':') {
                if (strcmp(argv[i] + 6, "portable") == 0)
                    gDvm.executionMode = kExecutionModeInterpPortable;
                else if (strcmp(argv[i] + 6, "fast") == 0)
                    gDvm.executionMode = kExecutionModeInterpFast;
#ifdef WITH_JIT
                else if (strcmp(argv[i] + 6, "jit") == 0)
                    gDvm.executionMode = kExecutionModeJit;
#endif
                else {
                    dvmFprintf(stderr,
                        "Warning: Unrecognized interpreter mode %s\n",argv[i]);
                    /* keep going */
                }
            } else {
                /* disable JIT if it was enabled by default */
                gDvm.executionMode = kExecutionModeInterpFast;
            }

        } else if (strncmp(argv[i], "-Xlockprofthreshold:", 20) == 0) {
            gDvm.lockProfThreshold = atoi(argv[i] + 20);

#ifdef WITH_JIT
        } else if (strncmp(argv[i], "-Xjitop", 7) == 0) {
            processXjitop(argv[i]);
        } else if (strncmp(argv[i], "-Xjitmethod:", 12) == 0) {
            processXjitmethod(argv[i] + strlen("-Xjitmethod:"), true);
        } else if (strncmp(argv[i], "-Xjitclass:", 11) == 0) {
            processXjitmethod(argv[i] + strlen("-Xjitclass:"), false);
        } else if (strncmp(argv[i], "-Xjitoffset:", 12) == 0) {
            processXjitoffset(argv[i] + strlen("-Xjitoffset:"));
        } else if (strncmp(argv[i], "-Xjitconfig:", 12) == 0) {
            processXjitconfig(argv[i] + strlen("-Xjitconfig:"));
        } else if (strncmp(argv[i], "-Xjitblocking", 13) == 0) {
          gDvmJit.blockingMode = true;
        } else if (strncmp(argv[i], "-Xjitthreshold:", 15) == 0) {
          gDvmJit.threshold = atoi(argv[i] + 15);
        } else if (strncmp(argv[i], "-Xincludeselectedop", 19) == 0) {
          gDvmJit.includeSelectedOp = true;
        } else if (strncmp(argv[i], "-Xincludeselectedmethod", 23) == 0) {
          gDvmJit.includeSelectedMethod = true;
        } else if (strncmp(argv[i], "-Xjitcheckcg", 12) == 0) {
          gDvmJit.checkCallGraph = true;
          /* Need to enable blocking mode due to stack crawling */
          gDvmJit.blockingMode = true;
        } else if (strncmp(argv[i], "-Xjitdumpbin", 12) == 0) {
          gDvmJit.printBinary = true;
        } else if (strncmp(argv[i], "-Xjitverbose", 12) == 0) {
          gDvmJit.printMe = true;
        } else if (strncmp(argv[i], "-Xjitprofile", 12) == 0) {
          gDvmJit.profileMode = kTraceProfilingContinuous;
        } else if (strncmp(argv[i], "-Xjitdisableopt", 15) == 0) {
          /* Disable selected optimizations */
          if (argv[i][15] == ':') {
              sscanf(argv[i] + 16, "%x", &gDvmJit.disableOpt);
          /* Disable all optimizations */
          } else {
              gDvmJit.disableOpt = -1;
          }
        } else if (strncmp(argv[i], "-Xjitsuspendpoll", 16) == 0) {
          gDvmJit.genSuspendPoll = true;
#endif

        } else if (strncmp(argv[i], "-Xstacktracefile:", 17) == 0) {
            gDvm.stackTraceFile = strdup(argv[i]+17);

        } else if (strcmp(argv[i], "-Xgenregmap") == 0) {
            gDvm.generateRegisterMaps = true;
        } else if (strcmp(argv[i], "-Xnogenregmap") == 0) {
            gDvm.generateRegisterMaps = false;

        } else if (strcmp(argv[i], "Xverifyopt:checkmon") == 0) {
            gDvm.monitorVerification = true;
        } else if (strcmp(argv[i], "Xverifyopt:nocheckmon") == 0) {
            gDvm.monitorVerification = false;

        } else if (strncmp(argv[i], "-Xgc:", 5) == 0) {
            if (strcmp(argv[i] + 5, "precise") == 0)
                gDvm.preciseGc = true;
            else if (strcmp(argv[i] + 5, "noprecise") == 0)
                gDvm.preciseGc = false;
            else if (strcmp(argv[i] + 5, "preverify") == 0)
                gDvm.preVerify = true;
            else if (strcmp(argv[i] + 5, "nopreverify") == 0)
                gDvm.preVerify = false;
            else if (strcmp(argv[i] + 5, "postverify") == 0)
                gDvm.postVerify = true;
            else if (strcmp(argv[i] + 5, "nopostverify") == 0)
                gDvm.postVerify = false;
            else if (strcmp(argv[i] + 5, "concurrent") == 0)
                gDvm.concurrentMarkSweep = true;
            else if (strcmp(argv[i] + 5, "noconcurrent") == 0)
                gDvm.concurrentMarkSweep = false;
            else if (strcmp(argv[i] + 5, "verifycardtable") == 0)
                gDvm.verifyCardTable = true;
            else if (strcmp(argv[i] + 5, "noverifycardtable") == 0)
                gDvm.verifyCardTable = false;
            else {
                dvmFprintf(stderr, "Bad value for -Xgc");
                return -1;
            }
            ALOGV("Precise GC configured %s", gDvm.preciseGc ? "ON" : "OFF");

        } else if (strcmp(argv[i], "-Xcheckdexsum") == 0) {
            gDvm.verifyDexChecksum = true;

        } else if (strcmp(argv[i], "-Xprofile:threadcpuclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceThreadCpu;
        } else if (strcmp(argv[i], "-Xprofile:wallclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceWall;
        } else if (strcmp(argv[i], "-Xprofile:dualclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceDual;

        } else {
            if (!ignoreUnrecognized) {
                dvmFprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
                return -1;
            }
        }
    }

    return 0;
}

static void setCommandLineDefaults()
{
    const char* envStr = getenv("CLASSPATH");
    if (envStr != NULL) {
        gDvm.classPathStr = strdup(envStr);
    } else {
        gDvm.classPathStr = strdup(".");
    }
    envStr = getenv("BOOTCLASSPATH");
    if (envStr != NULL) {
        gDvm.bootClassPathStr = strdup(envStr);
    } else {
        gDvm.bootClassPathStr = strdup(".");
    }

    gDvm.properties = new std::vector<std::string>();

    gDvm.heapStartingSize = 2 * 1024 * 1024;  // Spec says 16MB; too big for us.
    gDvm.heapMaximumSize = 16 * 1024 * 1024;  // Spec says 75% physical mem
    gDvm.heapGrowthLimit = 0;  // 0 means no growth limit
    gDvm.stackSize = kDefaultStackSize;
    gDvm.mainThreadStackSize = kDefaultStackSize;
    gDvm.heapTargetUtilization = 0.5;
    gDvm.heapMaxFree = 2 * 1024 * 1024;
    gDvm.heapMinFree = gDvm.heapMaxFree / 4;

    gDvm.concurrentMarkSweep = true;

    /* gDvm.jdwpSuspend = true; */

    /* allowed unless zygote config doesn't allow it */
    gDvm.jdwpAllowed = true;

    /* default verification and optimization modes */
    gDvm.classVerifyMode = VERIFY_MODE_ALL;
    gDvm.dexOptMode = OPTIMIZE_MODE_VERIFIED;
    gDvm.monitorVerification = false;
    gDvm.generateRegisterMaps = true;
    gDvm.registerMapMode = kRegisterMapModeTypePrecise;

#if defined(WITH_JIT)
    gDvm.executionMode = kExecutionModeJit;
    gDvmJit.num_entries_pcTable = 0;
    gDvmJit.includeSelectedMethod = false;
    gDvmJit.includeSelectedOffset = false;
    gDvmJit.methodTable = NULL;
    gDvmJit.classTable = NULL;

    gDvm.constInit = false;
    gDvm.commonInit = false;
#else
    gDvm.executionMode = kExecutionModeInterpFast;
#endif

    gDvm.dexOptForSmp = (ANDROID_SMP != 0);

    gDvm.profilerClockSource = kProfilerClockSourceDual;
}


static void busCatcher(int signum, siginfo_t* info, void* context)
{
    void* addr = info->si_addr;

    ALOGE("Caught a SIGBUS (%d), addr=%p", signum, addr);

    signal(SIGBUS, SIG_DFL);
}

static void blockSignals()
{
    sigset_t mask;
    int cc;

    sigemptyset(&mask);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGUSR1);      // used to initiate heap dump
#if defined(WITH_JIT) && defined(WITH_JIT_TUNING)
    sigaddset(&mask, SIGUSR2);      // used to investigate JIT internals
#endif
    sigaddset(&mask, SIGPIPE);
    cc = sigprocmask(SIG_BLOCK, &mask, NULL);
    assert(cc == 0);

    if (false) {
        /* TODO: save the old sigaction in a global */
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = busCatcher;
        sa.sa_flags = SA_SIGINFO;
        cc = sigaction(SIGBUS, &sa, NULL);
        assert(cc == 0);
    }
}

class ScopedShutdown {
public:
    ScopedShutdown() : armed_(true) {
    }

    ~ScopedShutdown() {
        if (armed_) {
            dvmShutdown();
        }
    }

    void disarm() {
        armed_ = false;
    }

private:
    bool armed_;
};

std::string dvmStartup(int argc, const char* const argv[],
        bool ignoreUnrecognized, JNIEnv* pEnv)
{
    ScopedShutdown scopedShutdown;

    assert(gDvm.initializing);

    ALOGV("VM init args (%d):", argc);
    for (int i = 0; i < argc; i++) {
        ALOGV("  %d: '%s'", i, argv[i]);
    }
    setCommandLineDefaults();

    int cc = processOptions(argc, argv, ignoreUnrecognized);
    if (cc != 0) {
        if (cc < 0) {
            dvmFprintf(stderr, "\n");
            usage("dalvikvm");
        }
        return "syntax error";
    }

#if WITH_EXTRA_GC_CHECKS > 1
    /* only "portable" interp has the extra goodies */
    if (gDvm.executionMode != kExecutionModeInterpPortable) {
        ALOGI("Switching to 'portable' interpreter for GC checks");
        gDvm.executionMode = kExecutionModeInterpPortable;
    }
#endif

    /* Configure group scheduling capabilities */
    if (!access("/dev/cpuctl/tasks", F_OK)) {
        ALOGV("Using kernel group scheduling");
        gDvm.kernelGroupScheduling = 1;
    } else {
        ALOGV("Using kernel scheduler policies");
    }

    /* configure signal handling */
    if (!gDvm.reduceSignals)
        blockSignals();

    /* verify system page size */
    if (sysconf(_SC_PAGESIZE) != SYSTEM_PAGE_SIZE) {
        return StringPrintf("expected page size %d, got %d",
                SYSTEM_PAGE_SIZE, (int) sysconf(_SC_PAGESIZE));
    }

    /* mterp setup */
    ALOGV("Using executionMode %d", gDvm.executionMode);
    dvmCheckAsmConstants();

    dvmQuasiAtomicsStartup();
    if (!dvmAllocTrackerStartup()) {
        return "dvmAllocTrackerStartup failed";
    }
    if (!dvmGcStartup()) {
        return "dvmGcStartup failed";
    }
    if (!dvmThreadStartup()) {
        return "dvmThreadStartup failed";
    }
    if (!dvmInlineNativeStartup()) {
        return "dvmInlineNativeStartup";
    }
    if (!dvmRegisterMapStartup()) {
        return "dvmRegisterMapStartup failed";
    }
    if (!dvmInstanceofStartup()) {
        return "dvmInstanceofStartup failed";
    }
    if (!dvmClassStartup()) {
        return "dvmClassStartup failed";
    }

    if (!dvmFindRequiredClassesAndMembers()) {
        return "dvmFindRequiredClassesAndMembers failed";
    }

    if (!dvmStringInternStartup()) {
        return "dvmStringInternStartup failed";
    }
    if (!dvmNativeStartup()) {
        return "dvmNativeStartup failed";
    }
    if (!dvmInternalNativeStartup()) {
        return "dvmInternalNativeStartup failed";
    }
    if (!dvmJniStartup()) {
        return "dvmJniStartup failed";
    }
    if (!dvmProfilingStartup()) {
        return "dvmProfilingStartup failed";
    }

    if (!dvmCreateInlineSubsTable()) {
        return "dvmCreateInlineSubsTable failed";
    }

    if (!dvmValidateBoxClasses()) {
        return "dvmValidateBoxClasses failed";
    }

    /*
     * Do the last bits of Thread struct initialization we need to allow
     * JNI calls to work.
     */
    if (!dvmPrepMainForJni(pEnv)) {
        return "dvmPrepMainForJni failed";
    }

    if (!dvmInitClass(gDvm.classJavaLangClass)) {
        return "couldn't initialized java.lang.Class";
    }

    if (!registerSystemNatives(pEnv)) {
        return "couldn't register system natives";
    }

    if (!dvmCreateStockExceptions()) {
        return "dvmCreateStockExceptions failed";
    }

    if (!dvmPrepMainThread()) {
        return "dvmPrepMainThread failed";
    }

    if (dvmReferenceTableEntries(&dvmThreadSelf()->internalLocalRefTable) != 0)
    {
        ALOGW("Warning: tracked references remain post-initialization");
        dvmDumpReferenceTable(&dvmThreadSelf()->internalLocalRefTable, "MAIN");
    }

    /* general debugging setup */
    if (!dvmDebuggerStartup()) {
        return "dvmDebuggerStartup failed";
    }

    if (!dvmGcStartupClasses()) {
        return "dvmGcStartupClasses failed";
    }

    /*
     * Init for either zygote mode or non-zygote mode.  The key difference
     * is that we don't start any additional threads in Zygote mode.
     */
    if (gDvm.zygote) {
        if (!initZygote()) {
            return "initZygote failed";
        }
    } else {
        if (!dvmInitAfterZygote()) {
            return "dvmInitAfterZygote failed";
        }
    }


#ifndef NDEBUG
    if (!dvmTestHash())
        ALOGE("dvmTestHash FAILED");
    if (false /*noisy!*/ && !dvmTestIndirectRefTable())
        ALOGE("dvmTestIndirectRefTable FAILED");
#endif

    if (dvmCheckException(dvmThreadSelf())) {
        dvmLogExceptionStackTrace();
        return "Exception pending at end of VM initialization";
    }

    scopedShutdown.disarm();
    return "";
}

static void loadJniLibrary(const char* name) {
    std::string mappedName(StringPrintf(OS_SHARED_LIB_FORMAT_STR, name));
    char* reason = NULL;
    if (!dvmLoadNativeCode(mappedName.c_str(), NULL, &reason)) {
        ALOGE("dvmLoadNativeCode failed for \"%s\": %s", name, reason);
        dvmAbort();
    }
}

static bool registerSystemNatives(JNIEnv* pEnv)
{
    // Main thread is always first in list.
    Thread* self = gDvm.threadList;

    // Must set this before allowing JNI-based method registration.
    self->status = THREAD_NATIVE;

    // First set up JniConstants, which is used by libcore.
    JniConstants::init(pEnv);

    // Set up our single JNI method.
    // TODO: factor this out if we add more.
    jclass c = pEnv->FindClass("java/lang/Class");
    if (c == NULL) {
        dvmAbort();
    }
    JNIEXPORT jobject JNICALL Java_java_lang_Class_getDex(JNIEnv* env, jclass javaClass);
    const JNINativeMethod Java_java_lang_Class[] = {
        { "getDex", "()Lcom/android/dex/Dex;", (void*) Java_java_lang_Class_getDex },
    };
    if (pEnv->RegisterNatives(c, Java_java_lang_Class, 1) != JNI_OK) {
        dvmAbort();
    }

    // Most JNI libraries can just use System.loadLibrary, but you can't
    // if you're the library that implements System.loadLibrary!
    loadJniLibrary("javacore");
    loadJniLibrary("nativehelper");

    // Back to run mode.
    self->status = THREAD_RUNNING;

    return true;
}

static std::string getMountsDevDir(const char *arg)
{
    char mount_dev[256];
    char mount_dir[256];
    int match;

    FILE *fp = fopen("/proc/self/mounts", "r");
    if (fp == NULL) {
        ALOGE("Could not open /proc/self/mounts: %s", strerror(errno));
        return "";
    }

    while ((match = fscanf(fp, "%255s %255s %*s %*s %*d %*d\n", mount_dev, mount_dir)) != EOF) {
        mount_dev[255] = 0;
        mount_dir[255] = 0;
        if (match == 2 && (strcmp(arg, mount_dir) == 0)) {
            fclose(fp);
            return mount_dev;
        }
    }

    fclose(fp);
    return "";
}

static bool initZygote()
{
    /* zygote goes into its own process group */
    setpgid(0,0);

    // See storage config details at http://source.android.com/tech/storage/
    // Create private mount namespace shared by all children
    if (unshare(CLONE_NEWNS) == -1) {
        SLOGE("Failed to unshare(): %s", strerror(errno));
        return -1;
    }

    // Mark rootfs as being a slave so that changes from default
    // namespace only flow into our children.
    if (mount("rootfs", "/", NULL, (MS_SLAVE | MS_REC), NULL) == -1) {
        SLOGE("Failed to mount() rootfs as MS_SLAVE: %s", strerror(errno));
        return -1;
    }

    // Create a staging tmpfs that is shared by our children; they will
    // bind mount storage into their respective private namespaces, which
    // are isolated from each other.
    const char* target_base = getenv("EMULATED_STORAGE_TARGET");
    if (target_base != NULL) {
        if (mount("tmpfs", target_base, "tmpfs", MS_NOSUID | MS_NODEV,
                "uid=0,gid=1028,mode=0050") == -1) {
            SLOGE("Failed to mount tmpfs to %s: %s", target_base, strerror(errno));
            return -1;
        }
    }

    // Mark /system as NOSUID | NODEV
    const char* android_root = getenv("ANDROID_ROOT");

    if (android_root == NULL) {
        SLOGE("environment variable ANDROID_ROOT does not exist?!?!");
        return -1;
    }

    std::string mountDev(getMountsDevDir(android_root));
    if (mountDev.empty()) {
        SLOGE("Unable to find mount point for %s", android_root);
        return -1;
    }

    if (mount(mountDev.c_str(), android_root, "none",
            MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_RDONLY | MS_BIND, NULL) == -1) {
        SLOGE("Remount of %s failed: %s", android_root, strerror(errno));
        return -1;
    }

#ifdef HAVE_ANDROID_OS
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        if (errno == EINVAL) {
            SLOGW("PR_SET_NO_NEW_PRIVS failed. "
                  "Is your kernel compiled correctly?: %s", strerror(errno));
            // Don't return -1 here, since it's expected that not all
            // kernels will support this option.
        } else {
            SLOGW("PR_SET_NO_NEW_PRIVS failed: %s", strerror(errno));
            return -1;
        }
    }
#endif

    return true;
}

bool dvmInitAfterZygote()
{
    u8 startHeap, startQuit, startJdwp;
    u8 endHeap, endQuit, endJdwp;

    startHeap = dvmGetRelativeTimeUsec();

    if (!dvmGcStartupAfterZygote())
        return false;

    endHeap = dvmGetRelativeTimeUsec();
    startQuit = dvmGetRelativeTimeUsec();

    /* start signal catcher thread that dumps stacks on SIGQUIT */
    if (!gDvm.reduceSignals && !gDvm.noQuitHandler) {
        if (!dvmSignalCatcherStartup())
            return false;
    }

    /* start stdout/stderr copier, if requested */
    if (gDvm.logStdio) {
        if (!dvmStdioConverterStartup())
            return false;
    }

    endQuit = dvmGetRelativeTimeUsec();
    startJdwp = dvmGetRelativeTimeUsec();

    if (!initJdwp()) {
        ALOGD("JDWP init failed; continuing anyway");
    }

    endJdwp = dvmGetRelativeTimeUsec();

    ALOGV("thread-start heap=%d quit=%d jdwp=%d total=%d usec",
        (int)(endHeap-startHeap), (int)(endQuit-startQuit),
        (int)(endJdwp-startJdwp), (int)(endJdwp-startHeap));

#ifdef WITH_JIT
    if (gDvm.executionMode == kExecutionModeJit) {
        if (!dvmCompilerStartup())
            return false;
    }
#endif

    return true;
}

static bool initJdwp()
{
    assert(!gDvm.zygote);

    if (gDvm.jdwpAllowed && gDvm.jdwpConfigured) {
        JdwpStartupParams params;

        if (gDvm.jdwpHost != NULL) {
            if (strlen(gDvm.jdwpHost) >= sizeof(params.host)-1) {
                ALOGE("ERROR: hostname too long: '%s'", gDvm.jdwpHost);
                return false;
            }
            strcpy(params.host, gDvm.jdwpHost);
        } else {
            params.host[0] = '\0';
        }
        params.transport = gDvm.jdwpTransport;
        params.server = gDvm.jdwpServer;
        params.suspend = gDvm.jdwpSuspend;
        params.port = gDvm.jdwpPort;

        gDvm.jdwpState = dvmJdwpStartup(&params);
        if (gDvm.jdwpState == NULL) {
            ALOGW("WARNING: debugger thread failed to initialize");
            /* TODO: ignore? fail? need to mimic "expected" behavior */
        }
    }

    if (dvmJdwpIsActive(gDvm.jdwpState)) {
        //dvmChangeStatus(NULL, THREAD_RUNNING);
        if (!dvmJdwpPostVMStart(gDvm.jdwpState, gDvm.jdwpSuspend)) {
            ALOGW("WARNING: failed to post 'start' message to debugger");
            /* keep going */
        }
        //dvmChangeStatus(NULL, THREAD_NATIVE);
    }

    return true;
}

int dvmPrepForDexOpt(const char* bootClassPath, DexOptimizerMode dexOptMode,
    DexClassVerifyMode verifyMode, int dexoptFlags)
{
    gDvm.initializing = true;
    gDvm.optimizing = true;

    /* configure signal handling */
    blockSignals();

    /* set some defaults */
    setCommandLineDefaults();
    free(gDvm.bootClassPathStr);
    gDvm.bootClassPathStr = strdup(bootClassPath);

    /* set opt/verify modes */
    gDvm.dexOptMode = dexOptMode;
    gDvm.classVerifyMode = verifyMode;
    gDvm.generateRegisterMaps = (dexoptFlags & DEXOPT_GEN_REGISTER_MAPS) != 0;
    if (dexoptFlags & DEXOPT_SMP) {
        assert((dexoptFlags & DEXOPT_UNIPROCESSOR) == 0);
        gDvm.dexOptForSmp = true;
    } else if (dexoptFlags & DEXOPT_UNIPROCESSOR) {
        gDvm.dexOptForSmp = false;
    } else {
        gDvm.dexOptForSmp = (ANDROID_SMP != 0);
    }

    if (!dvmGcStartup())
        goto fail;
    if (!dvmThreadStartup())
        goto fail;
    if (!dvmInlineNativeStartup())
        goto fail;
    if (!dvmRegisterMapStartup())
        goto fail;
    if (!dvmInstanceofStartup())
        goto fail;
    if (!dvmClassStartup())
        goto fail;


    return 0;

fail:
    dvmShutdown();
    return 1;
}


void dvmShutdown()
{
    ALOGV("VM shutting down");

    if (CALC_CACHE_STATS)
        dvmDumpAtomicCacheStats(gDvm.instanceofCache);

    dvmGcThreadShutdown();

    if (gDvm.jdwpState != NULL)
        dvmJdwpShutdown(gDvm.jdwpState);
    free(gDvm.jdwpHost);
    gDvm.jdwpHost = NULL;
    free(gDvm.jniTrace);
    gDvm.jniTrace = NULL;
    free(gDvm.stackTraceFile);
    gDvm.stackTraceFile = NULL;

    /* tell signal catcher to shut down if it was started */
    dvmSignalCatcherShutdown();

    /* shut down stdout/stderr conversion */
    dvmStdioConverterShutdown();

#ifdef WITH_JIT
    if (gDvm.executionMode == kExecutionModeJit) {
        /* shut down the compiler thread */
        dvmCompilerShutdown();
    }
#endif

    dvmSlayDaemons();

    if (gDvm.verboseShutdown)
        ALOGD("VM cleaning up");

    dvmDebuggerShutdown();
    dvmProfilingShutdown();
    dvmJniShutdown();
    dvmStringInternShutdown();
    dvmThreadShutdown();
    dvmClassShutdown();
    dvmRegisterMapShutdown();
    dvmInstanceofShutdown();
    dvmInlineNativeShutdown();
    dvmGcShutdown();
    dvmAllocTrackerShutdown();

    /* these must happen AFTER dvmClassShutdown has walked through class data */
    dvmNativeShutdown();
    dvmInternalNativeShutdown();

    dvmFreeInlineSubsTable();

    free(gDvm.bootClassPathStr);
    free(gDvm.classPathStr);
    delete gDvm.properties;

    freeAssertionCtrl();

    dvmQuasiAtomicsShutdown();

    memset(&gDvm, 0xcd, sizeof(gDvm));
}


int dvmFprintf(FILE* fp, const char* format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    if (gDvm.vfprintfHook != NULL)
        result = (*gDvm.vfprintfHook)(fp, format, args);
    else
        result = vfprintf(fp, format, args);
    va_end(args);

    return result;
}

#ifdef __GLIBC__
#include <execinfo.h>
void dvmPrintNativeBackTrace()
{
    size_t MAX_STACK_FRAMES = 64;
    void* stackFrames[MAX_STACK_FRAMES];
    size_t frameCount = backtrace(stackFrames, MAX_STACK_FRAMES);

    char** strings = backtrace_symbols(stackFrames, frameCount);
    if (strings == NULL) {
        ALOGE("backtrace_symbols failed: %s", strerror(errno));
        return;
    }

    size_t i;
    for (i = 0; i < frameCount; ++i) {
        ALOGW("#%-2d %s", i, strings[i]);
    }
    free(strings);
}
#else
void dvmPrintNativeBackTrace() {
    /* Hopefully, you're on an Android device and debuggerd will do this. */
}
#endif

void dvmAbort()
{
    /*
     * Leave gDvm.lastMessage on the stack frame which can be decoded in the
     * tombstone file. This is for situations where we only have tombstone files
     * but no logs (ie b/5372634).
     *
     * For example, in the tombstone file you usually see this:
     *
     *   #00  pc 00050ef2  /system/lib/libdvm.so (dvmAbort)
     *   #01  pc 00077670  /system/lib/libdvm.so (_Z15dvmClassStartupv)
     *     :
     *
     * stack:
     *     :
     * #00 beed2658  00000000
     *     beed265c  7379732f
     *     beed2660  2f6d6574
     *     beed2664  6d617266
     *     beed2668  726f7765
     *     beed266c  6f632f6b
     *     beed2670  6a2e6572
     *     beed2674  00007261
     *     beed2678  00000000
     *
     * The ascii values between beed265c and beed2674 belongs to messageBuffer
     * and it can be decoded as "/system/framework/core.jar".
     */
    const int messageLength = 512;
    char messageBuffer[messageLength] = {0};
    int result = 0;

    snprintf(messageBuffer, messageLength, "%s", gDvm.lastMessage);

    /* So that messageBuffer[] looks like useful stuff to the compiler */
    for (int i = 0; i < messageLength && messageBuffer[i]; i++) {
        result += messageBuffer[i];
    }

    ALOGE("VM aborting");

    fflush(NULL);       // flush all open file buffers

    if (gDvm.abortHook != NULL)
        (*gDvm.abortHook)();

    dvmPrintNativeBackTrace();

    abort();

}