#ifndef ENTITIES_H
#define ENTITIES_H

#include <string>
#include <functional>

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

enum TagType { Simple, Counter };

class Tag {
  public:
    int id;

    std::string name;
    TagType type;
};

enum TagInstructionType { Start, Stop };

class TagInstruction {
public:
    int id;

    int location;
    int tag;
    TagInstructionType type;
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
    return lhs.line == rhs.line && lhs.function == rhs.function && lhs.column == rhs.column;
}

inline bool operator<(const SourceLocation & lhs, const SourceLocation & rhs ) {
    if (lhs.line < rhs.line)
        return true;

    if (lhs.column < rhs.column)
        return true;

    if (lhs.function < rhs.function)
        return true;

    return false;
}

#endif // ENTITIES_H
