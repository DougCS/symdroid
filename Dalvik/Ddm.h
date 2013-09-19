#ifndef DALVIK_DDM_H_
#define DALVIK_DDM_H_

bool dvmDdmHandlePacket(const u1* buf, int dataLen, u1** pReplyBuf,
    int* pReplyLen);

void dvmDdmConnected(void);
void dvmDdmDisconnected(void);

void dvmDdmSetThreadNotification(bool enable);

void dvmDdmSendThreadNotification(Thread* thread, bool started);

void dvmDdmSendThreadNameChange(int threadId, StringObject* newName);

ArrayObject* dvmDdmGenerateThreadStats(void);

bool dvmDdmHandleHpifChunk(int when);

bool dvmDdmHandleHpsgNhsgChunk(int when, int what, bool native);

ArrayObject* dvmDdmGetStackTraceById(u4 threadId);

ArrayObject* dvmDdmGetRecentAllocations(void);

#endif  // DALVIK_DDM_H_
