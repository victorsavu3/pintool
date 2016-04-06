#include "sqlite.h"

#include <sstream>
#include <unistd.h>

#include <sqlite3.h>

#include "exception.h"

namespace SQLite {

NullClass SQLNULL;

Connection::Connection(const char * location, bool forceCreate) {
    if (forceCreate) {
        if( access( location, F_OK ) != -1 ) {
            SQLiteException("Database already exists", 0, "Connection constructor");
        }
    }

    int code;
    if (code = sqlite3_open_v2(location, &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        SQLiteException(this, code, "Connection constructor");

    PIN_MutexInit(&mutex);
}

Connection::~Connection() {
    int code;
    if (code = sqlite3_close(this->db) != SQLITE_OK)
        SQLiteException(this, code, "Connection destructor");

    PIN_MutexFini(&mutex);
}

const char* Connection::getErrorMessage() {
    return sqlite3_errmsg(this->db);
}

std::shared_ptr<Statement> Connection::makeStatement(const char* sql) {
    return std::make_shared<Statement>(this->shared_from_this(), sql);
}

int Connection::lastInsertedROWID()
{
    return sqlite3_last_insert_rowid(this->db);
}

void Connection::execute(const char *sql)
{
    lock();

    char* err;
    if (int code = sqlite3_exec(this->db, sql, NULL, NULL, &err) != SQLITE_OK) {
        SQLiteException(err, code, "execute");
    }

    unlock();
}

void Connection::lock()
{
    PIN_MutexLock(&mutex);
}

void Connection::unlock()
{
    PIN_MutexUnlock(&mutex);
}

Statement::Statement(std::shared_ptr<Connection> connection, const char* sql) : connection(connection) {
    connection->lock();

    int code;
    if  (code = sqlite3_prepare_v2(connection->db, sql, -1, &this->stmt, NULL) != SQLITE_OK) {
        SQLiteException(this->connection.get(), code, "Statemenet constructor");
    }

    columnCount = sqlite3_column_count(stmt);
    lastBound = -1;

    connection->unlock();
}

Statement::~Statement() {
    connection->lock();

    if (int code = sqlite3_finalize(this->stmt) != SQLITE_OK)
         SQLiteException(this->connection.get(), code, "Statemenet destructor");

    connection->unlock();
}

void Statement::bind(int pos, int64_t val) {
    connection->lock();

    if (int code = sqlite3_bind_int64(this->stmt, pos, (sqlite3_int64)val) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "bind int64_t");

    connection->unlock();
}

void Statement::bind(int pos, int val ) {
    connection->lock();

    if (int code = sqlite3_bind_int(this->stmt, pos, val) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "bind int");

    connection->unlock();
}

void Statement::bind(int pos, const std::string& val) {
    connection->lock();

    if (int code = sqlite3_bind_text(this->stmt, pos, val.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "bind std::string");

    connection->unlock();
}

void Statement::bind(int pos, const char* val) {
    connection->lock();

    if (int code = sqlite3_bind_text(this->stmt, pos, val, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "bind char*");

    connection->unlock();
}

void Statement::bind(int pos, struct timespec val)
{
    char timeString[100];
    char timeStringWithNanoseconds[200];
    struct tm t;

    gmtime_r(&(val.tv_sec), &t);

    strftime(timeString, sizeof(timeString), "%FT%T", &t);
    snprintf(timeStringWithNanoseconds, sizeof(timeStringWithNanoseconds), "%s:%ldZ", timeString, val.tv_nsec);

    bind(pos, timeStringWithNanoseconds);
}

void Statement::bindNULL(int col)
{
    connection->lock();

    if (int code = sqlite3_bind_null(this->stmt, col) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "bindNULL");

    connection->unlock();
}

void Statement::checkColumn(int col)
{
    if(columnCount <= col)
        SQLiteException("Request for column outside row", 0, "bind char*");

    if (col > lastBound) {
        lastBound = col;
    }
}

int Statement::columnInt(int col)
{
    checkColumn(col);

    connection->lock();

    int val = sqlite3_column_int(stmt, col);

    connection->unlock();

    return val;
}

void Statement::execute() {
    connection->lock();

    this->stepUnlocked();
    this->resetUnlocked();
    this->clearBindingsUnlocked();

    connection->unlock();
}

int Statement::executeInsert()
{
    connection->lock();

    this->stepUnlocked();
    this->resetUnlocked();
    this->clearBindingsUnlocked();

    int lastRow = connection->lastInsertedROWID();

    connection->unlock();

    return lastRow;

}

void Statement::reset()
{
    connection->lock();

    resetUnlocked();

    connection->unlock();
}

void Statement::clearBindings()
{
    lastBound = -1;

    connection->lock();

    clearBindingsUnlocked();

    connection->unlock();
}

void Statement::step()
{
    connection->lock();

    stepUnlocked();

    connection->unlock();
}

bool Statement::stepRow()
{
    connection->lock();

    bool isRow = stepRowUnlocked();

    connection->unlock();

    return isRow;
}

void Statement::resetUnlocked() {
    if (int code = sqlite3_reset(this->stmt) != SQLITE_OK)
        SQLiteException(this->connection.get(), code, "reset");
}

void Statement::clearBindingsUnlocked() {
    if (int code = sqlite3_clear_bindings(this->stmt) != SQLITE_OK)
       SQLiteException(this->connection.get(), code, "clearBindings");
}

void Statement::stepUnlocked() {
    if(lastBound != columnCount - 1)
        SQLiteException("Not all parameters bound", 0, "stepUnlocked");

    if (int code = sqlite3_step(this->stmt) != SQLITE_DONE)
        SQLiteException(this->connection.get(), code, "stepUnlocked");
}

bool Statement::stepRowUnlocked()
{
    int code = sqlite3_step(this->stmt);
    if (code == SQLITE_ROW)
        return true;
    else if (code == SQLITE_DONE)
        return false;
    else
        SQLiteException(this->connection.get(), code, "stepRow");
}

}
