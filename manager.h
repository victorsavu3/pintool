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

    /* Used in Trace */
    std::map<ADDRINT, TagBufferEntry> tagAddressesToInstrument;
    std::map<ADDRINT, CallBufferEntry> callAddressesToInstrument;
    std::map<ADDRINT, RetBufferEntry> retAddressesToInstrument;

    struct AccessStaticData {
        UINT32 size;
        BOOL isRead;
        UINT32 memOp;
    };
    std::multimap<ADDRINT, AccessStaticData> accessToInstrument;

    std::vector<Tag> tags;
    std::map<int, Tag> tagIdTagMap;

    std::vector<TagInstruction> tagInstructions;
    std::map<int, TagInstruction> tagInstructionIdMap;

    void bufferFull(struct BufferEntry*, UINT64, THREADID);

    void setUpThreadManager(THREADID);
    void tearDownThreadManager(THREADID);

    void lock();
    void unlock();
private:
    /* Called in constructor */
    void loadTags(const std::string& file);
    void writeTags();
    void loadSourceLocationTagIdMap();
    void loadTagIdTagMap();
    void loadTagInstructionIdMap();

    std::map<THREADID, ThreadManager> threadmanagers;

    PIN_MUTEX mutex;
};

#endif // MANAGER_H
