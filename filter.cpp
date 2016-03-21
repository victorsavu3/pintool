#include "filter.h"

#include <exception>
#include <iostream>

#include <yaml-cpp/yaml.h>

Filter::Filter(std::string file)
{
    YAML::Node filter = YAML::LoadFile(file);

    if(!filter.IsMap())
        throw std::invalid_argument("Filter file should be a map");

    for (auto it : filter) {
        if(!it.second.IsMap())
            throw std::invalid_argument("Filter file should be a map of maps");

        std::string name = it.first.as<std::string>();

        FilterData filterdata;



        if (it.second["include"]) {
            if(!it.second["include"].IsSequence())
                throw std::invalid_argument("'include' should be a sequence");

            for (auto it2 : it.second["include"]) {
                std::cout << it2.as<std::string>() << std::endl;
                filterdata.include.push_back(std::regex(it2.as<std::string>(), std::regex_constants::basic));
            }
        } else {

        }


        if (it.second["exclude"]) {
            if(!it.second["exclude"].IsSequence())
                throw std::invalid_argument("'exclude' should be a sequence");


            for (auto it2 : it.second["exclude"]) {
                filterdata.exclude.push_back(std::regex(it2.as<std::string>()));
            }

        }

        filters.insert(std::make_pair(name, filterdata));
    }
}

bool Filter::isImageFiltered(const std::string& image)
{
    return isFiltered("image", image);
}

bool Filter::isFileFiltered(const std::string& file)
{
    return isFiltered("file", file);
}

bool Filter::isFunctionFiltered(const std::string& function)
{
    return isFiltered("function", function);
}

bool Filter::isFiltered(const std::string& type, const std::string& content)
{
    FilterData& filterdata = filters[type];

    bool included = false;

    for (auto it : filterdata.include) {
        if (std::regex_search(content, it)) {
            included = true;
            break;
        }
    }

    if (!included && filterdata.include.size() > 0)
        return true;

    for (auto it : filterdata.exclude) {
        if (std::regex_search(content, it)) {
            return true;
        }
    }

    return false;
}
