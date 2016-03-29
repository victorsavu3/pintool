#include "manager.h"

#include <yaml-cpp/yaml.h>

#include "exception.h"

Manager::Manager(const string &db, const string &source, const string &filter) : writer(db), filter(filter)
{
    loadTags(source);
    writeTags();
    loadSourceLocationTagIdMap();
    loadTagIdTagMap();
}

void Manager::bufferFull(BufferEntry* entries, UINT64 count, THREADID tid)
{
    lock();
    ThreadManager& manager = threadmanagers[tid];
    unlock();

    manager.bufferFull(entries, count);
}

void Manager::setUpThreadManager(THREADID tid)
{
    lock();
    threadmanagers.insert(std::make_pair(tid, ThreadManager(this, tid)));
    unlock();
}

void Manager::tearDownThreadManager(THREADID tid)
{
    lock();
    threadmanagers.erase(tid);
    unlock();
}

void Manager::loadTags(const string &file)
{
    YAML::Node filter = YAML::LoadFile(file);

    if(!filter.IsMap())
        YAMLException(file, "Source file should be a map");

    YAML::Node tags = filter["tags"];

    if(!tags.IsSequence())
        YAMLException(file, "Source file should contain a sequence called 'tags'");

    for (std::size_t i=0; i < tags.size(); i++) {
        YAML::Node tag = tags[i];
        Tag t;

        if(!tag.IsMap())
            YAMLException(file, "Tag element should be a map");

        if (tag["name"]) {
            if(!tag["name"].IsScalar())
                YAMLException(file, "name should be a string");

            t.name = tag["name"].as<std::string>();
        }

        if(!tag["location"].IsScalar())
            YAMLException(file, "type should be a number");

        t.location = tag["location"].as<int>();


        if(!tag["type"].IsScalar())
            YAMLException(file, "type should be a string");

        std::string typeName = tag["type"].as<std::string>();

        if (typeName == "EndTag") {
            t.type = TagType::EndTag;
        } else if (typeName == "StartSimpleTag") {
            t.type = TagType::StartSimpleTag;
        }  else if (typeName == "StartCounterTag") {
            t.type = TagType::StartCounterTag;
        }

        this->tags.push_back(t);
    }
}

void Manager::writeTags()
{
    for (auto it : tags) {
        writer.insertTag(it);

        if (it.type == TagType::StartSimpleTag) {
            int id = writer.insertSimpleTagInstance(it.id);

            simpleTagInstanceMap.insert(std::make_pair(it.id, id));
        }
    }
}

void Manager::loadSourceLocationTagIdMap()
{
    for (auto it : tags)
        sourceLocationTagIdMap.insert(std::make_pair(writer.getSourceLocationById(it.location), it.id));
}

void Manager::loadTagIdTagMap()
{
    for (auto it : tags)
        tagIdTagMap.insert(std::make_pair(it.id, it));
}

void Manager::lock()
{
    PIN_MutexLock(&mutex);
}

void Manager::unlock()
{
    PIN_MutexUnlock(&mutex);
}

