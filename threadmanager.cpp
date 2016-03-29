#include "threadmanager.h"

#include <time.h>

#include "exception.h"
#include "asm.h"

ThreadManager::ThreadManager(Manager *manager, THREADID tid) : manager(manager), tid(tid)
{
    PIN_MutexInit(&mutex);

    startTSC = rdtsc();
    clock_gettime(CLOCK_REALTIME, &startTime);
}

void ThreadManager::bufferFull(BufferEntry * entries, UINT64 count)
{
    lock();
    for(UINT64 i = 0; i < count; i++) {
        handleEntry(&entries[i]);
    }
    unlock();
}

void ThreadManager::handleEntry(BufferEntry * entry)
{
    UINT64 passed = entry->tsc - startTSC;

    switch (entry->type) {
    case BuferEntryType::Tag:
        handleTag(entry->instruction, passed, entry->data.tag.tagId);
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

    manager->writer.insertTagHit(instruction, tsc, tagInstructionId);

    TagInstruction& tagInstruction = manager->tagInstructionIdMap[tagInstructionId];
    Tag& tag = manager->tagIdTagMap[tagInstruction.tag];

    switch (tagInstruction.type) {
        case TagInstructionType::Start:
            tags.insert(tagInstruction.tag);

            break;
        case TagInstructionType::Stop:
            tags.erase(tagInstruction.tag);

            break;
        default:
            CorruptedBufferException("Invalid type for TagInstruction");
    }
}

void ThreadManager::lock()
{
    PIN_MutexLock(&mutex);
}

void ThreadManager::unlock()
{
    PIN_MutexUnlock(&mutex);
}
