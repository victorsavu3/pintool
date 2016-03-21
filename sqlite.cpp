#include "sqlite.h"

#include <sstream>
#include <unistd.h>

#include <sqlite3.h>

namespace SQLite {

SQLiteException::SQLiteException(Connection* db, int error_code)
{
    this->error_code = error_code;
    this->error_message = db->getErrorMessage();

    std::ostringstream stream;

    stream << "SQLite error" << std::endl << "Code: " << error_code << std::endl << "Message: " << this->error_message;

    this->msg = stream.str();
}

SQLiteException::SQLiteException(const char* err, int error_code)
{
    this->error_code = error_code;
    this->error_message = err;

    std::ostringstream stream;

    stream << "SQLite error" << std::endl << "Code: " << error_code << std::endl << "Message: " << this->error_message;

    this->msg = stream.str();
}

const char *SQLiteException::what() const noexcept
{
    return msg.c_str();
}

Connection::Connection(const char * location, bool forceCreate) {
    if (forceCreate) {
        if( access( location, F_OK ) != -1 ) {
            throw std::invalid_argument("Database already exists");
        }
    }

    int code;
    if (code = sqlite3_open_v2(location, &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        throw SQLiteException(this, code);

    PIN_MutexInit(&mutex);
}

Connection::~Connection() {
    int code;
    if (code = sqlite3_close(this->db) != SQLITE_OK)
        throw SQLiteException(this, code);

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
        throw SQLiteException(err, code);
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
        throw SQLiteException(this->connection.get(), code);
    }

    connection->unlock();
}

Statement::~Statement() {
    connection->lock();

    if (int code = sqlite3_finalize(this->stmt) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);

    connection->unlock();
}

void Statement::bind(int pos, int64_t val) {
    connection->lock();

    if (int code = sqlite3_bind_int64(this->stmt, pos, (sqlite3_int64)val) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);

    connection->unlock();
}

void Statement::bind(int pos, int val ) {
    connection->lock();

    if (int code = sqlite3_bind_int(this->stmt, pos, val) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);

    connection->unlock();
}

void Statement::bind(int pos, const std::string& val) {
    connection->lock();

    if (int code = sqlite3_bind_text(this->stmt, pos, val.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);

    connection->unlock();
}

void Statement::bind(int pos, const char* val) {
    connection->lock();

    if (int code = sqlite3_bind_text(this->stmt, pos, val, -1, SQLITE_TRANSIENT) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);

    connection->unlock();
}

int Statement::columnInt(int col)
{
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
        throw SQLiteException(this->connection.get(), code);
}

void Statement::clearBindingsUnlocked() {
    if (int code = sqlite3_clear_bindings(this->stmt) != SQLITE_OK)
        throw SQLiteException(this->connection.get(), code);
}

void Statement::stepUnlocked() {
    if (int code = sqlite3_step(this->stmt) != SQLITE_DONE)
        throw SQLiteException(this->connection.get(), code);
}

bool Statement::stepRowUnlocked()
{
    int code = sqlite3_step(this->stmt);
    if (code == SQLITE_ROW)
        return true;
    else if (code == SQLITE_DONE)
        return false;
    else
        throw SQLiteException(this->connection.get(), code);
}

StatementBinder::StatementBinder(std::shared_ptr<Statement> stmt) : stmt(stmt) {
    this->at = 1;
}

}
