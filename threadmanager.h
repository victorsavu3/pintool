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

    void handleTag(UINT64 tsc, int tagInstructionId, ADDRINT address);
    int lastTagHitId;
    ADDRINT lastHitAddress;

    void handleCallEnter(UINT64 tsc,int functionId, UINT64 rbp);
    void handleCall(UINT64 tsc, int location);
    void handleRet(UINT64 tsc, int functionId);
    void handleLocation(const LocationDetails &location);

    void handleFree(ADDRINT address);
    void handleAfterAlloc();
    void handleAllocEnter(AllocEnterBufferEntry entry);
    void handleMalloc(ADDRINT address, UINT64 size);
    void handleCalloc(ADDRINT address, UINT64 num, UINT64 size);
    void handleRealloc(ADDRINT address, ADDRINT old, UINT64 size);
    void clearStackReferences(ADDRINT rbp);

    struct ReferenceData {
          Reference ref;
          std::vector<Access> accesses;
    };

    bool inAlloc;
    AllocEnterBufferEntry alloc;
    std::map<ADDRINT, ReferenceData> references;

    struct CallData {
          Call call;
          int segment;
          UINT64 rbp;
    };

    std::list<CallData> callStack;
    UINT64 lastCallTSC;
    int lastCallLocation;

    ReferenceData& getReference(ADDRINT address, int size);
    void handleMemRef(AccessInstructionDetails* details, ADDRINT addresses[7]);

    std::list<TagInstance> currentTagInstances;
    std::list<TagInstance>::iterator findCurrentTagInstance(int tagId);

    void insertCurrentTagInstances(int instruction);

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
