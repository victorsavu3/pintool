#ifndef BUFFER_H
#define BUFFER_H

#include <pin.H>

#include "entities.h"
#include "exception.h"

enum class BuferEntryType : UINT32
{
    CallEnter,
    Call,
    Ret,
    Tag,
    MemRef
};

struct CallInstructionBufferEntry
{
    UINT32 location;
    UINT64 tsc;
    UINT64 rsp;
};

struct CallEnterBufferEntry
{
    UINT32 functionId;
    UINT64 tsc;
    UINT64 rbp;
    UINT64 rsp;
};

struct RetBufferEntry
{
    UINT32 functionId;
    UINT64 tsc;
    UINT64 rsp;
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
    UINT64 rsp;
    UINT64 tsc;
};

enum class AllocType : UINT32
{
    malloc = 1,
    calloc,
    realloc,
    free
};

struct AllocData {
    AllocType type;
    UINT64 tsc;
    ADDRINT address;

    union {
        struct {
            size_t size;
        } malloc;

        struct {
            size_t size;
            size_t num;
        } calloc;

        struct {
            size_t size;
            ADDRINT ref;
        } realloc;
    };
};

union BufferEntryUnion
{
    CallInstructionBufferEntry callInstruction;
    CallEnterBufferEntry callEnter;
    RetBufferEntry ret;
    TagBufferEntry tag;
    AccessInstructionBufferEntry memref;
};

struct BufferEntry
{
    BuferEntryType type;

    BufferEntryUnion data;
};



#endif // BUFFER_H
