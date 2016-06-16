#include "threadmanager.h"

#include <time.h>

#include "exception.h"
#include "asm.h"

ThreadManager::ThreadManager(Manager *manager, THREADID tid) : manager(manager), tid(tid)
{
    PIN_MutexInit(&mutex);

    startTSC = rdtsc();
    clock_gettime(CLOCK_REALTIME, &self.startTime);

    self.genId();
    inAlloc = false;

    processAccesses = false;
    processCalls = false;
    ignoreAccesses = false;
    ignoreCalls = false;

    updateChecks();
}

ThreadManager::~ThreadManager()
{
    PIN_MutexFini(&mutex);
}

void ThreadManager::bufferFull(BufferEntry * entries, UINT64 count)
{
    lock();
    for(UINT64 i = 0; i < count; i++)
    {
        handleEntry(&entries[i]);
    }
    unlock();
}


void ThreadManager::threadStopped()
{
    self.endTSC = rdtsc();
    clock_gettime(CLOCK_REALTIME, &self.endTime);

    manager->writer.insertThread(self);

    while (!callStack.empty())
    {
        std::ostringstream oss;

        oss << "Closing " << callStack.back().call.function << " a end of thread";
        Warn("threadStopped", oss.str());

        insertCallTagInstance(callStack.back());

        Call c = callStack.back().call;
        callStack.pop_back();

        c.end = self.endTSC;

        manager->writer.insertCall(c);
    }
}

void ThreadManager::handleEntry(BufferEntry * entry)
{
    if (inAlloc) {
        handleAfterAlloc();
    }

    switch (entry->type)
    {
    case BuferEntryType::Tag:
        handleTag(entry->data.tag.tsc - this->startTSC, entry->data.tag.tagId, entry->data.tag.address);
        break;
    case BuferEntryType::Call:
        if (processCallsComputed)
            handleLocation(manager->locationDetails[entry->data.callInstruction.location]);
        if (processCallsComputed)
            handleCall(entry->data.callInstruction.tsc - this->startTSC, (int)entry->data.callInstruction.location);
        break;
    case BuferEntryType::CallEnter:
        if (processCallsComputed)
            handleCallEnter(entry->data.callEnter.tsc - this->startTSC, entry->data.callEnter.functionId, entry->data.callEnter.rbp);
        break;
    case BuferEntryType::Ret:
        if (processCallsComputed)
            handleRet(entry->data.ret.tsc - this->startTSC, entry->data.ret.functionId);
        break;
    case BuferEntryType::MemRef:
        if (processCallsComputed)
            handleLocation(manager->locationDetails[((AccessInstructionDetails*)entry->data.memref.accessDetails)->location]);
        if (processAccessesComputed)
            handleMemRef((AccessInstructionDetails*)entry->data.memref.accessDetails, entry->data.memref.addresses);
        break;
    case BuferEntryType::AllocEnter:
        handleAllocEnter(entry->data.allocenter);
        break;
    case BuferEntryType::Free:
        handleFree(entry->data.free.ref);
        break;
    default:
        CorruptedBufferException("Invalid entry type");
    }
}

#include <sstream>

void ThreadManager::handleTag(UINT64 tsc, int tagInstructionId, ADDRINT address)
{
    if (tagInstructionId == lastTagHitId && address != lastHitAddress)
        return;

    lastTagHitId = tagInstructionId;
    lastHitAddress = address;

    // manager->writer.insertTagHit(tsc, tagInstructionId, self.id);

    TagInstruction& tagInstruction = manager->tagInstructionIdMap[tagInstructionId];
    Tag& tag = manager->tagIdTagMap[tagInstruction.tag];

    std::list<TagInstance>::iterator tagInstance = findCurrentTagInstance(tag.id);

    switch (tag.type)
    {
    case TagType::Simple:
        handleSimpleTag(tsc, tag, tagInstruction, tagInstance);
        break;
    case TagType::Pipeline:
        handlePipelineTag(tsc, tag, tagInstruction, tagInstance);
        break;
    case TagType::Section:
        handleSectionTag(tsc, tag, tagInstruction, tagInstance);
        break;
    case TagType::SectionTask:
        handleSectionTaskTag(tsc, tag, tagInstruction, tagInstance);
        break;
    case TagType::PipelineTask:
        handlePipelineTaskTag(tsc, tag, tagInstruction, tagInstance);
        break;
    case TagType::IgnoreAll:
        handleIgnoreAllTag(tagInstruction);
        break;
    case TagType::ProcessAll:
        handleProcessAllTag(tagInstruction);
        break;
    case TagType::ProcessCalls:
        handleProcessCallsTag(tagInstruction);
        break;
    case TagType::ProcessAccesses:
        handleProcessAccessesTag(tagInstruction);
        break;
    case TagType::IgnoreCalls:
        handleIgnoreCallsTag(tagInstruction);
        break;
    case TagType::IgnoreAccesses:
        handleIgnoreAccessesTag(tagInstruction);
        break;
    default:
        CorruptedBufferException("Invalid tag type");
    }

    if (currentTagInstances.size() > 0) {
        interestingProgramPart = true;
        updateChecks();
    } else {
        interestingProgramPart = false;
        updateChecks();
    }
}

void ThreadManager::handleCall(UINT64 tsc, int location)
{
    lastCallTSC = tsc;
    lastCallLocation = location;
}

void ThreadManager::handleCallEnter(UINT64 tsc, int functionId, UINT64 rbp)
{
    Call c;
    c.genId();

    if(callStack.empty())
    {
        c.instruction = -1;

        c.start = tsc;
    }
    else
    {
        Instruction i;

        i.segment = callStack.back().segment;
        i.type = InstructionType::Call;
        i.line = manager->locationDetails[lastCallLocation].line;
        i.column = manager->locationDetails[lastCallLocation].column;
        manager->writer.insertInstruction(i);
        insertCurrentTagInstances(i.id);

        c.instruction = i.id;
        c.start = lastCallTSC;
    }

    c.function = functionId;
    c.thread = self.id;

    Segment s;

    s.call = c.id;
    s.type = SegmentType::Standard;
    manager->writer.insertSegment(s);

    callStack.push_back({c, s.id, rbp});

    std::set<int>& callTagInstances = callStack.back().tagInstances;

    for (auto& it : currentTagInstances) {
        callTagInstances.insert(it.id);
    }
}

void ThreadManager::handleRet(UINT64 tsc, int functionId)
{
    if (callStack.empty())
        CorruptedBufferException("Return from empty callstack");

    Call c = callStack.back().call;

    while (c.function != functionId && !callStack.empty())
    {
        std::ostringstream oss;

        oss << "Unexpected return, expected " << functionId << " got " << c.function;

        Warn("handleRet", oss.str());

        clearStackReferences(callStack.back().rbp);
        insertCallTagInstance(callStack.back());

        callStack.pop_back();
        c = callStack.back().call;
    }

    if (callStack.empty())
        CorruptedBufferException("Could not find call in callstack");

    clearStackReferences(callStack.back().rbp);
    insertCallTagInstance(callStack.back());

    callStack.pop_back();

    c.end = tsc;

    manager->writer.insertCall(c);
}

void ThreadManager::handleLocation(const LocationDetails& location)
{
    if (callStack.empty())
        return;
}

void ThreadManager::handleFree(ADDRINT address)
{
    manager->lockReferences();
    auto it = manager->references.find(address);

    if (it == manager->references.end()) {
        manager->unlockReferences();
        return;
    }

    if(!it->second.wasAccessed) {
        manager->references.erase(address);
        manager->unlockReferences();
        return;
    }

    if (!callStack.empty()){
        Instruction instr;

        instr.type = InstructionType::Free;
        instr.segment = callStack.back().segment;
        instr.line = -1;
        instr.column = -1;

        manager->writer.insertInstruction(instr);
        insertCurrentTagInstances(instr.id);

        it->second.ref.deallocator = instr.id;
    } else {
        it->second.ref.deallocator = -1;
    }

    manager->writer.insertReference(it->second.ref);

    manager->references.erase(it);

    manager->unlockReferences();
}

void ThreadManager::handleAfterAlloc()
{
    ADDRINT address;

    manager->lockKnownAllocations();

    auto it = manager->knownAllocations.find(alloc);

    if (it == manager->knownAllocations.end() || it->second.empty()) {
        //CorruptedBufferException("Allocation return value not found");
        manager->unlockKnownAllocations();
        return;
    }

    auto next = it->second.lower_bound(alloc.tsc);
    auto rem = next;

    if (next == it->second.end()) {
        rem = it->second.end();
        rem--;
    } else {
        if (next->first != alloc.tsc) {
            next--;

            if (next != it->second.end()) {
                INT64 d1 = abs(next->first - alloc.tsc);
                INT64 d2 = abs(rem->first - alloc.tsc);

                if (d1 < d2) {
                    rem = next;
                }
            }
        }
    }

    address = rem->second;
    it->second.erase(rem);

    manager->unlockKnownAllocations();

    switch(alloc.type) {
    case AllocEntryType::malloc:
        handleMalloc(address, alloc.size);
        break;
    case AllocEntryType::calloc:
        handleCalloc(address, alloc.num, alloc.size);
        break;
    case AllocEntryType::realloc:
        handleRealloc(address, alloc.ref, alloc.size);
        break;
    default:
        CorruptedBufferException("Invalid allocation type");
    }

    inAlloc = false;
}

void ThreadManager::handleAllocEnter(AllocEnterBufferEntry entry)
{
    alloc = entry;
    inAlloc = true;
}

void ThreadManager::handleMalloc(ADDRINT address, UINT64 size)
{
    ReferenceData data;
    data.ref.size = size;
    data.ref.type = ReferenceType::Heap;

    std::stringstream stream;
    stream << std::hex << address;
    data.ref.name = stream.str();

    if(!callStack.empty()) {
        Instruction instr;

        instr.type = InstructionType::Alloc;
        instr.segment = callStack.back().segment;
        instr.line = -1;
        instr.column = -1;

        manager->writer.insertInstruction(instr);
        insertCurrentTagInstances(instr.id);

        data.ref.allocator = instr.id;
    } else {
        data.ref.allocator = -1;
    }

    // if (references.find(address) != references.end())
    //     CorruptedBufferException("Address reused by allocation");

    manager->lockReferences();
    manager->references.insert(std::make_pair(address, data));
    manager->unlockReferences();
}

void ThreadManager::handleCalloc(ADDRINT address, UINT64 num, UINT64 size)
{
    handleMalloc(address, num*size);
}

void ThreadManager::handleRealloc(ADDRINT address, ADDRINT old, UINT64 size)
{
    handleFree(old);
    handleMalloc(address, size);
}

void ThreadManager::clearStackReferences(ADDRINT rbp)
{
    manager->lockReferences();

    auto it = manager->references.lower_bound(rbp);

    if (it == manager->references.end()) {
        manager->unlockReferences();
        return;
    }

    auto itEnd = it;
    auto itStart = manager->references.end();

    while (it != manager->references.end() && it->second.ref.type == ReferenceType::Stack) {
        itStart = it;
        it--;
    }

    if (itStart == manager->references.end()) {
        manager->unlockReferences();
        return;
    }

    manager->references.erase(itStart, itEnd);

    manager->unlockReferences();
}

ReferenceData &ThreadManager::getReference(ADDRINT address, int size)
{
    {
        auto it = manager->references.find(address);

        if (it != manager->references.end()) {
            return it->second;
        }
    }

    {
        auto it = manager->references.lower_bound(address);

        if (it != manager->references.end() && --it != manager->references.end()) {
            if (address < it->first + it->second.ref.size) {
                return it->second;
            }
        }
    }

    if (!callStack.empty()) {
        ADDRINT rbp = callStack.back().rbp;

        if (address <= rbp && address > rbp - 1000000) { // TODO find real stack end using DWARF
            ReferenceData data;
            data.ref.size = size;
            data.ref.type = ReferenceType::Stack;
            data.ref.allocator = -1;
            data.ref.deallocator = -1;

            std::stringstream stream;
            stream << "S: " << std::hex << (ADDRDELTA)rbp << ':' << std::dec << (ADDRDELTA) ((ADDRDELTA)address - (ADDRDELTA)rbp);
            data.ref.name = stream.str();

            manager->writer.insertReference(data.ref);

            return manager->references.insert(std::make_pair(address, data)).first->second;
        }
    }

    ReferenceData data;
    data.ref.size = size;
    data.ref.type = ReferenceType::Global;
    data.ref.allocator = -1;
    data.ref.deallocator = -1;

    std::stringstream stream;
    stream << "G: " << std::hex << address;
    data.ref.name = stream.str();

    manager->writer.insertReference(data.ref);

    return manager->references.insert(std::make_pair(address, data)).first->second;
}

void ThreadManager::handleMemRef(AccessInstructionDetails* details, ADDRINT addresses[7])
{
    if (callStack.empty())
        return;

    Instruction instr;

    instr.type = InstructionType::Access;
    instr.segment = callStack.back().segment;
    instr.line = manager->locationDetails[details->location].line;
    instr.column = manager->locationDetails[details->location].column;

    manager->writer.insertInstruction(instr);
    insertCurrentTagInstances(instr.id);

    for (int i=0;i < details->accesses.size(); i++) {
        Access a;
        int refid;
        {
            manager->lockReferences();

            ReferenceData& data = getReference(addresses[i], details->accesses[i].size);
            data.wasAccessed = true;

            refid = data.ref.id;

            manager->unlockReferences();
        }

        a.reference = refid;
        a.instruction = instr.id;
        a.position = i;
        a.address = addresses[i];
        a.size = details->accesses[i].size;
        if (details->accesses[i].isRead) {
            a.type = AccessType::Read;
        } else {
            a.type = AccessType::Write;
        }

        manager->writer.insertAccess(a);

        for (auto& it: currentTagInstances) {
            recordTagAccess(it, a.address, a.reference, a.id, a.type);
        }
    }
}

std::list<TagInstance>::iterator ThreadManager::findCurrentTagInstance(int tagId)
{
    for(auto tagInstance = currentTagInstances.begin(); tagInstance != currentTagInstances.end(); tagInstance++)
    {
        if(tagInstance->tag == tagId)
            return tagInstance;
    }

    return currentTagInstances.end();
}

void ThreadManager::insertCurrentTagInstances(int instruction)
{
    for (auto it : currentTagInstances) {
        InstructionTagInstance iti;

        iti.instruction = instruction;
        iti.tagInstance = it.id;

        manager->writer.insertInstructionTagInstance(iti);
    }
}

void ThreadManager::handleSimpleTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator& tagInstance)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end())
        {
            CorruptedBufferException("Starting an already started tag");
        }

        TagInstance ti;
        ti.genId();

        ti.start = tsc;
        ti.tag = tag.id;
        ti.thread = self.id;

        currentTagInstances.push_front(ti);

        break;
    }

    case TagInstructionType::Stop:
    {
        if (tagInstance == currentTagInstances.end())
        {
            CorruptedBufferException("Stopping an unstarted tag");
        }

        tagInstance->end = tsc;

        manager->writer.insertTagInstance(*tagInstance);

        currentTagInstances.erase(tagInstance);
    }

    break;
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::handleSectionTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator &tagInstance)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end())
        {
            return; // Section tag can be set on loop header
        }

        TagInstance ti;
        ti.genId();

        ti.start = tsc;
        ti.tag = tag.id;
        ti.thread = self.id;

        currentTagInstances.push_front(ti);

        interestingProgramPart = true;

        break;
    }

    case TagInstructionType::Stop:
    {
        if (tagInstance == currentTagInstances.end())
        {
            CorruptedBufferException("Stopping an unstarted tag");
        }

        endCurrentSectionTaskTag(tsc);

        tagInstance->end = tsc;

        manager->writer.insertTagInstance(*tagInstance);

        currentTagInstances.erase(tagInstance);

        closeTagInstanceAccesses(containerTagInstanceChildren[tagInstance->id]);
        containerTagInstanceChildren.erase(tagInstance->id);

        interestingProgramPart = false;
    }

    break;
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::handlePipelineTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator &tagInstance)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end())
        {
            CorruptedBufferException("Starting an already started tag");
        }

        TagInstance ti;
        ti.genId();

        ti.start = tsc;
        ti.tag = tag.id;
        ti.thread = self.id;

        currentTagInstances.push_front(ti);

        interestingProgramPart = true;

        break;
    }

    case TagInstructionType::Stop:
    {
        if (tagInstance == currentTagInstances.end())
        {
            CorruptedBufferException("Stopping an unstarted tag");
        }

        endCurrentPipelineTaskTag(tsc);

        tagInstance->end = tsc;

        manager->writer.insertTagInstance(*tagInstance);

        currentTagInstances.erase(tagInstance);

        interestingProgramPart = false;
    }

    break;
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::handleSectionTaskTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator &tagInstance)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end())
        {
            tagInstance->end = tsc;

            manager->writer.insertTagInstance(*tagInstance);
            childInstanceParent.erase(tagInstance->id);

            currentTagInstances.erase(tagInstance);
        }

        auto container = std::find_if(currentTagInstances.begin(), currentTagInstances.end(), [&] (const TagInstance& instance)
        {
            const Tag& tag = manager->tagIdTagMap[instance.tag];

            return tag.type == TagType::Section;
        });

        if (container == currentTagInstances.end())
            CorruptedBufferException("Found Task outside Section");

        TagInstance ti;
        ti.genId();

        ti.start = tsc;
        ti.tag = tag.id;
        ti.thread = self.id;

        currentTagInstances.push_front(ti);
        tagInstanceType[ti.id] = tag.type;

        containerTagInstanceChildren[container->id].insert(ti.id);
        childInstanceParent.insert(std::make_pair(ti.id, container->id));

        interestingProgramPart = true;

        break;
    }

    case TagInstructionType::Stop:
        CorruptedBufferException("Stop is not valid for Task");
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::handlePipelineTaskTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator &tagInstance)
{
}

void ThreadManager::endCurrentSectionTaskTag(UINT64 tsc)
{
    auto tagInstance = std::find_if(currentTagInstances.begin(), currentTagInstances.end(), [&] (const TagInstance& instance)
    {
        return manager->tagIdTagMap[instance.tag].type == TagType::SectionTask;
    } );

    if (tagInstance != currentTagInstances.end())
    {
        tagInstance->end = tsc;

        manager->writer.insertTagInstance(*tagInstance);

        currentTagInstances.erase(tagInstance);
        tagInstanceType.erase(tagInstance->id);
        childInstanceParent.erase(tagInstance->id);
    }
}

void ThreadManager::endCurrentPipelineTaskTag(UINT64 tsc)
{
}

void ThreadManager::handleIgnoreAllTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        ignoreAccesses = true;
        ignoreCalls = true;

        break;
    }

    case TagInstructionType::Stop:
    {
        ignoreAccesses = false;
        ignoreCalls = false;
    }

    break;
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::handleProcessAllTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        processAccesses = true;
        processCalls = true;

        break;
    }

    case TagInstructionType::Stop:
    {
        processAccesses = false;
        processCalls = false;

        break;
    }
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::handleProcessCallsTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        processCalls = true;
        break;
    }

    case TagInstructionType::Stop:
    {
        processCalls = false;
        break;
    }
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::handleProcessAccessesTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        processAccesses = true;
        break;
    }

    case TagInstructionType::Stop:
    {
        processAccesses = false;
        break;
    }
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::handleIgnoreCallsTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        ignoreCalls = true;

        break;
    }

    case TagInstructionType::Stop:
    {
        ignoreCalls = false;
        break;
    }
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::handleIgnoreAccessesTag(const TagInstruction &tagInstruction)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        ignoreAccesses = true;
        break;
    }

    case TagInstructionType::Stop:
    {
        ignoreAccesses = false;
        break;

    }
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }

    updateChecks();
}

void ThreadManager::recordTagAccess(TagInstance &instance, ADDRINT address, int reference, int access, AccessType accessType)
{
    TagType type = tagInstanceType[instance.id];

    if(type != TagType::PipelineTask && type != TagType::SectionTask)
        return;

    auto it = tagAccessingReference.find(reference);

    if (accessType == AccessType::Write || tagAccessingReference[reference][address].find(instance.id) == tagAccessingReference[reference][address].end())
        tagAccessingReference[reference][address].insert(std::make_pair(instance.id, std::make_pair(access, accessType)));

    if (it == tagAccessingReference.end())
        // First access of reference
        return;

    auto it2 = it->second.find(address);

    if (it2->second.size() == 1)
        // Only this tags accesses at address
        return;

    int parent = childInstanceParent[instance.id];

    for(auto it3 : it2->second) {
        if (it3.first != instance.id && (accessType == AccessType::Write || it3.second.second == AccessType::Write) && it3.first != parent) {
             Conflict c;

             c.tagInstance1 = instance.id;
             c.tagInstance2 = it3.first;
             c.access2 = it3.second.first;
             c.access1 = access;

             manager->writer.insertConflict(c);
        }
    }
}

void ThreadManager::closeTagInstanceAccesses(const std::set<int> &tagInstances)
{
    for(auto& it1: tagAccessingReference) {
        for(auto& it2: it1.second) {
            for(auto& it3: tagInstances) {
                it2.second.erase(it3);
            }
        }
    }
}

void ThreadManager::insertCallTagInstance(const ThreadManager::CallData &data)
{
    for (auto& instance : currentTagInstances) {
        if (data.tagInstances.find(instance.id) != data.tagInstances.end()) {
            CallTagInstance callTagInstance;

            callTagInstance.call = data.call.id;
            callTagInstance.tagInstance = instance.id;

            manager->writer.insertCallTagInstance(callTagInstance);
        }
    }
}

void ThreadManager::updateChecks()
{
    if (ignoreCalls)
        processCallsComputed = false;
    else if (processCalls)
        processCallsComputed = true;
    else if (interestingProgramPart)
        processCallsComputed = true;
    else
        processCallsComputed = manager->processCallsByDefault;

    if (!processCallsComputed)
        processAccessesComputed = false;
    else if (ignoreAccesses)
        processAccessesComputed = false;
    else if (processAccesses)
        processAccessesComputed = true;
    else if (interestingProgramPart)
        processAccessesComputed = true;
    else
        processAccessesComputed = manager->processAccessesByDefault;
}

void ThreadManager::lock()
{
    PIN_MutexLock(&mutex);
}

void ThreadManager::unlock()
{
    PIN_MutexUnlock(&mutex);
}
