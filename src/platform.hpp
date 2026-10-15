#pragma once
#include "catalog.hpp"

namespace psgi {
std::string httpGet(const std::string& url);
void prepareConsole();
int queueInstall(const Entry& entry, int userId);
}
