#ifndef DALVIK_DEBUGGER_H_
#define DALVIK_DEBUGGER_H_

#include <pthread.h>
#include "Common.h"
#include "Misc.h"
#include "jdwp/Jdwp.h"

struct Object;
struct ClassObject;
struct Method;
struct Thread;

struct AddressSet {
    u4 setSize;
    u1 set[1];
};

INLINE void dvmAddressSetSet(AddressSet *pSet, u4 toSet)
{
    if (toSet < pSet->setSize) {
        pSet->set[toSet/8] |= 1 << (toSet % 8);
    }
}

INLINE bool dvmAddressSetGet(const AddressSet *pSet, u4 toGet)
{
    if (toGet < pSet->setSize) {
        return (pSet->set[toGet/8] & (1 << (toGet % 8))) != 0;
    } else {
        return false;
    }
}

struct StepControl {
    JdwpStepSize size;
    JdwpStepDepth depth;
    Thread* thread;         /* don't deref; for comparison only */

    bool active;
    const Method* method;
    int line;
    const AddressSet* pAddressSet;
    int frameDepth;
};

struct DebugInvokeReq {
    bool ready;

    bool invokeNeeded;

    /* request */
    Object* obj;
    Object* thread;
    ClassObject* clazz;
    Method* method;
    u4 numArgs;
    u8* argArray;   /* will be NULL if numArgs==0 */
    u4 options;

    /* result */
    JdwpError err;
    u1 resultTag;
    JValue resultValue;
    ObjectId exceptObj;

    pthread_mutex_t lock;
    pthread_cond_t cv;
};

bool dvmDebuggerStartup(void);
void dvmDebuggerShutdown(void);

void dvmDbgInitMutex(pthread_mutex_t* pMutex);
void dvmDbgLockMutex(pthread_mutex_t* pMutex);
void dvmDbgUnlockMutex(pthread_mutex_t* pMutex);
void dvmDbgInitCond(pthread_cond_t* pCond);
void dvmDbgCondWait(pthread_cond_t* pCond, pthread_mutex_t* pMutex);
void dvmDbgCondSignal(pthread_cond_t* pCond);
void dvmDbgCondBroadcast(pthread_cond_t* pCond);

DebugInvokeReq* dvmDbgGetInvokeReq(void);

/*
 * Enable/disable breakpoints and step modes.  Used to provide a heads-up
 * when the debugger attaches.
 */
void dvmDbgConnected(void);
void dvmDbgActive(void);
void dvmDbgDisconnected(void);

bool dvmDbgIsDebuggerConnected(void);

s8 dvmDbgLastDebuggerActivity(void);

int dvmDbgThreadRunning(void);
int dvmDbgThreadWaiting(void);
int dvmDbgThreadContinuing(int status);

void dvmDbgExit(int status);

const char* dvmDbgGetClassDescriptor(RefTypeId id);
ObjectId dvmDbgGetClassObject(RefTypeId id);
RefTypeId dvmDbgGetSuperclass(RefTypeId id);
ObjectId dvmDbgGetClassLoader(RefTypeId id);
u4 dvmDbgGetAccessFlags(RefTypeId id);
bool dvmDbgIsInterface(RefTypeId id);
void dvmDbgGetClassList(u4* pNumClasses, RefTypeId** pClassRefBuf);
void dvmDbgGetVisibleClassList(ObjectId classLoaderId, u4* pNumClasses,
        RefTypeId** pClassRefBuf);
void dvmDbgGetClassInfo(RefTypeId classId, u1* pTypeTag, u4* pStatus,
    const char** pSignature);
bool dvmDbgFindLoadedClassBySignature(const char* classDescriptor,
        RefTypeId* pRefTypeId);
void dvmDbgGetObjectType(ObjectId objectId, u1* pRefTypeTag,
    RefTypeId* pRefTypeId);
u1 dvmDbgGetClassObjectType(RefTypeId refTypeId);
const char* dvmDbgGetSignature(RefTypeId refTypeId);
const char* dvmDbgGetSourceFile(RefTypeId refTypeId);
const char* dvmDbgGetObjectTypeName(ObjectId objectId);
u1 dvmDbgGetObjectTag(ObjectId objectId);
int dvmDbgGetTagWidth(int tag);

int dvmDbgGetArrayLength(ObjectId arrayId);
u1 dvmDbgGetArrayElementTag(ObjectId arrayId);
bool dvmDbgOutputArray(ObjectId arrayId, int firstIndex, int count,
    ExpandBuf* pReply);
bool dvmDbgSetArrayElements(ObjectId arrayId, int firstIndex, int count,
    const u1* buf);

ObjectId dvmDbgCreateString(const char* str);
ObjectId dvmDbgCreateObject(RefTypeId classId);
ObjectId dvmDbgCreateArrayObject(RefTypeId arrayTypeId, u4 length);

bool dvmDbgMatchType(RefTypeId instClassId, RefTypeId classId);

const char* dvmDbgGetMethodName(RefTypeId refTypeId, MethodId id);
void dvmDbgOutputAllFields(RefTypeId refTypeId, bool withGeneric,
    ExpandBuf* pReply);
void dvmDbgOutputAllMethods(RefTypeId refTypeId, bool withGeneric,
    ExpandBuf* pReply);
void dvmDbgOutputAllInterfaces(RefTypeId refTypeId, ExpandBuf* pReply);
void dvmDbgOutputLineTable(RefTypeId refTypeId, MethodId methodId,
    ExpandBuf* pReply);
void dvmDbgOutputVariableTable(RefTypeId refTypeId, MethodId id,
    bool withGeneric, ExpandBuf* pReply);

u1 dvmDbgGetFieldBasicTag(ObjectId objId, FieldId fieldId);
u1 dvmDbgGetStaticFieldBasicTag(RefTypeId refTypeId, FieldId fieldId);
void dvmDbgGetFieldValue(ObjectId objectId, FieldId fieldId, ExpandBuf* pReply);
void dvmDbgSetFieldValue(ObjectId objectId, FieldId fieldId, u8 value,
    int width);
void dvmDbgGetStaticFieldValue(RefTypeId refTypeId, FieldId fieldId,
    ExpandBuf* pReply);
void dvmDbgSetStaticFieldValue(RefTypeId refTypeId, FieldId fieldId,
    u8 rawValue, int width);

char* dvmDbgStringToUtf8(ObjectId strId);

char* dvmDbgGetThreadName(ObjectId threadId);
ObjectId dvmDbgGetThreadGroup(ObjectId threadId);
char* dvmDbgGetThreadGroupName(ObjectId threadGroupId);
ObjectId dvmDbgGetThreadGroupParent(ObjectId threadGroupId);
ObjectId dvmDbgGetSystemThreadGroupId(void);
ObjectId dvmDbgGetMainThreadGroupId(void);

bool dvmDbgGetThreadStatus(ObjectId threadId, u4* threadStatus,
    u4* suspendStatus);
u4 dvmDbgGetThreadSuspendCount(ObjectId threadId);
bool dvmDbgThreadExists(ObjectId threadId);
bool dvmDbgIsSuspended(ObjectId threadId);
//void dvmDbgWaitForSuspend(ObjectId threadId);
void dvmDbgGetThreadGroupThreads(ObjectId threadGroupId,
    ObjectId** ppThreadIds, u4* pThreadCount);
void dvmDbgGetAllThreads(ObjectId** ppThreadIds, u4* pThreadCount);
int dvmDbgGetThreadFrameCount(ObjectId threadId);
bool dvmDbgGetThreadFrame(ObjectId threadId, int num, FrameId* pFrameId,
    JdwpLocation* pLoc);

ObjectId dvmDbgGetThreadSelfId(void);
void dvmDbgSuspendVM(bool isEvent);
void dvmDbgResumeVM(void);
void dvmDbgSuspendThread(ObjectId threadId);
void dvmDbgResumeThread(ObjectId threadId);
void dvmDbgSuspendSelf(void);

bool dvmDbgGetThisObject(ObjectId threadId, FrameId frameId, ObjectId* pThisId);
void dvmDbgGetLocalValue(ObjectId threadId, FrameId frameId, int slot,
    u1 tag, u1* buf, int expectedLen);
void dvmDbgSetLocalValue(ObjectId threadId, FrameId frameId, int slot,
    u1 tag, u8 value, int width);


void dvmDbgPostLocationEvent(const Method* method, int pcOffset,
    Object* thisPtr, int eventFlags);
void dvmDbgPostException(void* throwFp, int throwRelPc, void* catchFp,
    int catchRelPc, Object* exception);
void dvmDbgPostThreadStart(Thread* thread);
void dvmDbgPostThreadDeath(Thread* thread);
void dvmDbgPostClassPrepare(ClassObject* clazz);

enum {
    DBG_BREAKPOINT      = 0x01,
    DBG_SINGLE_STEP     = 0x02,
    DBG_METHOD_ENTRY    = 0x04,
    DBG_METHOD_EXIT     = 0x08,
};

bool dvmDbgWatchLocation(const JdwpLocation* pLoc);
void dvmDbgUnwatchLocation(const JdwpLocation* pLoc);
bool dvmDbgConfigureStep(ObjectId threadId, JdwpStepSize size,
    JdwpStepDepth depth);
void dvmDbgUnconfigureStep(ObjectId threadId);

JdwpError dvmDbgInvokeMethod(ObjectId threadId, ObjectId objectId,
    RefTypeId classId, MethodId methodId, u4 numArgs, u8* argArray,
    u4 options, u1* pResultTag, u8* pResultValue, ObjectId* pExceptObj);
void dvmDbgExecuteMethod(DebugInvokeReq* pReq);

const AddressSet *dvmAddressSetForLine(const Method* method, int line);

void dvmDbgRegisterObjectId(ObjectId id);

bool dvmDbgDdmHandlePacket(const u1* buf, int dataLen, u1** pReplyBuf,
    int* pReplyLen);
void dvmDbgDdmConnected(void);
void dvmDbgDdmDisconnected(void);
void dvmDbgDdmSendChunk(int type, size_t len, const u1* buf);
void dvmDbgDdmSendChunkV(int type, const struct iovec* iov, int iovcnt);

#define CHUNK_TYPE(_name) \
    ((_name)[0] << 24 | (_name)[1] << 16 | (_name)[2] << 8 | (_name)[3])

#endif  // DALVIK_DEBUGGER_H_
