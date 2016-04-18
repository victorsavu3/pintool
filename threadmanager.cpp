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

    if (!callStack.empty())
    {
        CorruptedBufferException("Thread stopped before all calls returned");
    }
}

void ThreadManager::handleEntry(BufferEntry * entry)
{
    switch (entry->type)
    {
    case BuferEntryType::Tag:
        handleTag(entry->data.tag.tsc - this->startTSC, entry->data.tag.tagId);
        break;
    case BuferEntryType::Call:
        if (processCallsComputed)
            handleCall(entry->data.callInstruction.tsc - this->startTSC, (int)entry->data.callInstruction.location);
        break;
    case BuferEntryType::CallEnter:
        if (processCallsComputed)
            handleCallEnter(entry->data.callEnter.tsc - this->startTSC, entry->data.callEnter.functionId);
        break;
    case BuferEntryType::Ret:
        if (processCallsComputed)
            handleRet(entry->data.ret.tsc - this->startTSC, entry->data.ret.functionId);
        break;
    case BuferEntryType::MemRef:
        if (processAccessesComputed)
            handleMemRef((AccessInstructionDetails*)entry->data.memref.accessDetails, entry->data.memref.addresses);
        break;
    default:
        CorruptedBufferException("Invalid entry type");
    }
}

#include <sstream>

void ThreadManager::handleTag(UINT64 tsc, int tagInstructionId)
{
    if (tagInstructionId == lastTagHitId)
        return;

    lastTagHitId = tagInstructionId;

    manager->writer.insertTagHit(tsc, tagInstructionId, self.id);

    TagInstruction& tagInstruction = manager->tagInstructionIdMap[tagInstructionId];
    Tag& tag = manager->tagIdTagMap[tagInstruction.tag];

    std::list<TagInstance>::iterator tagInstance = findCurrentTagInstance(tag.id);

    switch (tag.type)
    {
    case TagType::Simple:
        handleSimpleTag(tsc, tag, tagInstruction, tagInstance);
    case TagType::Pipeline:
        handlePipelineTag(tsc, tag, tagInstruction, tagInstance);
    case TagType::Section:
        handleSectionTag(tsc, tag, tagInstruction, tagInstance);
    case TagType::Task:
        handleTaskTag(tsc, tag, tagInstruction, tagInstance);
    case TagType::IgnoreAll:
        handleIgnoreAllTag(tagInstruction);
    case TagType::ProcessAll:
        handleProcessAllTag(tagInstruction);
    case TagType::ProcessCalls:
        handleProcessCallsTag(tagInstruction);
    case TagType::ProcessAccesses:
        handleProcessAccessesTag(tagInstruction);
    case TagType::IgnoreCalls:
        handleIgnoreCallsTag(tagInstruction);
    case TagType::IgnoreAccesses:
        handleIgnoreAccessesTag(tagInstruction);
    default:
        CorruptedBufferException("Invalid tag type");
    }


}

void ThreadManager::handleCall(UINT64 tsc, int location)
{
    lastCallTSC = tsc;
    lastCallLocation = location;
}

void ThreadManager::handleCallEnter(UINT64 tsc, int functionId)
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

        i.segment = callStack.back().second;
        i.type = InstructionType::Call;
        i.line = manager->locationDetails[lastCallLocation].line;
        i.column = manager->locationDetails[lastCallLocation].column;
        manager->writer.insertInstruction(i);

        c.instruction = i.id;
        c.start = lastCallTSC;
    }

    c.function = functionId;
    c.thread = self.id;

    Segment s;

    s.call = c.id;
    s.type = SegmentType::Standard;
    manager->writer.insertSegment(s);

    callStack.push_back(std::make_pair(c, s.id));
}

void ThreadManager::handleRet(UINT64 tsc, int functionId)
{
    if (callStack.empty())
        CorruptedBufferException("Return from empty callstack");

    Call c = callStack.back().first;

    while (c.function != functionId && !callStack.empty())
    {
        std::ostringstream oss;

        oss << "Unexpected return, expected " << functionId << " got " << c.function;

        Warn("handleRet", oss.str());

        callStack.pop_back();
        c = callStack.back().first;
    }

    if (callStack.empty())
        CorruptedBufferException("Could not find call in callstack");

    callStack.pop_back();

    c.end = tsc;

    manager->writer.insertCall(c);
}

void ThreadManager::handleLocation(LocationDetails *location)
{
    if (callStack.empty())
        return;
}

void ThreadManager::handleMemRef(AccessInstructionDetails* details, ADDRINT addresses[7])
{
    if (callStack.empty())
        return;

    Instruction i;

    i.type = InstructionType::Access;
    i.segment = callStack.back().second;
    i.line = manager->locationDetails[details->location].line;
    i.column = manager->locationDetails[details->location].column;

    manager->writer.insertInstruction(i);
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

        endCurrentTaskTag(tsc);

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

        endCurrentTaskTag(tsc);

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

void ThreadManager::handleTaskTag(UINT64 tsc, const Tag &tag, const TagInstruction &tagInstruction, std::list<TagInstance>::iterator &tagInstance)
{
    switch (tagInstruction.type)
    {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end())
        {
            tagInstance->end = tsc;

            manager->writer.insertTagInstance(*tagInstance);

            currentTagInstances.erase(tagInstance);
        }

        auto container = std::find_if(currentTagInstances.begin(), currentTagInstances.end(), [&] (const TagInstance& instance)
        {
            const Tag& tag = manager->tagIdTagMap[instance.tag];

            return tag.type == TagType::Pipeline || tag.type == TagType::Section;
        });

        if (container == currentTagInstances.end())
            CorruptedBufferException("Found Task outside Pipeline or Section");

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
        CorruptedBufferException("Stop is not valid for Task");
    default:
        CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::endCurrentTaskTag(UINT64 tsc)
{
    auto tagInstance = std::find_if(currentTagInstances.begin(), currentTagInstances.end(), [&] (const TagInstance& instance)
    {
        return manager->tagIdTagMap[instance.tag].type == TagType::Task;
    } );

    if (tagInstance != currentTagInstances.end())
    {
        tagInstance->end = tsc;

        manager->writer.insertTagInstance(*tagInstance);

        currentTagInstances.erase(tagInstance);
    }
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

    if (ignoreAccesses)
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
