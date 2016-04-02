#include "entities.h"

#include <atomic>

std::atomic<int> GeneratedId(1);

void EntityWithGeneratedId::genId()
{
    id = GeneratedId++;
}
