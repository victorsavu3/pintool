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

    void insertFile(File& file);
    void insertImage(Image& image);
    void insertFunction(Function& image);
    void insertSourceLocation(SourceLocation& image);

    int getFunctionIdByPrototype(const std::string& name);
    int getSourceLocationId(const SourceLocation& location);

    void setSourceLocationId(SourceLocation& location);

    void insertTag(Tag& tag);
private:
    std::shared_ptr<SQLite::Connection> db;

    PIN_MUTEX mutex;

    std::shared_ptr<SQLite::Statement> insertSourceLocationStmt;
    std::shared_ptr<SQLite::Statement> insertFileStmt;
    std::shared_ptr<SQLite::Statement> insertImageStmt;
    std::shared_ptr<SQLite::Statement> insertFunctionStmt;
    std::shared_ptr<SQLite::Statement> insertTagStmt;

    std::shared_ptr<SQLite::Statement> getFunctionIdByNameStmt;
    std::shared_ptr<SQLite::Statement> getSourceLocationIdStmt;

    void prepareStatements();
    void createDatabase();

    void lock();
    void unlock();
};

class SQLWriterException: public std::exception {
public:
    SQLWriterException(const char* err);

    virtual const char* what() const noexcept;
private:
    std::string msg;
};

#endif // SQLWRITER_H
