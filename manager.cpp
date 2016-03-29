#include "manager.h"

#include <yaml-cpp/yaml.h>

#include "exception.h"

Manager::Manager(const string &db, const string &source, const string &filter) : writer(db), filter(filter)
{
    PIN_MutexInit(&mutex);

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

        t.id = i + 1;

        if(!tag.IsMap())
            YAMLException(file, "Tag element should be a map");

        if(!tag["name"].IsScalar())
            YAMLException(file, "name should be a string");

        t.name = tag["name"].as<std::string>();


        if(!tag["type"].IsScalar())
            YAMLException(file, "type should be a string");

        std::string typeName = tag["type"].as<std::string>();

        if (typeName == "Simple") {
            t.type = TagType::Simple;
        } else if (typeName == "Counter") {
            t.type = TagType::Counter;
        } else {
            YAMLException(file, "invalid tag type");
        }

        this->tags.push_back(t);
    }

    YAML::Node tagInstructions = filter["tagInstructions"];

    if(!tagInstructions.IsSequence())
        YAMLException(file, "Source file should contain a sequence called 'tagInstructions'");

    for (std::size_t i=0; i < tagInstructions.size(); i++) {
        YAML::Node tagInstruction = tagInstructions[i];
        TagInstruction t;

        if(!tagInstruction["type"].IsScalar())
            YAMLException(file, "type should be a string");

        std::string typeName = tagInstruction["type"].as<std::string>();

        if (typeName == "Start") {
            t.type = TagInstructionType::Start;
        } else if (typeName == "Stop") {
            t.type = TagInstructionType::Stop;
        } else {
            YAMLException(file, "invalid tag instruction type");
        }

        if(!tagInstruction["location"].IsScalar())
            YAMLException(file, "location should be a integer");

        t.location = tagInstruction["location"].as<int>();

        if(!tagInstruction["tag"].IsScalar())
            YAMLException(file, "tag should be a integer");

        t.tag = tagInstruction["tag"].as<int>();

        this->tagInstructions.push_back(t);
    }

}

void Manager::writeTags()
{
    for (auto it : tags) {
        writer.insertTag(it);
    }

    for (auto it : tagInstructions) {
        writer.insertTagInstruction(it);
    }
}

void Manager::loadSourceLocationTagIdMap()
{
    for (auto it : tagInstructions)
        sourceLocationTagInstructionIdMap.insert(std::make_pair(writer.getSourceLocationById(it.location), it.id));
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

