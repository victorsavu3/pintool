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
    UINT64 passed = entry->tsc - startTSC;

    switch (entry->type) {
    case BuferEntryType::Tag:
        handleTag(entry->instruction, passed, entry->data.tag.tagId);
        break;
    case BuferEntryType::Call:
        handleCallEnter(entry->instruction, passed, entry->data.call.functionId);
        break;
    case BuferEntryType::Ret:
        handleRet(entry->instruction, passed);
        break;
    default:
        CorruptedBufferException("Invalid entry type");
    }
}

void ThreadManager::handleTag(ADDRINT instruction, UINT64 tsc, int tagInstructionId)
{
    if (tagInstructionId == lastTagHitId)
        return;

    lastTagHitId = tagInstructionId;

    manager->writer.insertTagHit(instruction, tsc, tagInstructionId, self.id);

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

void ThreadManager::handleCallEnter(ADDRINT instruction, UINT64 tsc, int functionId)
{
    Call c;
    c.genId();

    if(callStack.empty()) {
        c.instruction = -1;
    } else {
        Instruction i;
        Segment s;

        s.call = callStack.back().id;
        s.type = SegmentType::Standard;
        manager->writer.insertSegment(s);

        i.segment = s.id;
        i.type = InstructionType::Call;
        i.tsc = tsc;
        manager->writer.insertInstruction(i);

        c.instruction = i.id;
    }

    c.function = functionId;
    c.start = tsc;
    c.thread = self.id;

    callStack.push_back(c);
}

void ThreadManager::handleRet(ADDRINT instruction, UINT64 tsc)
{
    if (callStack.empty())
        CorruptedBufferException("Found Return without call");

    Call c = callStack.back();
    callStack.pop_back();

    c.end = tsc;

    manager->writer.insertCall(c);
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
