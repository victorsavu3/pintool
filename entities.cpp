#include "entities.h"

std::atomic<int> TagInstanceIdCounter(1);

void TagInstance::genId()
{
    id = TagInstanceIdCounter++;
}

std::atomic<int> ThreadIdCounter(1);

void Thread::genId()
{
    id = ThreadIdCounter++;
}
