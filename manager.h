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
struct ReferenceData;

#include "sqlwriter.h"
#include "entities.h"
#include "filter.h"
#include "buffer.h"
#include "threadmanager.h"

struct LocationDetails
{
    int functionId;
    int line;
    int column;
};

struct MemoryOperationDetails
{
    ADDRINT address;
    UINT32 size;
    BOOL isRead;
};

struct AccessInstructionDetails
{
    std::vector<MemoryOperationDetails> accesses;
    int location;
};

struct ReferenceData {
    ReferenceData() {
        wasAccessed = false;
    }

      Reference ref;
      bool wasAccessed;
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
    std::set<ADDRINT> freeEnterAddresesToInstrument;
    std::map<ADDRINT, AllocEntryType> allocEnterAddresesToInstrument;

    std::map<THREADID, AllocEnterBufferEntry> knownAllocationsInProgess;
    std::map<AllocEnterBufferEntry, std::map<UINT64, ADDRINT> > knownAllocations;
    void lockKnownAllocations();
    void unlockKnownAllocations();

    std::map<ADDRINT, ReferenceData> references;
    ReferenceData redZone;
    void lockReferences();
    void unlockReferences();

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
    void writeRedZone();

    std::map<THREADID, ThreadManager> threadmanagers;

    PIN_MUTEX mutex;
    PIN_MUTEX knownAllocationsLock;
    PIN_MUTEX referencesLock;
};

#endif // MANAGER_H
