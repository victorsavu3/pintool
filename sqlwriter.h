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

    void insertFile(File&);
    void insertImage(Image&);
    void insertFunction(Function&);
    void insertSourceLocation(SourceLocation&);
    void insertTag(Tag&);
    void insertTagInstruction(TagInstruction&);
    void insertTagInstance(TagInstance&);
    void insertThread(Thread&);

    void insertTagHit(ADDRINT address, UINT64 tsc, int tagId, int thread);

    int getFunctionIdByPrototype(const std::string& name);
    int getSourceLocationId(const SourceLocation& location);

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

    std::shared_ptr<SQLite::Statement> insertTagHitStmt;

    std::shared_ptr<SQLite::Statement> getFunctionIdByNameStmt;
    std::shared_ptr<SQLite::Statement> getSourceLocationIdStmt;
    std::shared_ptr<SQLite::Statement> getSourceLocationByIdStmt;

    void prepareStatements();
    void createDatabase();

    void lock();
    void unlock();
};

#endif // SQLWRITER_H
