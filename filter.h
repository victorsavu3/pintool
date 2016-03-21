#ifndef FILTER_H
#define FILTER_H

#include <regex>
#include <vector>
#include <map>
#include <string>

struct FilterData {
    std::vector<std::regex> include;
    std::vector<std::regex> exclude;
};

class Filter
{
public:
    Filter(std::string file);

    bool isImageFiltered(const std::string& image);
    bool isFileFiltered(const std::string& file);
    bool isFunctionFiltered(const std::string& function);

    bool isFiltered(const std::string& type, const std::string& content);

private:
    std::map<std::string, FilterData> filters;
};

#endif // FILTER_H
