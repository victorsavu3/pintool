#include "sqlwriter.h"

#include <iostream>

SQLWriter::SQLWriter(const std::string& file, bool forceCreate) : db(std::make_shared<SQLite::Connection>(file.c_str(), forceCreate)) {
    PIN_MutexInit(&mutex);

    createDatabase();
    prepareStatements();
}

SQLWriter::SQLWriter(std::shared_ptr<SQLite::Connection> db) : db(db) {
    PIN_MutexFini(&mutex);

    createDatabase();
    prepareStatements();
}

void SQLWriter::prepareStatements() {
    this->insertImageStmt = this->db->makeStatement("INSERT INTO Image(Name) VALUES(?);");
    this->insertFileStmt = this->db->makeStatement("INSERT INTO File(Path, Image) VALUES(?, ?);");
    this->insertFunctionStmt = this->db->makeStatement("INSERT INTO Function(Name, Prototype, File, Line) VALUES(?, ?, ?, ?);");
    this->insertSourceLocationStmt = this->db->makeStatement("INSERT INTO SourceLocation(Function, Line, Column) VALUES(?, ?, ?);");

    this->getFunctionIdByNameStmt = this->db->makeStatement("SELECT Id FROM Function WHERE Prototype = ?");
    this->getSourceLocationIdStmt = this->db->makeStatement("SELECT Id FROM SourceLocation WHERE Function = ? AND Line = ? AND Column = ?");
}


SQLWriter::~SQLWriter()
{

}

void SQLWriter::createDatabase() {
    this->db->execute(
        "CREATE TABLE IF NOT EXISTS Image(Id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, Name VARCHAR);"
        "CREATE TABLE IF NOT EXISTS File(Id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, Path VARCHAR, Image INTEGER);"
        "CREATE TABLE IF NOT EXISTS Function(Id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, Name VARCHAR, Prototype VARCHAR, File INTEGER, Line INTEGER);"
        "CREATE TABLE IF NOT EXISTS SourceLocation(Id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, Function INTEGER, Line INTEGER, Column INTEGER, UNIQUE(Function, Line, Column) ON CONFLICT IGNORE);"
        "CREATE TABLE IF NOT EXISTS Tag(Id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, Name VARCHAR, Location INTEGER, Type INTEGER);"
                );
}

void SQLWriter::lock()
{
    PIN_MutexLock(&mutex);
}

void SQLWriter::unlock()
{
    PIN_MutexUnlock(&mutex);
}

void SQLWriter::insertImage(Image &image)
{
    lock();

    insertImageStmt << image.name;
    image.id = insertImageStmt->executeInsert();

    unlock();
}

void SQLWriter::insertFile(File &file)
{
    lock();

    insertFileStmt << file.name << file.image;
    file.id = insertFileStmt->executeInsert();

    unlock();
}

void SQLWriter::insertFunction(Function &function)
{
    lock();

    insertFunctionStmt << function.name << function.prototype << function.file << function.line;
    function.id = insertFunctionStmt->executeInsert();

    unlock();
}

void SQLWriter::insertSourceLocation(SourceLocation &location)
{
    lock();

    insertSourceLocationStmt << location.function << location.line << location.column;
    location.id = insertSourceLocationStmt->executeInsert();

    unlock();
}

int SQLWriter::getFunctionIdByPrototype(const string &name)
{
    lock();

    int Id;

    getFunctionIdByNameStmt << name;
    if (getFunctionIdByNameStmt->stepRow()) {
        Id = getFunctionIdByNameStmt->columnInt(0);
    }

    if (getFunctionIdByNameStmt->stepRow()) {
        throw SQLWriterException("Too many rows returned in getFunctionIdByPrototype");
    } else {
        throw SQLWriterException("Could not find row in getFunctionIdByPrototype");
    }

    getFunctionIdByNameStmt->reset();
    getFunctionIdByNameStmt->clearBindings();

    unlock();

    return Id;
}

int SQLWriter::getSourceLocationId(const SourceLocation &location)
{
    lock();

    int Id;

    getSourceLocationIdStmt << location.function << location.line << location.column;
    if (getSourceLocationIdStmt->stepRow()) {
        Id = getSourceLocationIdStmt->columnInt(0);
    } else {
        throw SQLWriterException("Could not find row in setSourceLocationId");
    }

    if (getSourceLocationIdStmt->stepRow()) {
        throw SQLWriterException("Too many rows returned in setSourceLocationId");
    }

    getSourceLocationIdStmt->reset();
    getSourceLocationIdStmt->clearBindings();

    unlock();

    return Id;
}

void SQLWriter::setSourceLocationId(SourceLocation &location)
{
    location.id = getSourceLocationId(location);
}


SQLWriterException::SQLWriterException(const char *err) : msg(err)
{
}

const char *SQLWriterException::what() const noexcept
{
    return msg.c_str();
}
