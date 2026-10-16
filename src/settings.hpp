#pragma once
#include "catalog.hpp"

namespace psgi {
enum class Layout { Tiles, Summary };
enum class Grouping { None, Category, Region, Letter, LocalCategory, Tag, Title };
enum class SortField { Name, Size, Rating, LastLaunch, FirstInstalled };
struct ServerProfile { std::string name; std::vector<Source> sources; };
constexpr size_t MaxServerProfiles = 16;
struct Settings {
    std::vector<Source> sources{{"games", ""}};
    std::vector<ServerProfile> profiles;
    std::string activeProfile;
    Layout layout = Layout::Tiles;
    Grouping grouping = Grouping::None;
    SortField sort = SortField::Name;
    bool descending = false, notInstalledOnly = false, animations = true, refreshOnStart = true;
};
std::string sortName(SortField field);
void syncActiveProfile(Settings& settings);
void addServerProfile(Settings& settings, const std::string& name);
void useServerProfile(Settings& settings, size_t index);
void renameServerProfile(Settings& settings, size_t index, const std::string& name);
void deleteServerProfile(Settings& settings, size_t index);
std::string catalogCachePath(const std::string& directory, const Settings& settings);
Settings parseSettings(const std::string& text);
std::string serializeSettings(const Settings& settings);
bool sameSettings(const Settings& a, const Settings& b);
void atomicWrite(const std::string& path, const std::string& text);
std::string applicationDataDirectory(const std::string& parent = "/data");
Settings loadSettings(const std::string& directory);
void saveSettings(const std::string& directory, const Settings& settings);
}
