#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <set>

#include <pin.H>

class ThreadManager;

#include "manager.h"
#include "buffer.h"

class ThreadManager
{
public:
    ThreadManager() {}
    ThreadManager(Manager* manager, THREADID tid);

    void bufferFull(struct BufferEntry*, UINT64);
private:
    Manager* manager;
    THREADID tid;

    void handleEntry(struct BufferEntry*);

    void handleTag(ADDRINT instruction, UINT64 tsc, int tagInstructionId);
    int lastTagHitId;
    std::set<int> tags;

    UINT64 startTSC;
    struct timespec startTime;

    void lock();
    void unlock();

    PIN_MUTEX mutex;
};

#endif // THREADMANAGER_H
