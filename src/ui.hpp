#pragma once
#include "library.hpp"
#include "storage.hpp"
#include "userdata.hpp"
#include <deque>
#include <optional>

namespace psgi {
enum class Button { Up, Down, Left, Right, Cross, Circle, Square, Triangle, Options, L1, R1 };
enum class Screen { Splash, Dashboard, Options, Settings, Sources, Storage, Details, Actions, Rating, About, ConfirmExit, ConfirmInstall, InstallError, ConfirmUninstall, Discard, Keyboard };
enum class CommandType { Refresh, Save, Test, Storage, SetStorage, ClearArtwork, RefreshArtwork, Install, Uninstall, Rate, Launch, Keyboard, Quit };
struct Command { CommandType type; Entry entry; size_t source = 0; std::string text; std::optional<Settings> configuration = {}; };
struct TileRow { std::string group; std::vector<size_t> items; };
enum class EditKind { SourceUrl, SourceCategory, Search };

class Ui {
    std::deque<Command> commands;
    std::vector<TileRow> grid;
    std::vector<size_t> gridRowByPosition;
    std::string selectionKey() const;
    void emit(CommandType type, const Entry& entry = {});
    void beginEdit(EditKind kind, size_t index, const std::string& text);
    void quickSave();
public:
    Settings settings, draft;
    PlayerData player;
    std::vector<Entry> installed, available;
    std::set<std::string> queued;
    std::vector<VisibleEntry> visible;
    Library library = Library::Installed;
    Screen screen = Screen::Splash, detailReturn = Screen::Dashboard;
    size_t selected[2] = {0, 0}, menuIndex = 0, settingsIndex = 0, sourceIndex = 0, detailScroll = 0;
    size_t ratingIndex = 0;
    bool sidebar = false, loading = true;
    uint64_t freeBytes = 0;
    InstallStorage storage;
    bool storageChecked = false;
    size_t storageIndex = 0;
    std::string status = "Loading your library...", query, keyboardText;
    std::string settingsNotice;
    std::string installError;
    Entry failedInstall;
    EditKind editKind = EditKind::SourceUrl;
    size_t editSource = 0;
    Screen keyboardReturn = Screen::Settings;
    Entry pending;
    size_t position() const;
    const std::vector<Entry>& entries() const;
    const Entry* current() const;
    const std::vector<TileRow>& tileRows() const { return grid; }
    size_t tileRowIndex() const { return gridRowByPosition.empty() ? 0 : gridRowByPosition[position()]; }
    std::vector<std::string> details() const;
    void rebuild();
    void setLibraries(std::optional<std::vector<Entry>> local, std::optional<std::vector<Entry>> remote);
    ViewState captureView() const;
    void restoreView(const ViewState& state);
    std::string librarySummary() const;
    void handle(Button button);
    void finishEdit(const std::optional<std::string>& text);
    bool take(Command& command);
    std::vector<std::string> options() const;
    std::vector<std::string> settingRows() const;
    std::vector<std::string> actionRows() const;
};
std::string layoutName(Layout layout);
std::string groupName(Grouping group);
}
