#pragma once
#include "catalog.hpp"

namespace psgi {
enum class Layout { Tiles, Summary };
enum class Grouping { None, Category, Region, Letter, LocalCategory, Tag };
struct Settings {
    std::vector<Source> sources{{"games", ""}};
    Layout layout = Layout::Tiles;
    Grouping grouping = Grouping::None;
    bool sizeSort = false, notInstalledOnly = false, animations = true, refreshOnStart = true;
};
Settings parseSettings(const std::string& text);
std::string serializeSettings(const Settings& settings);
bool sameSettings(const Settings& a, const Settings& b);
void atomicWrite(const std::string& path, const std::string& text);
std::string applicationDataDirectory(const std::string& parent = "/data");
Settings loadSettings(const std::string& directory);
void saveSettings(const std::string& directory, const Settings& settings);
}
