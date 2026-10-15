#pragma once
#include <map>
#include <string>

namespace psgi {
std::map<std::string, std::string> parseSfo(const std::string& bytes);
bool validTitleId(const std::string& titleId);
}
