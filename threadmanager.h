#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <list>

#include <pin.H>

class ThreadManager;

#include "manager.h"
#include "buffer.h"

class ThreadManager
{
public:
    ThreadManager() {}
    ThreadManager(Manager* manager, THREADID tid);
    ~ThreadManager();

    void bufferFull(struct BufferEntry*, UINT64);
    void threadStopped();
private:
    Manager* manager;
    THREADID tid;

    void handleEntry(struct BufferEntry*);

    void handleTag(ADDRINT instruction, UINT64 tsc, int tagInstructionId);
    int lastTagHitId;

    void handleCallEnter(ADDRINT instruction, UINT64 tsc, int functionId);
    void handleRet(ADDRINT instruction, UINT64 tsc);
    std::list<Call> callStack;

    std::list<TagInstance> currentTagInstances;
    std::list<TagInstance>::iterator findCurrentTagInstance(int tagId);

    UINT64 startTSC;

    Thread self;

    void lock();
    void unlock();

    PIN_MUTEX mutex;
};

#endif // THREADMANAGER_H
