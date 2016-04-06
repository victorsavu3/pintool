#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <string>
#include <memory>
#include <cstdint>

#include <pin.H>

#include "sqlite.h"
#include "entities.h"

class SQLWriter
{
public:
    SQLWriter(const std::string& file, bool forceCreate = false);
    SQLWriter(std::shared_ptr<SQLite::Connection> db);
    ~SQLWriter();

    void begin();
    void commit();

    void insertFile(File&);
    void insertImage(Image&);
    void insertFunction(Function&);
    void insertSourceLocation(SourceLocation&);
    void insertTag(Tag&);
    void insertTagInstruction(TagInstruction&);
    void insertTagInstance(const TagInstance&);
    void insertThread(const Thread&);
    void insertCall(const Call&);
    void insertSegment(Segment&);
    void insertInstruction(Instruction&);

    void insertTagHit(ADDRINT address, UINT64 tsc, int tagId, int thread);

    int getFunctionIdByProperties(const std::string& name, int image, const std::string& file, int line);
    int getSourceLocationId(const SourceLocation& location);
    int getImageIdByName(const std::string& name);

    int functionExists(const Function&);

    void setSourceLocationId(SourceLocation& location);

    SourceLocation getSourceLocationById(int id);
private:
    std::shared_ptr<SQLite::Connection> db;

    PIN_MUTEX mutex;

    std::shared_ptr<SQLite::Statement> insertSourceLocationStmt;
    std::shared_ptr<SQLite::Statement> insertFileStmt;
    std::shared_ptr<SQLite::Statement> insertImageStmt;
    std::shared_ptr<SQLite::Statement> insertFunctionStmt;
    std::shared_ptr<SQLite::Statement> insertTagStmt;
    std::shared_ptr<SQLite::Statement> insertTagInstructionStmt;
    std::shared_ptr<SQLite::Statement> insertTagInstanceStmt;
    std::shared_ptr<SQLite::Statement> insertThreadStmt;
    std::shared_ptr<SQLite::Statement> insertCallStmt;
    std::shared_ptr<SQLite::Statement> insertSegmentStmt;
    std::shared_ptr<SQLite::Statement> insertInstructionStmt;

    std::shared_ptr<SQLite::Statement> insertTagHitStmt;

    std::shared_ptr<SQLite::Statement> getFunctionIdByPropertiesStmt;
    std::shared_ptr<SQLite::Statement> getSourceLocationIdStmt;
    std::shared_ptr<SQLite::Statement> getSourceLocationByIdStmt;
    std::shared_ptr<SQLite::Statement> getImageIdByNameStmt;

    std::shared_ptr<SQLite::Statement> functionExistsStmt;

    std::shared_ptr<SQLite::Statement> beginTransactionStmt;
    std::shared_ptr<SQLite::Statement> commitTransactionStmt;

    void prepareStatements();
    void createDatabase();

    void lock();
    void unlock();
};

#endif // SQLWRITER_H
