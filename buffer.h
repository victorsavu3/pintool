#ifndef BUFFER_H
#define BUFFER_H

#include <pin.H>

#include "entities.h"

enum class BuferEntryType : UINT32 {
    Call,
    Ret,
    Tag,
    MemRef,
    AllocEnter,
    AllocExit,
    Free
};

struct CallBufferEntry {
    UINT32 functionId;
};

struct RetBufferEntry {
};

struct TagBufferEntry {
    UINT32 tagId;
};

struct MemRefBufferEntry {
    ADDRINT ref;
    UINT32 size;
    bool isRead;
};

struct AllocEnterBufferEntry {
    UINT32 size;
};

struct AllocExitBufferEntry {
    ADDRINT ref;
};

struct FreeBufferEntry {
    ADDRINT ref;
};

union BufferEntryUnion {
    BuferEntryType type;
    CallBufferEntry call;
    RetBufferEntry ret;
    TagBufferEntry tag;
    MemRefBufferEntry memref;
    AllocEnterBufferEntry allocenter;
    AllocExitBufferEntry allocexit;
    FreeBufferEntry free;
};

struct BufferEntry {
    BuferEntryType type;
    ADDRINT instruction;
    UINT64 tsc;

    BufferEntryUnion data;
};



#endif // BUFFER_H
