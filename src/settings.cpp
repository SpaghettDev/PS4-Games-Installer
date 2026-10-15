#include "settings.hpp"
#include "cJSON.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
std::string stringField(const cJSON* root, const char* key) {
    auto* item = cJSON_GetObjectItem(root, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}
bool boolean(const cJSON* root, const char* key, bool fallback) {
    auto* item = cJSON_GetObjectItem(root, key);
    if (!item) return fallback;
    if (!cJSON_IsBool(item)) throw std::runtime_error(std::string("Invalid setting: ") + key);
    return cJSON_IsTrue(item);
}
}
Settings parseSettings(const std::string& text) {
    if (text.size() > MaxCatalogBytes || text.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid settings data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) throw std::runtime_error("Invalid settings JSON");
    auto* schema = cJSON_GetObjectItem(root.get(), "schema");
    if (schema && (!cJSON_IsNumber(schema) || schema->valuedouble != 1)) throw std::runtime_error("Unsupported settings version");
    Settings s;
    s.sources.clear();
    const cJSON* prefs = cJSON_GetObjectItem(root.get(), "preferences");
    const cJSON* urls = cJSON_GetObjectItem(prefs ? prefs : root.get(), "content_urls");
    if (!cJSON_IsObject(urls)) throw std::runtime_error("Settings need content_urls");
    for (const cJSON* item = urls->child; item; item = item->next) {
        std::string category = lower(item->string);
        if (category == "ps5") continue;
        if (category.empty() || category.size() > 32 || category.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_- ") != std::string::npos)
            throw std::runtime_error("Invalid source category");
        std::string url;
        if (cJSON_IsString(item)) url = item->valuestring;
        else if (!cJSON_IsNull(item)) throw std::runtime_error("Invalid catalog URL");
        if (!url.empty() && !isHttpUrl(url)) throw std::runtime_error("Use a direct http:// or https:// catalog URL");
        for (const auto& source : s.sources) if (source.category == category) throw std::runtime_error("Duplicate source category");
        s.sources.push_back({category, url});
        if (s.sources.size() > 32) throw std::runtime_error("Maximum 32 catalog sources");
    }
    if (std::none_of(s.sources.begin(), s.sources.end(), [](const Source& source) { return source.category == "games"; }))
        s.sources.insert(s.sources.begin(), {"games", ""});
    else std::stable_partition(s.sources.begin(), s.sources.end(), [](const Source& source) { return source.category == "games"; });
    auto layout = stringField(root.get(), "layout");
    if (!layout.empty() && layout != "tiles" && layout != "summary") throw std::runtime_error("Invalid layout");
    s.layout = layout == "summary" ? Layout::Summary : Layout::Tiles;
    auto grouping = stringField(root.get(), "grouping");
    if (grouping == "category") s.grouping = Grouping::Category;
    else if (grouping == "region") s.grouping = Grouping::Region;
    else if (grouping == "letter") s.grouping = Grouping::Letter;
    else if (!grouping.empty() && grouping != "none") throw std::runtime_error("Invalid grouping");
    s.sizeSort = boolean(root.get(), "size_sort", false);
    s.notInstalledOnly = boolean(root.get(), "not_installed_only", false);
    s.animations = boolean(root.get(), "animations", true);
    s.refreshOnStart = boolean(root.get(), "refresh_on_start", true);
    return s;
}

std::string serializeSettings(const Settings& s) {
    Json root(cJSON_CreateObject(), cJSON_Delete);
    cJSON_AddNumberToObject(root.get(), "schema", 1);
    auto* urls = cJSON_AddObjectToObject(root.get(), "content_urls");
    for (const auto& source : s.sources) cJSON_AddStringToObject(urls, source.category.c_str(), source.url.c_str());
    cJSON_AddStringToObject(root.get(), "layout", s.layout == Layout::Tiles ? "tiles" : "summary");
    const char* groups[] = {"none", "category", "region", "letter"};
    cJSON_AddStringToObject(root.get(), "grouping", groups[static_cast<int>(s.grouping)]);
    cJSON_AddBoolToObject(root.get(), "size_sort", s.sizeSort);
    cJSON_AddBoolToObject(root.get(), "not_installed_only", s.notInstalledOnly);
    cJSON_AddBoolToObject(root.get(), "animations", s.animations);
    cJSON_AddBoolToObject(root.get(), "refresh_on_start", s.refreshOnStart);
    std::unique_ptr<char, decltype(&cJSON_free)> out(cJSON_Print(root.get()), cJSON_free);
    if (!out) throw std::runtime_error("Settings allocation failed");
    return out.get();
}
bool sameSettings(const Settings& a, const Settings& b) { return serializeSettings(a) == serializeSettings(b); }

void atomicWrite(const std::string& path, const std::string& text) {
    std::string temporary = path + ".tmp";
    int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) throw std::runtime_error("Cannot write " + path);
    size_t offset = 0;
    while (offset < text.size()) {
        ssize_t n = write(fd, text.data() + offset, text.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); unlink(temporary.c_str()); throw std::runtime_error("Settings/cache write failed"); }
        offset += n;
    }
    int synced = fsync(fd), closed = close(fd);
    if (synced || closed || rename(temporary.c_str(), path.c_str())) {
        unlink(temporary.c_str()); throw std::runtime_error("Cannot save " + path);
    }
}
Settings loadSettings(const std::string& directory, const std::string& legacyPath) {
    if (mkdir(directory.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create application data directory");
    const std::string config = directory + "/config.json";
    if (access(config.c_str(), F_OK) == 0) return parseSettings(readFile(config));
    Settings settings;
    if (!legacyPath.empty() && access(legacyPath.c_str(), F_OK) == 0) settings = parseSettings(readFile(legacyPath));
    saveSettings(directory, settings);
    return settings;
}
void saveSettings(const std::string& directory, const Settings& s) {
    std::string text = serializeSettings(s);
    parseSettings(text); // Validate drafts before replacing the working file.
    atomicWrite(directory + "/config.json", text);
}
}
