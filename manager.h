#ifndef MANAGER_H
#define MANAGER_H

#include <unordered_map>
#include <map>
#include <set>

#include <pin.H>

class Manager;

#include "sqlwriter.h"
#include "entities.h"
#include "filter.h"
#include "buffer.h"
#include "threadmanager.h"

class Manager
{
public:
    Manager(const std::string& db, const std::string& source, const std::string& filter);

    SQLWriter writer;
    Filter filter;

    std::map<SourceLocation, int> sourceLocationTagInstructionIdMap;

    std::vector<Tag> tags;
    std::map<int, Tag> tagIdTagMap;

    std::vector<TagInstruction> tagInstructions;
    std::map<int, TagInstruction> tagInstructionIdMap;

      void bufferFull(struct BufferEntry*, UINT64, THREADID);

    void setUpThreadManager(THREADID);
    void tearDownThreadManager(THREADID);
private:
    /* Called in constructor */
    void loadTags(const std::string& file);
    void writeTags();
    void loadSourceLocationTagIdMap();
    void loadTagIdTagMap();

    std::map<THREADID, ThreadManager> threadmanagers;

    void lock();
    void unlock();

    PIN_MUTEX mutex;
};

#endif // MANAGER_H
