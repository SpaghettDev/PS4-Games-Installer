#include "userdata.hpp"
#include "settings.hpp"
#include "sfo.hpp"
#include "cJSON.h"
#include <cmath>
#include <algorithm>
#include <cerrno>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string& text, unsigned maxSchema = 1) {
    if (text.size() > MaxCatalogBytes || text.find('\0') != std::string::npos) throw std::runtime_error("Invalid local data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) throw std::runtime_error("Invalid local data JSON");
    auto* schema = cJSON_GetObjectItemCaseSensitive(root.get(), "schema");
    if (!cJSON_IsNumber(schema) || (schema->valuedouble != 1 && !(maxSchema >= 2 && schema->valuedouble == 2))) throw std::runtime_error("Unsupported local data schema");
    return root;
}
uint64_t number(const cJSON* object, const char* key, uint64_t limit = 9007199254740991ULL) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return 0;
    if (!cJSON_IsNumber(field) || !std::isfinite(field->valuedouble) || field->valuedouble < 0 ||
        field->valuedouble > double(limit) || std::floor(field->valuedouble) != field->valuedouble)
        throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return uint64_t(field->valuedouble);
}
bool boolean(const cJSON* object, const char* key) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return false;
    if (!cJSON_IsBool(field)) throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return cJSON_IsTrue(field);
}
std::string string(const cJSON* object, const char* key, size_t limit) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return "";
    if (!cJSON_IsString(field) || std::string(field->valuestring).size() > limit)
        throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return field->valuestring;
}
std::string print(const cJSON* root) {
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(root), cJSON_free);
    if (!text) throw std::runtime_error("Cannot serialize local data");
    return text.get();
}
void requireId(const std::string& id) { if (!validTitleId(id)) throw std::runtime_error("This game needs a valid Title ID for local records"); }
bool exists(const std::string& path) {
    if (!access(path.c_str(), F_OK)) return true;
    if (errno == ENOENT) return false;
    throw std::runtime_error("Cannot inspect local data file: " + path);
}
std::string canonical(std::vector<std::string>& terms, const std::string& name) {
    for (const auto& term : terms) if (lower(term) == lower(name)) return term;
    if (terms.size() >= MaxLocalTerms) throw std::runtime_error("Maximum 256 local categories or tags");
    terms.push_back(name);
    std::sort(terms.begin(), terms.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    return name;
}
std::vector<std::string> terms(const cJSON* object, const char* key, size_t maximum) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return {};
    if (!cJSON_IsArray(field)) throw std::runtime_error(std::string("Invalid local list: ") + key);
    std::vector<std::string> out;
    for (auto* item = field->child; item; item = item->next) {
        if (!cJSON_IsString(item) || out.size() >= maximum) throw std::runtime_error("Invalid local category/tag list");
        auto name = localName(item->valuestring);
        if (name.empty()) throw std::runtime_error("Empty local category/tag name");
        for (const auto& existing : out) if (lower(existing) == lower(name)) throw std::runtime_error("Duplicate local category/tag name");
        out.push_back(name);
    }
    return out;
}
void array(cJSON* object, const char* key, const std::vector<std::string>& values) {
    auto* field = cJSON_AddArrayToObject(object, key);
    for (const auto& value : values) cJSON_AddItemToArray(field, cJSON_CreateString(value.c_str()));
}
}
std::string localName(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
    const auto name = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    if (name.size() > MaxLocalName) throw std::runtime_error("Local names and labels must fit in 64 UTF-8 bytes");
    for (unsigned char c : name) if (c < 32 || c == 127) throw std::runtime_error("Local names cannot contain control characters");
    return name;
}
LocalInfo normalizeInfo(LocalInfo info) {
    info.category = localName(info.category); info.label = localName(info.label);
    if (info.description.size() > MaxLocalDescription) throw std::runtime_error("Local description exceeds 2047 UTF-8 bytes");
    for (unsigned char c : info.description) if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127)
        throw std::runtime_error("Invalid character in local description");
    std::vector<std::string> names;
    for (const auto& tag : info.tags) {
        auto name = localName(tag);
        if (name.empty()) throw std::runtime_error("Tag name cannot be empty");
        canonical(names, name);
    }
    if (names.size() > MaxGameTags) throw std::runtime_error("Maximum 16 tags per game");
    info.tags = std::move(names); return info;
}
bool sameInfo(const LocalInfo& a, const LocalInfo& b) {
    return a.category == b.category && a.label == b.label && a.description == b.description && a.tags == b.tags;
}
std::string joinedTags(const std::vector<std::string>& tags) {
    std::string text;
    for (const auto& tag : tags) { if (!text.empty()) text += ", "; text += tag; }
    return text;
}
bool PlayerData::observeInstalled(const std::vector<Entry>& entries, uint64_t now) {
    bool changed = false;
    for (const auto& entry : entries) if (validTitleId(entry.titleId)) {
        auto& record = games[entry.titleId];
        if (!record.wasInstalled) { record.wasInstalled = true; record.firstSeenInstalled = now; changed = true; }
    }
    return changed;
}
void PlayerData::rate(const std::string& id, unsigned stars) {
    requireId(id);
    if (stars > 5) throw std::runtime_error("Choose a rating from 1 to 5 stars");
    games[id].rating = stars;
}
void PlayerData::noteLaunch(const std::string& id, uint64_t now) {
    requireId(id); auto& record = games[id];
    if (record.launches >= 9007199254740991ULL) throw std::runtime_error("Launch counter is full");
    ++record.launches; record.lastLaunch = now;
}
void PlayerData::setInfo(const std::string& id, LocalInfo info) {
    requireId(id); info = normalizeInfo(std::move(info));
    auto nextCategories = categories, nextTags = tags;
    if (!info.category.empty()) info.category = canonical(nextCategories, info.category);
    for (auto& tag : info.tags) tag = canonical(nextTags, tag);
    games[id].info = std::move(info); categories = std::move(nextCategories); tags = std::move(nextTags);
}
void PlayerData::createTerm(bool tag, const std::string& name) {
    const auto value = localName(name);
    if (value.empty()) throw std::runtime_error("Enter a category or tag name");
    canonical(tag ? tags : categories, value);
}
void PlayerData::renameTerm(bool tag, const std::string& previous, const std::string& name) {
    const auto value = localName(name);
    if (value.empty()) throw std::runtime_error("Enter a category or tag name");
    auto& list = tag ? tags : categories;
    auto found = std::find_if(list.begin(), list.end(), [&](const std::string& term) { return lower(term) == lower(previous); });
    if (found == list.end()) throw std::runtime_error("That local category or tag no longer exists");
    for (const auto& term : list) if (lower(term) == lower(value) && lower(term) != lower(previous)) throw std::runtime_error("That local name already exists");
    for (auto& game : games) {
        auto& info = game.second.info;
        if (tag) { for (auto& term : info.tags) if (lower(term) == lower(previous)) term = value; }
        else if (lower(info.category) == lower(previous)) info.category = value;
    }
    *found = value;
    std::sort(list.begin(), list.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
}
void PlayerData::deleteTerm(bool tag, const std::string& name) {
    auto& list = tag ? tags : categories;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& term) { return lower(term) == lower(name); }), list.end());
    for (auto& game : games) {
        auto& info = game.second.info;
        if (tag) info.tags.erase(std::remove_if(info.tags.begin(), info.tags.end(), [&](const std::string& term) { return lower(term) == lower(name); }), info.tags.end());
        else if (lower(info.category) == lower(name)) info.category.clear();
    }
}
void PlayerData::apply(Entry& entry) const {
    auto found = games.find(entry.titleId);
    const GameRecord empty{};
    const auto& record = found == games.end() ? empty : found->second;
    entry.wasInstalled = record.wasInstalled; entry.rating = record.rating;
    entry.launches = record.launches; entry.firstSeenInstalled = record.firstSeenInstalled; entry.lastLaunch = record.lastLaunch;
    entry.info = record.info;
}
void PlayerData::apply(std::vector<Entry>& entries) const { for (auto& entry : entries) apply(entry); }
PlayerData parsePlayerData(const std::string& text) {
    auto root = parse(text, 2); auto* games = cJSON_GetObjectItemCaseSensitive(root.get(), "games");
    if (!cJSON_IsObject(games)) throw std::runtime_error("Local game records are missing");
    PlayerData data;
    data.categories = terms(root.get(), "categories", MaxLocalTerms); data.tags = terms(root.get(), "tags", MaxLocalTerms);
    for (auto* item = games->child; item; item = item->next) {
        const std::string id = item->string; requireId(id);
        if (!cJSON_IsObject(item) || data.games.count(id) || data.games.size() >= MaxEntries) throw std::runtime_error("Invalid or duplicate local game record");
        data.games[id] = {boolean(item, "was_installed"), unsigned(number(item, "rating", 5)),
            number(item, "first_seen_installed"), number(item, "launches"), number(item, "last_launch")};
        LocalInfo info{string(item, "category", MaxLocalName), string(item, "label", MaxLocalName),
            string(item, "description", MaxLocalDescription), terms(item, "tags", MaxGameTags)};
        data.setInfo(id, std::move(info));
    }
    return data;
}
std::string serializePlayerData(const PlayerData& data) {
    // Older releases reject schema 2 and preserve the file instead of silently
    // dropping the new personal fields when saving ratings or history.
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 2);
    auto* games = cJSON_AddObjectToObject(root.get(), "games");
    array(root.get(), "categories", data.categories); array(root.get(), "tags", data.tags);
    for (const auto& game : data.games) {
        auto* record = cJSON_AddObjectToObject(games, game.first.c_str());
        cJSON_AddBoolToObject(record, "was_installed", game.second.wasInstalled);
        cJSON_AddNumberToObject(record, "rating", game.second.rating);
        cJSON_AddNumberToObject(record, "first_seen_installed", game.second.firstSeenInstalled);
        cJSON_AddNumberToObject(record, "launches", game.second.launches);
        cJSON_AddNumberToObject(record, "last_launch", game.second.lastLaunch);
        cJSON_AddStringToObject(record, "category", game.second.info.category.c_str());
        cJSON_AddStringToObject(record, "label", game.second.info.label.c_str());
        cJSON_AddStringToObject(record, "description", game.second.info.description.c_str());
        array(record, "tags", game.second.info.tags);
    }
    return print(root.get());
}
PlayerData loadPlayerData(const std::string& directory) {
    const auto path = directory + "/player.json";
    return exists(path) ? parsePlayerData(readFile(path)) : PlayerData{};
}
void savePlayerData(const std::string& directory, const PlayerData& data) {
    const auto text = serializePlayerData(data); parsePlayerData(text); atomicWrite(directory + "/player.json", text);
}
ViewState parseViewState(const std::string& text) {
    auto root = parse(text); ViewState state;
    state.available = boolean(root.get(), "available"); state.sidebar = boolean(root.get(), "sidebar");
    state.query = string(root.get(), "query", 512);
    if (auto* filter = cJSON_GetObjectItemCaseSensitive(root.get(), "local_filter")) {
        if (!cJSON_IsObject(filter)) throw std::runtime_error("Invalid local view filter");
        state.filter.category = localName(string(filter, "category", MaxLocalName));
        state.filter.tag = localName(string(filter, "tag", MaxLocalName));
        state.filter.uncategorized = boolean(filter, "uncategorized"); state.filter.untagged = boolean(filter, "untagged");
    }
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_GetObjectItemCaseSensitive(root.get(), i ? "remote" : "installed");
        if (!cJSON_IsObject(item)) throw std::runtime_error("Saved library position is missing");
        auto& cursor = state.cursor[i]; cursor.titleId = string(item, "title_id", 16); cursor.url = string(item, "url", 2047);
        cursor.index = number(item, "index", MaxEntries * MaxGameTags);
        cursor.group = string(item, "group", MaxLocalName);
        if ((!cursor.titleId.empty() && !validTitleId(cursor.titleId)) || (!cursor.url.empty() && !isHttpUrl(cursor.url)))
            throw std::runtime_error("Invalid saved library position");
    }
    return state;
}
std::string serializeViewState(const ViewState& state) {
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 1);
    cJSON_AddBoolToObject(root.get(), "available", state.available); cJSON_AddBoolToObject(root.get(), "sidebar", state.sidebar);
    cJSON_AddStringToObject(root.get(), "query", state.query.c_str());
    auto* filter = cJSON_AddObjectToObject(root.get(), "local_filter");
    cJSON_AddStringToObject(filter, "category", state.filter.category.c_str()); cJSON_AddStringToObject(filter, "tag", state.filter.tag.c_str());
    cJSON_AddBoolToObject(filter, "uncategorized", state.filter.uncategorized); cJSON_AddBoolToObject(filter, "untagged", state.filter.untagged);
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_AddObjectToObject(root.get(), i ? "remote" : "installed");
        const auto& cursor = state.cursor[i];
        cJSON_AddStringToObject(item, "title_id", cursor.titleId.c_str()); cJSON_AddStringToObject(item, "url", cursor.url.c_str());
        cJSON_AddNumberToObject(item, "index", cursor.index);
        cJSON_AddStringToObject(item, "group", cursor.group.c_str());
    }
    return print(root.get());
}
ViewState loadViewState(const std::string& directory) {
    const auto path = directory + "/session.json";
    return exists(path) ? parseViewState(readFile(path)) : ViewState{};
}
void saveViewState(const std::string& directory, const ViewState& state) {
    const auto text = serializeViewState(state); parseViewState(text); atomicWrite(directory + "/session.json", text);
}
}
