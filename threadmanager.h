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

    void handleTag(UINT64 tsc, int tagInstructionId);
    int lastTagHitId;

    void handleCallEnter(UINT64 tsc,int functionId);
    void handleCall(UINT64 tsc, int location);
    void handleRet(UINT64 tsc, int functionId);
    void handleLocation(LocationDetails* location);
    std::list<std::pair<Call, int> > callStack;
    UINT64 lastCallTSC;
    int lastCallLocation;

    void handleMemRef(AccessInstructionDetails* details, ADDRINT addresses[7]);

    std::list<TagInstance> currentTagInstances;
    std::list<TagInstance>::iterator findCurrentTagInstance(int tagId);

    /* tag handlers */
    void handleSimpleTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handleSectionTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handlePipelineTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handleTaskTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void endCurrentTaskTag(UINT64 tsc);

    void handleIgnoreAllTag(const TagInstruction& tagInstruction);
    void handleProcessAllTag(const TagInstruction& tagInstruction);

    void handleProcessCallsTag(const TagInstruction& tagInstruction);
    void handleProcessAccessesTag(const TagInstruction& tagInstruction);
    void handleIgnoreCallsTag(const TagInstruction& tagInstruction);
    void handleIgnoreAccessesTag(const TagInstruction& tagInstruction);


    /* Ignore checks */

    bool processCallsComputed;
    bool processAccessesComputed;

    /* Inputs for ignore checks */

    bool ignoreCalls;
    bool processCalls;

    bool ignoreAccesses;
    bool processAccesses;

    bool interestingProgramPart;

    void updateChecks();

    UINT64 startTSC;

    Thread self;

    void lock();
    void unlock();

    PIN_MUTEX mutex;
};

#endif // THREADMANAGER_H
