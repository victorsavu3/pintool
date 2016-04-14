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
}

ThreadManager::~ThreadManager()
{
    PIN_MutexFini(&mutex);
}

void ThreadManager::bufferFull(BufferEntry * entries, UINT64 count)
{
    lock();
    for(UINT64 i = 0; i < count; i++) {
        handleEntry(&entries[i]);
    }
    unlock();
}


void ThreadManager::threadStopped()
{
    self.endTSC = rdtsc();
    clock_gettime(CLOCK_REALTIME, &self.endTime);

    manager->writer.insertThread(self);

    if (!callStack.empty()) {
        CorruptedBufferException("Thread stopped before all calls returned");
    }
}

void ThreadManager::handleEntry(BufferEntry * entry)
{
    switch (entry->type) {
    case BuferEntryType::Tag:
        handleTag(entry->data.tag.tsc - this->startTSC, entry->data.tag.tagId);
        break;
    case BuferEntryType::Call:
        handleCallEnter(entry->data.call.tsc - this->startTSC, entry->data.call.functionId);
        break;
    case BuferEntryType::Ret:
        handleRet(entry->data.ret.tsc - this->startTSC, entry->data.ret.functionId);
        break;
    case BuferEntryType::MemRef:
        handleMemRef(entry->data.memref.address, entry->data.memref.size, entry->data.memref.isRead);
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

    switch (tagInstruction.type) {
    case TagInstructionType::Start:
    {
        if (tagInstance != currentTagInstances.end()) {
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
        if (tagInstance == currentTagInstances.end()) {
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

void ThreadManager::handleCallEnter(UINT64 tsc, int functionId)
{
    Call c;
    c.genId();

    if(callStack.empty()) {
        c.instruction = -1;
    } else {
        Instruction i;

        i.segment = callStack.back().second;
        i.type = InstructionType::Call;
        manager->writer.insertInstruction(i);

        c.instruction = i.id;
    }

    c.function = functionId;
    c.start = tsc;
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

    while (c.function != functionId && !callStack.empty()) {
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

void ThreadManager::handleMemRef(ADDRINT address, UINT32 size, BOOL isRead)
{
    Instruction i;

    i.segment = callStack.back().second;
    i.type = InstructionType::Access;
    manager->writer.insertInstruction(i);
}

std::list<TagInstance>::iterator ThreadManager::findCurrentTagInstance(int tagId)
{
    for(auto tagInstance = currentTagInstances.begin(); tagInstance != currentTagInstances.end(); tagInstance++) {
        if(tagInstance->tag == tagId)
            return tagInstance;
    }

    return currentTagInstances.end();
}

void ThreadManager::lock()
{
    PIN_MutexLock(&mutex);
}

void ThreadManager::unlock()
{
    PIN_MutexUnlock(&mutex);
}
