#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace psgi {
constexpr size_t MaxCatalogBytes = 8 * 1024 * 1024;
constexpr size_t MaxEntries = 50000;

struct Source { std::string category, url; };
struct Entry {
    std::string url, name, titleId, region, version, minFirmware, category;
    std::string coverUrl, localIcon, release, contentId, description;
    uint64_t size = 0, installedSize = 0;
    bool installed = false, queued = false;
    // Local records are applied by Title ID after catalog parsing/refresh.
    bool wasInstalled = false;
    unsigned rating = 0;
    uint64_t firstSeenInstalled = 0, launches = 0, lastLaunch = 0;
    std::vector<std::pair<std::string, std::string>> metadata;
};

bool isHttpUrl(const std::string& url);
std::vector<Source> parseConfig(const std::string& text);
std::vector<Entry> parseCatalog(const std::string& text, const std::string& category, size_t limit = MaxCatalogBytes);
void mergeCatalog(std::vector<Entry>& into, const std::vector<Entry>& entries);
std::vector<size_t> filterCatalog(const std::vector<Entry>& entries,
                                const std::string& category, const std::string& query,
                                bool sortBySize = false);
std::string formatSize(uint64_t bytes);
std::string readFile(const std::string& path, size_t limit = MaxCatalogBytes);
std::string serializeCatalog(const std::vector<Entry>& entries);
std::string lower(std::string value);
}
