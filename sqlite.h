#ifndef CONNECTION_H
#define CONNECTION_H

#include <memory>
#include <string>

#include <pin.H>

struct sqlite3;
struct sqlite3_stmt;

namespace SQLite {

class Connection;
class Statement;
class StatementBinder;

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    Connection(const char* location, bool forceCreate = false);
    ~Connection();

    const char* getErrorMessage();
    std::shared_ptr<Statement> makeStatement(const char* sql);

    int lastInsertedROWID();

    void execute(const char* sql);

    void lock();
    void unlock();
private:
    sqlite3* db;

    PIN_MUTEX mutex;

    friend class Statement;
};

class Statement : public std::enable_shared_from_this<Statement>
{
public:
    Statement(std::shared_ptr<Connection> connection, const char* sql);
    ~Statement();

    void bind(int, int64_t);
    void bind(int, int);
    void bind(int, const std::string&);
    void bind(int, const char*);

    int columnInt(int);

    void execute();
    int executeInsert();

    void reset();
    void clearBindings();
    void step();
    bool stepRow();

    void resetUnlocked();
    void clearBindingsUnlocked();
    void stepUnlocked();

    bool stepRowUnlocked();
private:
    sqlite3_stmt* stmt;
    std::shared_ptr<Connection> connection;
};

class StatementBinder
{
public:
    StatementBinder(std::shared_ptr<Statement>);

    template <typename T>
    StatementBinder& operator<<(const T& obj) {
        stmt->bind(at++, obj);

        return *this;
    }
private:
    int at;
    std::shared_ptr<Statement> stmt;
};

template <typename T>
StatementBinder operator<<(Statement& stmt, const T& obj) {
    StatementBinder binder(stmt.shared_from_this());

    binder << obj;

    return binder;
}

template <typename T>
StatementBinder operator<<(std::shared_ptr<Statement>& stmt, const T& obj) {
    StatementBinder binder(stmt);

    binder << obj;

    return binder;
}

class SQLiteException: public std::exception {
public:
    SQLiteException(Connection* db, int error_code);
    SQLiteException(const char* err, int error_code);

    virtual const char* what() const noexcept;
private:
    int error_code;
    std::string error_message;
    std::string msg;
};


}
#endif // CONNECTION_H
