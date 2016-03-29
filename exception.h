#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>

void YAMLException(std::string file, std::string err);
void CorruptedBufferException(std::string err);

namespace SQLite {
    class Connection;
}

void SQLiteException(std::string err, int code, std::string context);
void SQLiteException(SQLite::Connection* db, int code, std::string context);

void SQLWriterException(std::string err, std::string context);

#endif // EXCEPTION_H
