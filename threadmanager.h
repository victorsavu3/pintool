#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <list>
#include <deque>

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
    void storeAllocation(AllocData allocation);
    void threadStopped();
private:
    Manager* manager;
    THREADID tid;

    void handleEntry(struct BufferEntry*);

    void handleTag(UINT64 tsc, int tagInstructionId, ADDRINT address);
    int lastTagHitId;
    ADDRINT lastHitAddress;

    void handleCallEnter(UINT64 tsc, int functionId, UINT64 rbp, UINT64 rsp);
    void handleCall(UINT64 tsc, int location, UINT64 rsp);
    void handleRet(UINT64 tsc, int functionId, UINT64 rsp);
    void handleLocation(const LocationDetails &location);

    void checkAllocation(UINT64 tsc);
    void handleFree(ADDRINT address);
    void handleMalloc(ADDRINT address, UINT64 size);
    void handleCalloc(ADDRINT address, UINT64 num, UINT64 size);
    void handleRealloc(ADDRINT address, ADDRINT old, UINT64 size);
    void clearStackReferences(ADDRINT from, ADDRINT rsp);

    struct CallData {
          Call call;
          int segment;
          UINT64 rbp;
          UINT64 rsp;
          std::set<int> tagInstances;
    };

    std::list<CallData> callStack;
    UINT64 lastCallTSC;
    int lastCallLocation;

    std::deque<AllocData> allocations; // allocated by malloc and friends tsc -> AllocationData

    ReferenceData& getReference(ADDRINT address, int size, UINT64 rsp);
    void handleMemRef(AccessInstructionDetails* details, ADDRINT addresses[7], UINT64 rsp);

    std::list<TagInstance> currentTagInstances;
    std::map<int, TagType> tagInstanceType;
    std::list<TagInstance>::iterator findCurrentTagInstance(int tagId);
    std::map<int, std::set<int> > containerTagInstanceChildren;
    std::map<int, int> childInstanceParent;

    void insertCurrentTagInstances(int instruction);

    /* tag handlers */
    void handleSimpleTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handleSectionTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handlePipelineTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handleSectionTaskTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void handlePipelineTaskTag(UINT64 tsc, const Tag& tag, const TagInstruction& tagInstruction, std::list<TagInstance>::iterator& instance);
    void endCurrentSectionTaskTag(UINT64 tsc);
    void endCurrentPipelineTaskTag(UINT64 tsc);

    void handleIgnoreAllTag(const TagInstruction& tagInstruction);
    void handleProcessAllTag(const TagInstruction& tagInstruction);

    void handleProcessCallsTag(const TagInstruction& tagInstruction);
    void handleProcessAccessesTag(const TagInstruction& tagInstruction);
    void handleIgnoreCallsTag(const TagInstruction& tagInstruction);
    void handleIgnoreAccessesTag(const TagInstruction& tagInstruction);

    /* Dependency analysis */

    std::map<int, std::map<ADDRINT, std::map<int, std::pair<int, AccessType> >>> tagAccessingReference;
    void recordTagAccess(TagInstance& instance, ADDRINT address, int reference, int access, AccessType accessType);
    void closeTagInstanceAccesses(const std::set<int>& tagInstances);
    void insertCallTagInstance(const CallData& data);


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
