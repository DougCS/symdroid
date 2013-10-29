#include "dalvik/Dalvik.h"


#ifdef HAVE__MEMCMP16
extern "C" u4 __memcmp16(const u2* s0, const u2* s1, size_t count);
#endif

static bool org_apache_harmony_dalvik_NativeTestTarget_emptyInlineMethod(
    u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
{
    // do nothing
    return true;
}

bool javaLangString_charAt(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    int count, offset;
    ArrayObject* chars;

    /* null reference check on "this" */
    if ((Object*) arg0 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    //ALOGI("String.charAt this=0x%08x index=%d", arg0, arg1);
    count = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_COUNT);
    if ((s4) arg1 < 0 || (s4) arg1 >= count) {
        dvmThrowStringIndexOutOfBoundsExceptionWithIndex(count, arg1);
        return false;
    } else {
        offset = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_OFFSET);
        chars = (ArrayObject*)
            dvmGetFieldObject((Object*) arg0, STRING_FIELDOFF_VALUE);

        pResult->i = ((const u2*)(void*)chars->contents)[arg1 + offset];
        return true;
    }
}

#ifdef CHECK_MEMCMP16
static void badMatch(StringObject* thisStrObj, StringObject* compStrObj,
    int expectResult, int newResult, const char* compareType)
{
    ArrayObject* thisArray;
    ArrayObject* compArray;
    const char* thisStr;
    const char* compStr;
    int thisOffset, compOffset, thisCount, compCount;

    thisCount =
        dvmGetFieldInt((Object*) thisStrObj, STRING_FIELDOFF_COUNT);
    compCount =
        dvmGetFieldInt((Object*) compStrObj, STRING_FIELDOFF_COUNT);
    thisOffset =
        dvmGetFieldInt((Object*) thisStrObj, STRING_FIELDOFF_OFFSET);
    compOffset =
        dvmGetFieldInt((Object*) compStrObj, STRING_FIELDOFF_OFFSET);
    thisArray = (ArrayObject*)
        dvmGetFieldObject((Object*) thisStrObj, STRING_FIELDOFF_VALUE);
    compArray = (ArrayObject*)
        dvmGetFieldObject((Object*) compStrObj, STRING_FIELDOFF_VALUE);

    thisStr = dvmCreateCstrFromString(thisStrObj);
    compStr = dvmCreateCstrFromString(compStrObj);

    ALOGE("%s expected %d got %d", compareType, expectResult, newResult);
    ALOGE(" this (o=%d l=%d) '%s'", thisOffset, thisCount, thisStr);
    ALOGE(" comp (o=%d l=%d) '%s'", compOffset, compCount, compStr);
    dvmPrintHexDumpEx(ANDROID_LOG_INFO, LOG_TAG,
        ((const u2*) thisArray->contents) + thisOffset, thisCount*2,
        kHexDumpLocal);
    dvmPrintHexDumpEx(ANDROID_LOG_INFO, LOG_TAG,
        ((const u2*) compArray->contents) + compOffset, compCount*2,
        kHexDumpLocal);
    dvmAbort();
}
#endif

bool javaLangString_compareTo(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    if ((Object*) arg0 == NULL || (Object*) arg1 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    if (arg0 == arg1) {
        pResult->i = 0;
        return true;
    }

    int thisCount, thisOffset, compCount, compOffset;
    ArrayObject* thisArray;
    ArrayObject* compArray;
    const u2* thisChars;
    const u2* compChars;
    int minCount, countDiff;

    thisCount = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_COUNT);
    compCount = dvmGetFieldInt((Object*) arg1, STRING_FIELDOFF_COUNT);
    countDiff = thisCount - compCount;
    minCount = (countDiff < 0) ? thisCount : compCount;
    thisOffset = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_OFFSET);
    compOffset = dvmGetFieldInt((Object*) arg1, STRING_FIELDOFF_OFFSET);
    thisArray = (ArrayObject*)
        dvmGetFieldObject((Object*) arg0, STRING_FIELDOFF_VALUE);
    compArray = (ArrayObject*)
        dvmGetFieldObject((Object*) arg1, STRING_FIELDOFF_VALUE);
    thisChars = ((const u2*)(void*)thisArray->contents) + thisOffset;
    compChars = ((const u2*)(void*)compArray->contents) + compOffset;

#ifdef HAVE__MEMCMP16
    int otherRes = __memcmp16(thisChars, compChars, minCount);
# ifdef CHECK_MEMCMP16
    int i;
    for (i = 0; i < minCount; i++) {
        if (thisChars[i] != compChars[i]) {
            pResult->i = (s4) thisChars[i] - (s4) compChars[i];
            if (pResult->i != otherRes) {
                badMatch((StringObject*) arg0, (StringObject*) arg1,
                    pResult->i, otherRes, "compareTo");
            }
            return true;
        }
    }
# endif
    if (otherRes != 0) {
        pResult->i = otherRes;
        return true;
    }

#else
    int i;
    for (i = 0; i < minCount; i++) {
        if (thisChars[i] != compChars[i]) {
            pResult->i = (s4) thisChars[i] - (s4) compChars[i];
            return true;
        }
    }
#endif

    pResult->i = countDiff;
    return true;
}

bool javaLangString_equals(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    if ((Object*) arg0 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    if (arg0 == arg1) {
        pResult->i = true;
        return true;
    }

    if (arg1 == 0 || ((Object*) arg0)->clazz != ((Object*) arg1)->clazz) {
        pResult->i = false;
        return true;
    }

    int thisCount, thisOffset, compCount, compOffset;
    ArrayObject* thisArray;
    ArrayObject* compArray;
    const u2* thisChars;
    const u2* compChars;

    thisCount = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_COUNT);
    compCount = dvmGetFieldInt((Object*) arg1, STRING_FIELDOFF_COUNT);
    if (thisCount != compCount) {
        pResult->i = false;
        return true;
    }

    thisOffset = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_OFFSET);
    compOffset = dvmGetFieldInt((Object*) arg1, STRING_FIELDOFF_OFFSET);
    thisArray = (ArrayObject*)
        dvmGetFieldObject((Object*) arg0, STRING_FIELDOFF_VALUE);
    compArray = (ArrayObject*)
        dvmGetFieldObject((Object*) arg1, STRING_FIELDOFF_VALUE);
    thisChars = ((const u2*)(void*)thisArray->contents) + thisOffset;
    compChars = ((const u2*)(void*)compArray->contents) + compOffset;

#ifdef HAVE__MEMCMP16
    pResult->i = (__memcmp16(thisChars, compChars, thisCount) == 0);
# ifdef CHECK_MEMCMP16
    int otherRes = (memcmp(thisChars, compChars, thisCount * 2) == 0);
    if (pResult->i != otherRes) {
        badMatch((StringObject*) arg0, (StringObject*) arg1,
            otherRes, pResult->i, "equals-1");
    }
# endif
#else
    int i;
    for (i = 0; i < thisCount; i++)
    for (i = thisCount-1; i >= 0; --i)
    {
        if (thisChars[i] != compChars[i]) {
            pResult->i = false;
            return true;
        }
    }
    pResult->i = true;
#endif

    return true;
}

bool javaLangString_length(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    ALOGI("String.length this=0x%08x pResult=%p", arg0, pResult);

    if ((Object*) arg0 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    pResult->i = dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_COUNT);
    return true;
}

bool javaLangString_isEmpty(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    //ALOGI("String.isEmpty this=0x%08x pResult=%p", arg0, pResult);

    if ((Object*) arg0 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    pResult->i = (dvmGetFieldInt((Object*) arg0, STRING_FIELDOFF_COUNT) == 0);
    return true;
}

static inline int indexOfCommon(Object* strObj, int ch, int start)
{
    if ((ch & 0xffff) != ch)
        return -1;

    ArrayObject* charArray =
        (ArrayObject*) dvmGetFieldObject(strObj, STRING_FIELDOFF_VALUE);
    const u2* chars = (const u2*)(void*)charArray->contents;
    int offset = dvmGetFieldInt(strObj, STRING_FIELDOFF_OFFSET);
    int count = dvmGetFieldInt(strObj, STRING_FIELDOFF_COUNT);
    ALOGI("String.indexOf(0x%08x, 0x%04x, %d) off=%d count=%d",
        (u4) strObj, ch, start, offset, count);

    chars += offset;

    if (start < 0)
        start = 0;
    else if (start > count)
        start = count;

#if 0
    while (start < count) {
        if (chars[start] == ch)
            return start;
        start++;
    }
#else
    const u2* ptr = chars + start;
    const u2* endPtr = chars + count;
    while (ptr < endPtr) {
        if (*ptr++ == ch)
            return (ptr-1) - chars;
    }
#endif

    return -1;
}

bool javaLangString_fastIndexOf_II(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    if ((Object*) arg0 == NULL) {
        dvmThrowNullPointerException(NULL);
        return false;
    }

    pResult->i = indexOfCommon((Object*) arg0, arg1, arg2);
    return true;
}

union Convert32 {
    u4 arg;
    float ff;
};

union Convert64 {
    u4 arg[2];
    s8 ll;
    double dd;
};

bool javaLangMath_abs_int(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    s4 val = (s4) arg0;
    pResult->i = (val >= 0) ? val : -val;
    return true;
}

bool javaLangMath_abs_long(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    s8 val = convert.ll;
    pResult->j = (val >= 0) ? val : -val;
    return true;
}

bool javaLangMath_abs_float(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert32 convert;
    /* clear the sign bit; assumes a fairly common fp representation */
    convert.arg = arg0 & 0x7fffffff;
    pResult->f = convert.ff;
    return true;
}

bool javaLangMath_abs_double(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    convert.ll &= 0x7fffffffffffffffULL;
    pResult->d = convert.dd;
    return true;
}

bool javaLangMath_min_int(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    pResult->i = ((s4) arg0 < (s4) arg1) ? arg0 : arg1;
    return true;
}

bool javaLangMath_max_int(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    pResult->i = ((s4) arg0 > (s4) arg1) ? arg0 : arg1;
    return true;
}

bool javaLangMath_sqrt(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->d = sqrt(convert.dd);
    return true;
}

bool javaLangMath_cos(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->d = cos(convert.dd);
    return true;
}

bool javaLangMath_sin(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->d = sin(convert.dd);
    return true;
}

bool javaLangFloat_floatToIntBits(u4 arg0, u4 arg1, u4 arg2, u4 arg,
    JValue* pResult)
{
    Convert32 convert;
    convert.arg = arg0;
    pResult->i = isnanf(convert.ff) ? 0x7fc00000 : arg0;
    return true;
}

bool javaLangFloat_floatToRawIntBits(u4 arg0, u4 arg1, u4 arg2, u4 arg,
    JValue* pResult)
{
    pResult->i = arg0;
    return true;
}

bool javaLangFloat_intBitsToFloat(u4 arg0, u4 arg1, u4 arg2, u4 arg,
    JValue* pResult)
{
    Convert32 convert;
    convert.arg = arg0;
    pResult->f = convert.ff;
    return true;
}

bool javaLangDouble_doubleToLongBits(u4 arg0, u4 arg1, u4 arg2, u4 arg,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->j = isnan(convert.dd) ? 0x7ff8000000000000LL : convert.ll;
    return true;
}

bool javaLangDouble_doubleToRawLongBits(u4 arg0, u4 arg1, u4 arg2,
    u4 arg, JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->j = convert.ll;
    return true;
}

bool javaLangDouble_longBitsToDouble(u4 arg0, u4 arg1, u4 arg2, u4 arg,
    JValue* pResult)
{
    Convert64 convert;
    convert.arg[0] = arg0;
    convert.arg[1] = arg1;
    pResult->d = convert.dd;
    return true;
}

const InlineOperation gDvmInlineOpsTable[] = {
    { org_apache_harmony_dalvik_NativeTestTarget_emptyInlineMethod,
        "Lorg/apache/harmony/dalvik/NativeTestTarget;",
        "emptyInlineMethod", "()V" },

    { javaLangString_charAt, "Ljava/lang/String;", "charAt", "(I)C" },
    { javaLangString_compareTo, "Ljava/lang/String;", "compareTo", "(Ljava/lang/String;)I" },
    { javaLangString_equals, "Ljava/lang/String;", "equals", "(Ljava/lang/Object;)Z" },
    { javaLangString_fastIndexOf_II, "Ljava/lang/String;", "fastIndexOf", "(II)I" },
    { javaLangString_isEmpty, "Ljava/lang/String;", "isEmpty", "()Z" },
    { javaLangString_length, "Ljava/lang/String;", "length", "()I" },

    { javaLangMath_abs_int, "Ljava/lang/Math;", "abs", "(I)I" },
    { javaLangMath_abs_long, "Ljava/lang/Math;", "abs", "(J)J" },
    { javaLangMath_abs_float, "Ljava/lang/Math;", "abs", "(F)F" },
    { javaLangMath_abs_double, "Ljava/lang/Math;", "abs", "(D)D" },
    { javaLangMath_min_int, "Ljava/lang/Math;", "min", "(II)I" },
    { javaLangMath_max_int, "Ljava/lang/Math;", "max", "(II)I" },
    { javaLangMath_sqrt, "Ljava/lang/Math;", "sqrt", "(D)D" },
    { javaLangMath_cos, "Ljava/lang/Math;", "cos", "(D)D" },
    { javaLangMath_sin, "Ljava/lang/Math;", "sin", "(D)D" },

    { javaLangFloat_floatToIntBits, "Ljava/lang/Float;", "floatToIntBits", "(F)I" },
    { javaLangFloat_floatToRawIntBits, "Ljava/lang/Float;", "floatToRawIntBits", "(F)I" },
    { javaLangFloat_intBitsToFloat, "Ljava/lang/Float;", "intBitsToFloat", "(I)F" },

    { javaLangDouble_doubleToLongBits, "Ljava/lang/Double;", "doubleToLongBits", "(D)J" },
    { javaLangDouble_doubleToRawLongBits, "Ljava/lang/Double;", "doubleToRawLongBits", "(D)J" },
    { javaLangDouble_longBitsToDouble, "Ljava/lang/Double;", "longBitsToDouble", "(J)D" },

    // These are implemented exactly the same in Math and StrictMath,
    // so we can make the StrictMath calls fast too. Note that this
    // isn't true in general!
    { javaLangMath_abs_int, "Ljava/lang/StrictMath;", "abs", "(I)I" },
    { javaLangMath_abs_long, "Ljava/lang/StrictMath;", "abs", "(J)J" },
    { javaLangMath_abs_float, "Ljava/lang/StrictMath;", "abs", "(F)F" },
    { javaLangMath_abs_double, "Ljava/lang/StrictMath;", "abs", "(D)D" },
    { javaLangMath_min_int, "Ljava/lang/StrictMath;", "min", "(II)I" },
    { javaLangMath_max_int, "Ljava/lang/StrictMath;", "max", "(II)I" },
    { javaLangMath_sqrt, "Ljava/lang/StrictMath;", "sqrt", "(D)D" },
};

bool dvmInlineNativeStartup()
{
    gDvm.inlinedMethods =
        (Method**) calloc(NELEM(gDvmInlineOpsTable), sizeof(Method*));
    if (gDvm.inlinedMethods == NULL)
        return false;

    return true;
}

void dvmInlineNativeShutdown()
{
    free(gDvm.inlinedMethods);
}


const InlineOperation* dvmGetInlineOpsTable()
{
    return gDvmInlineOpsTable;
}

int dvmGetInlineOpsTableLength()
{
    return NELEM(gDvmInlineOpsTable);
}

Method* dvmFindInlinableMethod(const char* classDescriptor,
    const char* methodName, const char* methodSignature)
{
    ClassObject* clazz = dvmFindClassNoInit(classDescriptor, NULL);
    if (clazz == NULL) {
        ALOGE("dvmFindInlinableMethod: can't find class '%s'",
            classDescriptor);
        dvmClearException(dvmThreadSelf());
        return NULL;
    }

    Method* method = dvmFindDirectMethodByDescriptor(clazz, methodName,
        methodSignature);
    if (method == NULL) {
        method = dvmFindVirtualMethodByDescriptor(clazz, methodName,
            methodSignature);
    }
    if (method == NULL) {
        ALOGE("dvmFindInlinableMethod: can't find method %s.%s %s",
            clazz->descriptor, methodName, methodSignature);
        return NULL;
    }

    if (!dvmIsFinalClass(clazz) && !dvmIsFinalMethod(method)) {
        ALOGE("dvmFindInlinableMethod: can't inline non-final method %s.%s",
            clazz->descriptor, method->name);
        return NULL;
    }
    if (dvmIsSynchronizedMethod(method) ||
            dvmIsDeclaredSynchronizedMethod(method)) {
        ALOGE("dvmFindInlinableMethod: can't inline synchronized method %s.%s",
            clazz->descriptor, method->name);
        return NULL;
    }

    return method;
}

Method* dvmResolveInlineNative(int opIndex)
{
    assert(opIndex >= 0 && opIndex < NELEM(gDvmInlineOpsTable));
    Method* method = gDvm.inlinedMethods[opIndex];
    if (method != NULL) {
        return method;
    }

    method = dvmFindInlinableMethod(
        gDvmInlineOpsTable[opIndex].classDescriptor,
        gDvmInlineOpsTable[opIndex].methodName,
        gDvmInlineOpsTable[opIndex].methodSignature);

    if (method == NULL) {
        return NULL;
    }

    gDvm.inlinedMethods[opIndex] = method;
    IF_ALOGV() {
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGV("Registered for profile: %s.%s %s",
            method->clazz->descriptor, method->name, desc);
        free(desc);
    }

    return method;
}

bool dvmPerformInlineOp4Dbg(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult, int opIndex)
{
    Method* method = dvmResolveInlineNative(opIndex);
    if (method == NULL) {
        return (*gDvmInlineOpsTable[opIndex].func)(arg0, arg1, arg2, arg3,
            pResult);
    }

    Thread* self = dvmThreadSelf();
    TRACE_METHOD_ENTER(self, method);
    bool result = (*gDvmInlineOpsTable[opIndex].func)(arg0, arg1, arg2, arg3,
        pResult);
    TRACE_METHOD_EXIT(self, method);
    return result;
}
