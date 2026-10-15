#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace psgi {
constexpr size_t MaxCatalogBytes = 8 * 1024 * 1024;
constexpr size_t MaxEntries = 50000;

struct Source { std::string category, url; };
struct Entry {
    std::string url, name, titleId, region, version, minFirmware, category;
    uint64_t size = 0;
};

bool isHttpUrl(const std::string& url);
std::vector<Source> parseConfig(const std::string& text);
std::vector<Entry> parseCatalog(const std::string& text, const std::string& category);
void mergeCatalog(std::vector<Entry>& into, const std::vector<Entry>& entries);
std::vector<size_t> filterCatalog(const std::vector<Entry>& entries,
                                const std::string& category, const std::string& query,
                                bool sortBySize = false);
std::string formatSize(uint64_t bytes);
std::string readFile(const std::string& path);
}
