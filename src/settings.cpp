#include "settings.hpp"
#include "json.hpp"
#include "text.hpp"
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
std::string profileName(const std::string& value) {
    auto name=value; const auto start=name.find_first_not_of(" \t\r\n"), end=name.find_last_not_of(" \t\r\n");
    name=start==std::string::npos ? "" : name.substr(start,end-start+1);
    if (name.empty() || name.size()>64) throw std::runtime_error("Use a profile name of 1-64 bytes");
    toUtf16(name);
    for (unsigned char c:name) if (c<32 || c==127) throw std::runtime_error("Profile names cannot contain control characters");
    return name;
}
std::vector<Source> readSources(const cJSON* urls) {
    std::vector<Source> sources;
    if (!cJSON_IsObject(urls)) throw std::runtime_error("Settings need content_urls");
    for (const cJSON* item = urls->child; item; item = item->next) {
        std::string category = lower(item->string);
        if (category == "ps5") continue;
        if (category.empty() || category.size() > 32 || category.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_- ") != std::string::npos)
            throw std::runtime_error("Invalid source category");
        std::string url;
        std::vector<std::pair<std::string, std::string>> headers;
        if (cJSON_IsString(item)) {
            url = item->valuestring;
        } else if (cJSON_IsObject(item)) {
            const auto* urlItem = cJSON_GetObjectItem(item, "url");
            if (cJSON_IsString(urlItem)) url = urlItem->valuestring;
            const auto* headersItem = cJSON_GetObjectItem(item, "headers");
            if (cJSON_IsObject(headersItem)) {
                for (const auto* h = headersItem->child; h; h = h->next) {
                    if (cJSON_IsString(h)) headers.push_back({h->string, h->valuestring});
                }
            }
        } else if (!cJSON_IsNull(item)) throw std::runtime_error("Invalid catalog URL");
        if (!url.empty() && !isHttpUrl(url)) throw std::runtime_error("Use a direct http:// or https:// catalog URL");
        for (const auto& source : sources) if (source.category == category) throw std::runtime_error("Duplicate source category");
        sources.push_back({category, url, headers});
        if (sources.size() > 32) throw std::runtime_error("Maximum 32 catalog sources");
    }
    if (std::none_of(sources.begin(), sources.end(), [](const Source& source) { return source.category == "games"; }))
        sources.insert(sources.begin(), {"games", "", {}});
    else std::stable_partition(sources.begin(), sources.end(), [](const Source& source) { return source.category == "games"; });
    if (sources.size()>32) throw std::runtime_error("Maximum 32 catalog sources including games");
    return sources;
}
void writeSources(cJSON* object, const std::vector<Source>& sources) {
    auto* urls=cJSON_AddObjectToObject(object,"content_urls");
    for (const auto& source:sources) {
        if (source.headers.empty()) {
            cJSON_AddStringToObject(urls,source.category.c_str(),source.url.c_str());
        } else {
            auto* sourceObj = cJSON_AddObjectToObject(urls, source.category.c_str());
            cJSON_AddStringToObject(sourceObj, "url", source.url.c_str());
            auto* headersObj = cJSON_AddObjectToObject(sourceObj, "headers");
            for (const auto& h : source.headers) cJSON_AddStringToObject(headersObj, h.first.c_str(), h.second.c_str());
        }
    }
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
    const cJSON* prefs = cJSON_GetObjectItem(root.get(), "preferences");
    s.sources = readSources(cJSON_GetObjectItem(prefs ? prefs : root.get(), "content_urls"));
    const auto* profiles = cJSON_GetObjectItemCaseSensitive(root.get(), "server_profiles");
    if (profiles && !cJSON_IsArray(profiles)) throw std::runtime_error("Invalid server profiles");
    if (profiles) for (const auto* item = profiles->child; item; item = item->next) {
        if (!cJSON_IsObject(item) || s.profiles.size() >= MaxServerProfiles) throw std::runtime_error("Maximum 16 server profiles");
        const auto name = profileName(stringField(item, "name"));
        if (std::any_of(s.profiles.begin(),s.profiles.end(),[&](const ServerProfile& p){return lower(p.name)==lower(name);})) throw std::runtime_error("Duplicate server profile name");
        s.profiles.push_back({name,readSources(cJSON_GetObjectItemCaseSensitive(item,"content_urls"))});
    }
    const auto* active = cJSON_GetObjectItemCaseSensitive(root.get(), "active_profile");
    if (active && !cJSON_IsString(active)) throw std::runtime_error("Invalid active server profile");
    s.activeProfile = stringField(root.get(), "active_profile");
    syncActiveProfile(s);
    auto layout = stringField(root.get(), "layout");
    if (!layout.empty() && layout != "tiles" && layout != "summary") throw std::runtime_error("Invalid layout");
    s.layout = layout == "summary" ? Layout::Summary : Layout::Tiles;
    auto grouping = stringField(root.get(), "grouping");
    if (grouping == "category") s.grouping = Grouping::Category;
    else if (grouping == "region") s.grouping = Grouping::Region;
    else if (grouping == "letter") s.grouping = Grouping::Letter;
    else if (grouping == "local_category") s.grouping = Grouping::LocalCategory;
    else if (grouping == "tag") s.grouping = Grouping::Tag;
    else if (grouping == "title") s.grouping = Grouping::Title;
    else if (!grouping.empty() && grouping != "none") throw std::runtime_error("Invalid grouping");
    const auto sort = stringField(root.get(), "sort_field");
    if (sort.empty()) { s.sort = boolean(root.get(), "size_sort", false) ? SortField::Size : SortField::Name; s.descending = s.sort == SortField::Size; }
    else {
        const char* names[] = {"name", "size", "rating", "last_launch", "first_installed"};
        size_t i = 0; for (; i < 5 && sort != names[i]; ++i) {}
        if (i == 5) throw std::runtime_error("Invalid sort field");
        s.sort = static_cast<SortField>(i); s.descending = boolean(root.get(), "sort_descending", i != 0);
    }
    s.notInstalledOnly = boolean(root.get(), "not_installed_only", false);
    s.animations = boolean(root.get(), "animations", true);
    s.refreshOnStart = boolean(root.get(), "refresh_on_start", true);
    s.skipBrokenDataObjects = boolean(root.get(), "skip_broken_data_objs", true);
    return s;
}

std::string serializeSettings(const Settings& input) {
    auto s=input; syncActiveProfile(s);
    Json root(cJSON_CreateObject(), cJSON_Delete);
    cJSON_AddNumberToObject(root.get(), "schema", 1);
    writeSources(root.get(),s.sources);
    cJSON_AddStringToObject(root.get(),"active_profile",s.activeProfile.c_str());
    auto* profiles=cJSON_AddArrayToObject(root.get(),"server_profiles");
    for (const auto& profile:s.profiles) {
        auto* item=cJSON_CreateObject(); cJSON_AddItemToArray(profiles,item);
        cJSON_AddStringToObject(item,"name",profile.name.c_str()); writeSources(item,profile.sources);
    }
    cJSON_AddStringToObject(root.get(), "layout", s.layout == Layout::Tiles ? "tiles" : "summary");
    const char* groups[] = {"none", "category", "region", "letter", "local_category", "tag", "title"};
    cJSON_AddStringToObject(root.get(), "grouping", groups[static_cast<int>(s.grouping)]);
    const char* sorts[] = {"name", "size", "rating", "last_launch", "first_installed"};
    const auto sort = static_cast<unsigned>(s.sort);
    if (sort >= 5) throw std::runtime_error("Invalid sort field");
    cJSON_AddStringToObject(root.get(), "sort_field", sorts[sort]);
    cJSON_AddBoolToObject(root.get(), "sort_descending", s.descending);
    cJSON_AddBoolToObject(root.get(), "size_sort", s.sort == SortField::Size && s.descending);
    cJSON_AddBoolToObject(root.get(), "not_installed_only", s.notInstalledOnly);
    cJSON_AddBoolToObject(root.get(), "animations", s.animations);
    cJSON_AddBoolToObject(root.get(), "refresh_on_start", s.refreshOnStart);
    cJSON_AddBoolToObject(root.get(), "skip_broken_data_objs", s.skipBrokenDataObjects);
    std::unique_ptr<char, decltype(&cJSON_free)> out(printJson(root.get(),true), cJSON_free);
    if (!out) throw std::runtime_error("Settings allocation failed");
    return out.get();
}
void syncActiveProfile(Settings& s) {
    if (s.activeProfile.empty()) return;
    for (auto& profile:s.profiles) if (lower(profile.name)==lower(s.activeProfile)) {
        s.activeProfile=profile.name; profile.sources=s.sources; return;
    }
    throw std::runtime_error("Active server profile does not exist");
}
void addServerProfile(Settings& s, const std::string& value) {
    const auto name=profileName(value);
    if (s.profiles.size()>=MaxServerProfiles) throw std::runtime_error("Maximum 16 server profiles");
    if (std::any_of(s.profiles.begin(),s.profiles.end(),[&](const ServerProfile& p){return lower(p.name)==lower(name);})) throw std::runtime_error("That server profile name already exists");
    auto next=s; next.profiles.push_back({name,next.sources}); next.activeProfile=name;
    s=parseSettings(serializeSettings(next));
}
void useServerProfile(Settings& s, size_t index) {
    if (index>=s.profiles.size()) throw std::runtime_error("Server profile no longer exists");
    auto next=s; syncActiveProfile(next); next.sources=next.profiles[index].sources; next.activeProfile=next.profiles[index].name;
    s=parseSettings(serializeSettings(next));
}
void renameServerProfile(Settings& s, size_t index, const std::string& value) {
    if (index>=s.profiles.size()) throw std::runtime_error("Server profile no longer exists");
    const auto name=profileName(value);
    for (size_t i=0;i<s.profiles.size();++i) if (i!=index && lower(s.profiles[i].name)==lower(name)) throw std::runtime_error("That server profile name already exists");
    auto next=s;
    if (lower(next.activeProfile)==lower(next.profiles[index].name)) next.activeProfile=name;
    next.profiles[index].name=name; s=parseSettings(serializeSettings(next));
}
void deleteServerProfile(Settings& s, size_t index) {
    if (index>=s.profiles.size()) throw std::runtime_error("Server profile no longer exists");
    auto next=s;
    if (lower(next.activeProfile)==lower(next.profiles[index].name)) next.activeProfile.clear();
    next.profiles.erase(next.profiles.begin()+index); s=parseSettings(serializeSettings(next));
}
std::string catalogCachePath(const std::string& directory, const Settings& s) {
    uint64_t hash=14695981039346656037ULL;
    for (const auto& source:s.sources) for (const auto& part:{source.category,source.url}) {
        for (unsigned char c:part) { hash^=c; hash*=1099511628211ULL; }
        hash^=0; hash*=1099511628211ULL;
    }
    char name[40]; std::snprintf(name,sizeof(name),"/catalog-%016llx.json",static_cast<unsigned long long>(hash)); return directory+name;
}
bool sameSettings(const Settings& a, const Settings& b) { return serializeSettings(a) == serializeSettings(b); }
std::string sortName(SortField field) {
    const char* names[] = {"Name", "Size", "Rating", "Last launched here", "First recorded install"};
    const auto index = static_cast<unsigned>(field); return index < 5 ? names[index] : "Unknown";
}

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
std::string applicationDataDirectory(const std::string& parent) {
    const std::string directory = parent + "/PS4-Game-Installer";
    struct stat st{};
    if (stat(directory.c_str(), &st) == 0) {
        if (!S_ISDIR(st.st_mode)) throw std::runtime_error("Application data path is not a directory: " + directory);
        return directory;
    }
    if (errno != ENOENT) throw std::runtime_error("Cannot inspect application data directory: " + directory);
    // Preserve data from this application's earlier releases when adopting the
    // canonical directory. An existing destination always takes precedence.
    const std::string previous = parent + "/PS4-Games-Installer";
    if (stat(previous.c_str(), &st) == 0) {
        if (!S_ISDIR(st.st_mode) || rename(previous.c_str(), directory.c_str()))
            throw std::runtime_error("Cannot move application data to " + directory);
    } else {
        if (errno != ENOENT || mkdir(directory.c_str(), 0700))
            throw std::runtime_error("Cannot create application data directory: " + directory);
    }
    return directory;
}
Settings loadSettings(const std::string& directory) {
    if (mkdir(directory.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create application data directory");
    const std::string config = directory + "/config.json";
    if (access(config.c_str(), F_OK) == 0) return parseSettings(readFile(config));
    Settings settings;
    saveSettings(directory, settings);
    return settings;
}
void saveSettings(const std::string& directory, const Settings& s) {
    std::string text = serializeSettings(s);
    parseSettings(text); // Validate drafts before replacing the working file.
    atomicWrite(directory + "/config.json", text);
    // A successful UI acknowledgement must describe the file that was written.
    if (!sameSettings(parseSettings(readFile(directory + "/config.json")), s))
        throw std::runtime_error("Saved config.json did not match the requested settings");
}
}
