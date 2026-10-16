#pragma once
#include "settings.hpp"
#include <set>

namespace psgi {
enum class Library { Installed, Available };
struct VisibleEntry { size_t index; std::string group; };
void matchInstalled(std::vector<Entry>& remote, const std::vector<Entry>& installed,
                    const std::set<std::string>& queuedUrls);
std::vector<VisibleEntry> makeView(const std::vector<Entry>& entries, const Settings& settings,
                                 Library library, const std::string& query = "", const LocalFilter& filter = {});
bool matchesLocal(const Entry& entry, const LocalFilter& filter);
std::vector<std::pair<std::string, std::string>> entryDetails(const Entry& entry);
}
