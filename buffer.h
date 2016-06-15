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
    MemRef,
    AllocEnter,
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
    malloc = 1,
    calloc,
    realloc
};

struct AllocEnterBufferEntry
{
    AllocEntryType type;
    UINT64 tsc;
    THREADID thread;

    UINT64 num;
    UINT64 size;
    ADDRINT ref;
};

inline bool operator==(const AllocEnterBufferEntry & lhs, const AllocEnterBufferEntry & rhs )
{
    if (lhs.type != rhs.type)
        return false;

    switch(lhs.type) {
        case AllocEntryType::malloc:
            return std::tie(lhs.size, lhs.thread) == std::tie(rhs.size, rhs.thread);
        case AllocEntryType::calloc:
            return std::tie(lhs.size, rhs.num, lhs.thread) == std::tie(rhs.size, rhs.num, rhs.thread);
        case AllocEntryType::realloc:
            return std::tie(lhs.size, lhs.ref, lhs.thread) == std::tie(rhs.size, rhs.ref, rhs.thread);
        default:
            CorruptedBufferException("Invalid AllocEntryType");
    }
}

inline bool operator!=(const AllocEnterBufferEntry & lhs, const AllocEnterBufferEntry & rhs )
{
    return !(lhs == rhs);
}

inline bool operator<(const AllocEnterBufferEntry & lhs, const AllocEnterBufferEntry & rhs )
{
    if (lhs.type >= rhs.type)
        return false;

    switch(lhs.type) {
    case AllocEntryType::malloc:
        return std::tie(lhs.size, lhs.thread) < std::tie(rhs.size, rhs.thread);
    case AllocEntryType::calloc:
        return std::tie(lhs.size, rhs.num, lhs.thread) < std::tie(rhs.size, rhs.num, rhs.thread);
    case AllocEntryType::realloc:
        return std::tie(lhs.size, lhs.ref, lhs.thread) < std::tie(rhs.size, rhs.ref, rhs.thread);
    default:
        CorruptedBufferException("Invalid AllocEntryType");
    }
}

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
    FreeBufferEntry free;
};

struct BufferEntry
{
    BuferEntryType type;

    BufferEntryUnion data;
};



#endif // BUFFER_H
