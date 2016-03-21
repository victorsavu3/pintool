#ifndef MANAGER_H
#define MANAGER_H

#include <unordered_map>
#include <map>
#include <set>

#include <pin.H>

#include "sqlwriter.h"
#include "entities.h"
#include "filter.h"

class Manager
{
public:
    Manager(const std::string& db, const std::string& source, const std::string& filter);

    SQLWriter writer;
    Filter filter;

    std::vector<Tag> tags;
    std::map<SourceLocation, Tag> sourceLocationTagMap;
private:

};

#endif // MANAGER_H
