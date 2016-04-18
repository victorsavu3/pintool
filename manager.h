#ifndef MANAGER_H
#define MANAGER_H

#include <unordered_map>
#include <map>
#include <set>

#include <pin.H>

class Manager;
struct MemoryOperationDetails;
struct AccessInstructionDetails;
struct LocationDetails;

#include "sqlwriter.h"
#include "entities.h"
#include "filter.h"
#include "buffer.h"
#include "threadmanager.h"

struct LocationDetails {
    int functionId;
    int line;
    int column;
};

struct MemoryOperationDetails {
    ADDRINT address;
    UINT32 size;
    BOOL isRead;
};

struct AccessInstructionDetails {
    std::vector<MemoryOperationDetails> accesses;
    int location;
};

class Manager
{
public:
    Manager(const std::string& db, const std::string& source, const std::string& filter);

    SQLWriter writer;
    Filter filter;

    std::map<SourceLocation, int> sourceLocationTagInstructionIdMap;

    /* Buffer optimization */
    std::vector<LocationDetails> locationDetails;
    int getLocation(ADDRINT address, int functionId);

    /* Used in Trace */
    std::map<ADDRINT, TagBufferEntry> tagAddressesToInstrument;
    std::map<ADDRINT, CallEnterBufferEntry> callAddressesToInstrument;
    std::map<ADDRINT, RetBufferEntry> retAddressesToInstrument;
    std::map<ADDRINT, ADDRINT> callInstructionAddressesToInstrument;

    std::map<ADDRINT, int> accessToInstrument;
    std::vector<AccessInstructionDetails> accessDetails;

    std::vector<Tag> tags;
    std::map<int, Tag> tagIdTagMap;

    std::vector<TagInstruction> tagInstructions;
    std::map<int, TagInstruction> tagInstructionIdMap;

    bool processCallsByDefault;
    bool processAccessesByDefault;

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
