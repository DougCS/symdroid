#ifndef _DALVIK_EXCEPTION
#define _DALVIK_EXCEPTION

/* initialization */
bool dvmExceptionStartup(void);
void dvmExceptionShutdown(void);

void dvmThrowChainedException(const char* exceptionDescriptor, const char* msg,
    Object* cause);
INLINE void dvmThrowException(const char* exceptionDescriptor,
    const char* msg)
{
    dvmThrowChainedException(exceptionDescriptor, msg, NULL);
}

/*
 * Like dvmThrowChainedException, but takes printf-style args for the message.
 */
void dvmThrowExceptionFmtV(const char* exceptionDescriptor, const char* fmt,
    va_list args);
void dvmThrowExceptionFmt(const char* exceptionDescriptor, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__ ((format(printf, 2, 3)))
#endif
    ;
INLINE void dvmThrowExceptionFmt(const char* exceptionDescriptor,
    const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dvmThrowExceptionFmtV(exceptionDescriptor, fmt, args);
    va_end(args);
}

void dvmThrowChainedExceptionByClass(ClassObject* exceptionClass,
    const char* msg, Object* cause);
INLINE void dvmThrowExceptionByClass(ClassObject* exceptionClass,
    const char* msg)
{
    dvmThrowChainedExceptionByClass(exceptionClass, msg, NULL);
}

void dvmThrowChainedExceptionWithClassMessage(const char* exceptionDescriptor,
    const char* messageDescriptor, Object* cause);
INLINE void dvmThrowExceptionWithClassMessage(const char* exceptionDescriptor,
    const char* messageDescriptor)
{
    dvmThrowChainedExceptionWithClassMessage(exceptionDescriptor,
        messageDescriptor, NULL);
}

void dvmThrowExceptionByClassWithClassMessage(ClassObject* exceptionClass,
    const char* messageDescriptor);

INLINE Object* dvmGetException(Thread* self) {
    return self->exception;
}

INLINE void dvmSetException(Thread* self, Object* exception)
{
    assert(exception != NULL);
    self->exception = exception;
}

INLINE void dvmClearException(Thread* self) {
    self->exception = NULL;
}

void dvmClearOptException(Thread* self);

INLINE bool dvmCheckException(Thread* self) {
    return (self->exception != NULL);
}

bool dvmIsCheckedException(const Object* exception);

void dvmWrapException(const char* newExcepStr);

Object* dvmGetExceptionCause(const Object* exception);

void dvmPrintExceptionStackTrace(void);

void dvmLogExceptionStackTrace(void);

int dvmFindCatchBlock(Thread* self, int relPc, Object* exception,
    bool doUnroll, void** newFrame);
void* dvmFillInStackTraceInternal(Thread* thread, bool wantObject, int* pCount);
INLINE Object* dvmFillInStackTrace(Thread* thread) {
    return (Object*) dvmFillInStackTraceInternal(thread, true, NULL);
}
ArrayObject* dvmGetStackTrace(const Object* stackState);
INLINE int* dvmFillInStackTraceRaw(Thread* thread, int* pCount) {
    return (int*) dvmFillInStackTraceInternal(thread, false, pCount);
}
ArrayObject* dvmGetStackTraceRaw(const int* intVals, int stackDepth);

void dvmLogRawStackTrace(const int* intVals, int stackDepth);

#endif
