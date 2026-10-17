#pragma once
#include "catalog.hpp"
namespace psgi {
void readIgdb(Entry& entry, const std::string& json);
std::vector<std::string> displayCategories(const Entry& entry);
std::vector<std::string> displayTags(const Entry& entry);
std::string joinedNames(const std::vector<std::string>& names);
std::string displayDescription(const Entry& entry);
double displayRating(const Entry& entry);
std::string ratingText(const Entry& entry);
std::string readableUrl(const std::string& url);
}
