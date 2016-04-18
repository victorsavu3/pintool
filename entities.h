#ifndef ENTITIES_H
#define ENTITIES_H

#include <string>
#include <functional>
#include <tuple>

#include <time.h>

#include <pin.H>

class EntityWithGeneratedId {
public:
    int id;
    void genId();
};

class Image {
public:
    int id;
    std::string name;
};

class File {
public:
    int id;
    std::string name;

    int image;
};

class Function {
public:
    int id;
    std::string name;
    std::string prototype;

    int file;
    int line;
};

class SourceLocation {
public:
    int id;

    int function;
    int line;
    int column;
};

enum class TagType {
    Simple = 0,
    Counter = 1,
    Section = 2,
    Pipeline = 3,
    IgnoreAll = 4,
    IgnoreCalls = 5,
    IgnoreAccesses = 6,
    ProcessAll = 7,
    ProcessCalls = 8,
    ProcessAccesses = 9,
    Task = 10
};

class Tag {
public:
    int id;

    std::string name;
    TagType type;
};

enum class TagInstructionType {
    Start = 0,
    Stop = 1
};

class TagInstruction {
public:
    int id;

    int location;
    int tag;
    TagInstructionType type;
};

class TagInstance : public EntityWithGeneratedId {
public:
    int thread;
    int tag;
    UINT64 start;
    UINT64 end;

    /* Counter tag */
    int counter;
};

class Thread : public EntityWithGeneratedId {
public:
    int instruction;
    struct timespec startTime;

    struct timespec endTime;
    UINT64 endTSC;
};

class Call : public EntityWithGeneratedId {
public:
    int thread;
    int instruction;
    int function;
    UINT64 start, end;
};

enum class SegmentType {
    Standard = 0,
    Loop = 1
};

class Segment {
public:
    int id;
    int call;
    SegmentType type;
};

enum class InstructionType {
    Call    = 0,
    Access  = 1,
    Alloc   = 2,
    Free    = 3
};

class Instruction {
public:
    int id;

    InstructionType type;
    int segment;
    int line;
    int column;
};

namespace std {
template <>
struct hash<SourceLocation>
{
    std::size_t operator()(const SourceLocation& s) const
    {
        return (((s.function << 1) ^ s.line) << 1) ^ s.column;
    }
};
}


inline bool operator==(const SourceLocation & lhs, const SourceLocation & rhs ) {
    return std::tie(lhs.function, lhs.line, lhs.column) == std::tie(rhs.function, rhs.line, rhs.column);
}

inline bool operator!=(const SourceLocation & lhs, const SourceLocation & rhs ) {
    return !(lhs == rhs);
}

inline bool operator<(const SourceLocation & lhs, const SourceLocation & rhs ) {
    return std::tie(lhs.function, lhs.line, lhs.column) < std::tie(rhs.function, rhs.line, rhs.column);
}

#endif // ENTITIES_H
