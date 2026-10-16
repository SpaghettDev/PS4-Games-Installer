#pragma once
#include "catalog.hpp"

namespace psgi {
enum class Layout { Tiles, Summary };
enum class Grouping { None, Category, Region, Letter, LocalCategory, Tag, Title };
enum class SortField { Name, Size, Rating, LastLaunch, FirstInstalled };
struct Settings {
    std::vector<Source> sources{{"games", ""}};
    Layout layout = Layout::Tiles;
    Grouping grouping = Grouping::None;
    SortField sort = SortField::Name;
    bool descending = false, notInstalledOnly = false, animations = true, refreshOnStart = true;
};
std::string sortName(SortField field);
Settings parseSettings(const std::string& text);
std::string serializeSettings(const Settings& settings);
bool sameSettings(const Settings& a, const Settings& b);
void atomicWrite(const std::string& path, const std::string& text);
std::string applicationDataDirectory(const std::string& parent = "/data");
Settings loadSettings(const std::string& directory);
void saveSettings(const std::string& directory, const Settings& settings);
}
