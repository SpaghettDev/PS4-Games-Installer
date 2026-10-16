#include "userdata.hpp"
#include "settings.hpp"
#include "sfo.hpp"
#include "cJSON.h"
#include <cmath>
#include <cerrno>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string& text) {
    if (text.size() > MaxCatalogBytes || text.find('\0') != std::string::npos) throw std::runtime_error("Invalid local data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) throw std::runtime_error("Invalid local data JSON");
    auto* schema = cJSON_GetObjectItemCaseSensitive(root.get(), "schema");
    if (!cJSON_IsNumber(schema) || schema->valuedouble != 1) throw std::runtime_error("Unsupported local data schema");
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
void PlayerData::apply(Entry& entry) const {
    auto found = games.find(entry.titleId);
    const GameRecord record = found == games.end() ? GameRecord{} : found->second;
    entry.wasInstalled = record.wasInstalled; entry.rating = record.rating;
    entry.launches = record.launches; entry.firstSeenInstalled = record.firstSeenInstalled; entry.lastLaunch = record.lastLaunch;
}
void PlayerData::apply(std::vector<Entry>& entries) const { for (auto& entry : entries) apply(entry); }
PlayerData parsePlayerData(const std::string& text) {
    auto root = parse(text); auto* games = cJSON_GetObjectItemCaseSensitive(root.get(), "games");
    if (!cJSON_IsObject(games)) throw std::runtime_error("Local game records are missing");
    PlayerData data;
    for (auto* item = games->child; item; item = item->next) {
        const std::string id = item->string; requireId(id);
        if (!cJSON_IsObject(item) || data.games.count(id) || data.games.size() >= MaxEntries) throw std::runtime_error("Invalid or duplicate local game record");
        data.games[id] = {boolean(item, "was_installed"), unsigned(number(item, "rating", 5)),
            number(item, "first_seen_installed"), number(item, "launches"), number(item, "last_launch")};
    }
    return data;
}
std::string serializePlayerData(const PlayerData& data) {
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 1);
    auto* games = cJSON_AddObjectToObject(root.get(), "games");
    for (const auto& game : data.games) {
        auto* record = cJSON_AddObjectToObject(games, game.first.c_str());
        cJSON_AddBoolToObject(record, "was_installed", game.second.wasInstalled);
        cJSON_AddNumberToObject(record, "rating", game.second.rating);
        cJSON_AddNumberToObject(record, "first_seen_installed", game.second.firstSeenInstalled);
        cJSON_AddNumberToObject(record, "launches", game.second.launches);
        cJSON_AddNumberToObject(record, "last_launch", game.second.lastLaunch);
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
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_GetObjectItemCaseSensitive(root.get(), i ? "remote" : "installed");
        if (!cJSON_IsObject(item)) throw std::runtime_error("Saved library position is missing");
        auto& cursor = state.cursor[i]; cursor.titleId = string(item, "title_id", 16); cursor.url = string(item, "url", 2047);
        cursor.index = number(item, "index", MaxEntries);
        if ((!cursor.titleId.empty() && !validTitleId(cursor.titleId)) || (!cursor.url.empty() && !isHttpUrl(cursor.url)))
            throw std::runtime_error("Invalid saved library position");
    }
    return state;
}
std::string serializeViewState(const ViewState& state) {
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 1);
    cJSON_AddBoolToObject(root.get(), "available", state.available); cJSON_AddBoolToObject(root.get(), "sidebar", state.sidebar);
    cJSON_AddStringToObject(root.get(), "query", state.query.c_str());
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_AddObjectToObject(root.get(), i ? "remote" : "installed");
        const auto& cursor = state.cursor[i];
        cJSON_AddStringToObject(item, "title_id", cursor.titleId.c_str()); cJSON_AddStringToObject(item, "url", cursor.url.c_str());
        cJSON_AddNumberToObject(item, "index", cursor.index);
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
