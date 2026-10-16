#pragma once
#include "settings.hpp"
#include <set>
#include <optional>

namespace psgi {
enum class Library { Installed, Available };
struct VisibleEntry { size_t index; std::string group; };
void matchInstalled(std::vector<Entry>& remote, const std::vector<Entry>& installed,
                    const std::set<std::string>& queuedUrls);
std::vector<VisibleEntry> makeView(const std::vector<Entry>& entries, const Settings& settings,
                                 Library library, const std::string& query = "", const LocalFilter& filter = {});
bool matchesLocal(const Entry& entry, const LocalFilter& filter);
std::vector<std::pair<std::string, std::string>> entryDetails(const Entry& entry);
// Unknown/malformed versions do not establish installation or update status.
std::optional<int> compareVersions(const std::string& a, const std::string& b);
unsigned packageKind(const Entry& entry); // base, update, DLC, other
std::string packageKindName(const Entry& entry);
std::string packageStatus(const Entry& entry);
std::vector<size_t> relatedPackages(const std::vector<Entry>& entries, const std::string& titleId);
}
