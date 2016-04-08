#include "exception.h"

#include <iostream>

#include <pin.H>

#include "sqlite.h"
#include "asm.h"

void startDebugger() {
    debugger_trap();
}

void YAMLException(std::string file, std::string err) {
    std::cerr << "Error loading YAML file '" << file << "': " << err.c_str() << std::endl;

    startDebugger();

    PIN_WriteErrorMessage("Error loading YAML file", 1001, PIN_ERR_FATAL, 2, file.c_str(), err.c_str());
}

void CorruptedBufferException(std::string err)
{
    std::cerr << "Corrupted buffer: " << err << std::endl;

    startDebugger();

    PIN_WriteErrorMessage("Corrupted buffer", 1002, PIN_ERR_FATAL, 1, err.c_str());
}

void SQLiteException(std::string err, int code, std::string context)
{
    std::cerr << "SQL Error: " << err << std::endl << "Code: " << code << std::endl << "In context: " << context << std::endl;

    startDebugger();

    PIN_WriteErrorMessage("SQL Error", 1003, PIN_ERR_FATAL, 2, err.c_str(), context.c_str());
}

void SQLiteException(SQLite::Connection *db, int code, std::string context)
{
    SQLiteException(db->getErrorMessage(), code, context);
}

void SQLWriterException(string err, string context)
{
    std::cerr << "SQLWriter Error: " << err << std::endl << "In context: " << context << std::endl;

    startDebugger();

    PIN_WriteErrorMessage("SQLWriter Error", 1004, PIN_ERR_FATAL, 2, err.c_str(), context.c_str());
}

void Warn(string context, string err)
{
    std::cerr << context << ": " << err << std::endl;
}
