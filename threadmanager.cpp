#include "threadmanager.h"

#include <time.h>

#include "exception.h"
#include "asm.h"

ThreadManager::ThreadManager(Manager *manager, THREADID tid) : manager(manager), tid(tid)
{
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

void ThreadManager::handleTag(ADDRINT instruction, UINT64 tsc, int tagId)
{
    if (tagId == lastTagHitId)
        return;

    tagId = lastTagHitId;

    manager->writer.insertTagHit(instruction, tsc, tagId);

    Tag& tag = manager->tagIdTagMap[tagId];

    switch (tag.type) {

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
