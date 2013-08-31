#ifndef INLINENATIVE
#define INLINENATIVE

bool dvmInlineNativeStartup(void);
void dvmInlineNativeShutdown(void);

typedef bool (*InlineOp4Func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult);

typedef struct InlineOperation {
    InlineOp4Func   func;
    const char*     classDescriptor;
    const char*     methodName;
    const char*     methodSignature;
} InlineOperation;

typedef enum NativeInlineOps {
    INLINE_EMPTYINLINEMETHOD = 0,
    INLINE_STRING_CHARAT = 1,
    INLINE_STRING_COMPARETO = 2,
    INLINE_STRING_EQUALS = 3,
    INLINE_STRING_FASTINDEXOF_II = 4,
    INLINE_STRING_IS_EMPTY = 5,
    INLINE_STRING_LENGTH = 6,
    INLINE_MATH_ABS_INT = 7,
    INLINE_MATH_ABS_LONG = 8,
    INLINE_MATH_ABS_FLOAT = 9,
    INLINE_MATH_ABS_DOUBLE = 10,
    INLINE_MATH_MIN_INT = 11,
    INLINE_MATH_MAX_INT = 12,
    INLINE_MATH_SQRT = 13,
    INLINE_MATH_COS = 14,
    INLINE_MATH_SIN = 15,
    INLINE_FLOAT_TO_INT_BITS = 16,
    INLINE_FLOAT_TO_RAW_INT_BITS = 17,
    INLINE_INT_BITS_TO_FLOAT = 18,
    INLINE_DOUBLE_TO_LONG_BITS = 19,
    INLINE_DOUBLE_TO_RAW_LONG_BITS = 20,
    INLINE_LONG_BITS_TO_DOUBLE = 21,
} NativeInlineOps;

const InlineOperation* dvmGetInlineOpsTable(void);
int dvmGetInlineOpsTableLength(void);

extern const InlineOperation gDvmInlineOpsTable[];

INLINE bool dvmPerformInlineOp4Std(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult, int opIndex)
{
    return (*gDvmInlineOpsTable[opIndex].func)(arg0, arg1, arg2, arg3, pResult);
}

bool dvmPerformInlineOp4Dbg(u4 arg0, u4 arg1, u4 arg2, u4 arg3,
    JValue* pResult, int opIndex);

#endif
