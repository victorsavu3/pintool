#ifndef BUFFER_H
#define BUFFER_H

#include <pin.H>

#include "entities.h"

enum class BuferEntryType : UINT32
{
    CallEnter,
    Call,
    Ret,
    Tag,
    MemRef,
    AllocEnter,
    AllocExit,
    Free
};

struct CallInstructionBufferEntry
{
    UINT32 location;
    UINT64 tsc;
};

struct CallEnterBufferEntry
{
    UINT32 functionId;
    UINT64 tsc;
    UINT64 rbp;
};

struct RetBufferEntry
{
    UINT32 functionId;
    UINT64 tsc;
};

struct TagBufferEntry
{
    UINT32 tagId;
    UINT64 tsc;
    ADDRINT address;
};

struct AccessInstructionBufferEntry
{
    ADDRINT accessDetails;
    ADDRINT addresses[7];
};

enum class AllocEntryType : UINT32
{
    malloc,
    calloc,
    realloc
};

struct AllocEnterBufferEntry
{
    AllocEntryType type;

    UINT64 num;
    UINT64 size;
    ADDRINT ref;
};

struct AllocExitBufferEntry
{
    ADDRINT ref;
};

struct FreeBufferEntry
{
    ADDRINT ref;
};

union BufferEntryUnion
{
    BuferEntryType type;
    CallInstructionBufferEntry callInstruction;
    CallEnterBufferEntry callEnter;
    RetBufferEntry ret;
    TagBufferEntry tag;
    AccessInstructionBufferEntry memref;
    AllocEnterBufferEntry allocenter;
    AllocExitBufferEntry allocexit;
    FreeBufferEntry free;
};

struct BufferEntry
{
    BuferEntryType type;

    BufferEntryUnion data;
};



#endif // BUFFER_H
