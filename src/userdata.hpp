#pragma once
#include "catalog.hpp"
#include <map>

namespace psgi {
struct GameRecord {
    bool wasInstalled = false;
    unsigned rating = 0;
    uint64_t firstSeenInstalled = 0, launches = 0, lastLaunch = 0;
};
struct PlayerData {
    std::map<std::string, GameRecord> games;
    bool observeInstalled(const std::vector<Entry>& entries, uint64_t now);
    void rate(const std::string& titleId, unsigned stars);
    void noteLaunch(const std::string& titleId, uint64_t now);
    void apply(Entry& entry) const;
    void apply(std::vector<Entry>& entries) const;
};
struct LibraryCursor { std::string titleId, url; size_t index = 0; };
struct ViewState {
    bool available = false, sidebar = false;
    std::string query;
    LibraryCursor cursor[2];
};
PlayerData parsePlayerData(const std::string& text);
std::string serializePlayerData(const PlayerData& data);
PlayerData loadPlayerData(const std::string& directory);
void savePlayerData(const std::string& directory, const PlayerData& data);
ViewState parseViewState(const std::string& text);
std::string serializeViewState(const ViewState& state);
ViewState loadViewState(const std::string& directory);
void saveViewState(const std::string& directory, const ViewState& state);
}
