#include "ui.hpp"
#include <algorithm>
#include <map>

namespace psgi {
std::string layoutName(Layout layout) { return layout == Layout::Tiles ? "Tiles" : "Summary list"; }
std::string groupName(Grouping group) {
    const char* names[] = {"None", "Category", "Region", "First letter"};
    return names[static_cast<int>(group)];
}
size_t Ui::position() const { return selected[static_cast<int>(library)]; }
const std::vector<Entry>& Ui::entries() const { return library == Library::Installed ? installed : available; }
const Entry* Ui::current() const { return visible.empty() ? nullptr : &entries()[visible[position()].index]; }
std::string Ui::selectionKey() const {
    auto* e = current(); return e ? (e->url.empty() ? e->titleId : e->url) : "";
}
void Ui::emit(CommandType type, const Entry& entry) { commands.push_back({type, entry, 0, ""}); }
bool Ui::take(Command& out) {
    if (commands.empty()) return false;
    out = std::move(commands.front()); commands.pop_front(); return true;
}
void Ui::rebuild() {
    matchInstalled(available, installed, queued);
    std::set<std::string> queuedTitles;
    for (const auto& e : available) if (e.queued && !e.titleId.empty()) queuedTitles.insert(e.titleId);
    for (auto& e : installed) e.queued = queuedTitles.count(e.titleId) != 0;
    // Index once so large remote catalogs do not multiply installed-library work.
    std::map<std::string, const Entry*> baseTitles;
    for (const auto& e : available) if (!e.titleId.empty() && e.category != "updates" && e.category != "dlc" && e.category != "themes") {
        auto found = baseTitles.find(e.titleId);
        if (found == baseTitles.end() || e.category == "games") baseTitles[e.titleId] = &e;
    }
    for (auto& local : installed) {
        auto found = baseTitles.find(local.titleId);
        if (found != baseTitles.end()) {
            const auto& remote = *found->second;
            local.coverUrl = remote.coverUrl; local.url = remote.url; local.size = remote.size;
            local.release = remote.release; local.minFirmware = remote.minFirmware;
            local.description = remote.description;
            if (!remote.category.empty()) local.category = remote.category;
            if (local.region.empty()) local.region = remote.region;
        }
    }
    visible = makeView(entries(), settings, library, query);
    selected[static_cast<int>(library)] = visible.empty() ? 0 : std::min(position(), visible.size() - 1);
    grid.clear(); gridRowByPosition.clear();
    for (size_t i = 0; i < visible.size(); ++i) {
        if (grid.empty() || grid.back().items.size() == 4 || grid.back().group != visible[i].group)
            grid.push_back({visible[i].group, {}});
        grid.back().items.push_back(i); gridRowByPosition.push_back(grid.size() - 1);
    }
}
void Ui::setLibraries(std::optional<std::vector<Entry>> local, std::optional<std::vector<Entry>> remote) {
    std::string key = selectionKey();
    if (local) installed = std::move(*local);
    if (remote) available = std::move(*remote);
    rebuild();
    if (!key.empty()) for (size_t i = 0; i < visible.size(); ++i) {
        const auto& e = entries()[visible[i].index];
        if ((e.url.empty() ? e.titleId : e.url) == key) selected[static_cast<int>(library)] = i;
    }
}
std::vector<std::string> Ui::details() const {
    std::vector<std::string> lines;
    for (const auto& field : entryDetails(pending)) {
        std::string line = field.first + ": " + field.second;
        while (line.size() > 72) {
            size_t cut = line.rfind(' ', 72);
            if (cut < 20 || cut == std::string::npos) cut = 72;
            while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xc0) == 0x80) --cut;
            if (!cut) cut = 72; // Invalid UTF-8 must still make forward progress.
            lines.push_back(line.substr(0, cut));
            line.erase(0, cut + (line[cut] == ' ' ? 1 : 0));
        }
        lines.push_back(line);
    }
    return lines;
}
void Ui::beginEdit(EditKind kind, size_t index, const std::string& text) {
    editKind = kind; editSource = index; keyboardText = text;
    keyboardReturn = screen; screen = Screen::Keyboard;
    commands.push_back({CommandType::Keyboard, {}, index, text});
}
void Ui::finishEdit(const std::optional<std::string>& text) {
    screen = keyboardReturn;
    if (!text) return;
    if (editKind == EditKind::Search) { query = *text; rebuild(); return; }
    if (editKind == EditKind::SourceUrl) {
        if (!text->empty() && !isHttpUrl(*text)) { status = "Invalid URL. Use http:// or https:// with no spaces."; return; }
        draft.sources.at(editSource).url = *text;
    } else {
        std::string category = lower(*text);
        if (category.empty() || category.size() > 32 || category == "ps5" || category.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_- ") != std::string::npos) {
            status = "Use a short PS4 category name (letters, numbers, spaces, - or _)."; return;
        }
        for (const auto& source : draft.sources) if (source.category == category) { status = "That category already exists."; return; }
        draft.sources.push_back({category, ""});
        sourceIndex = draft.sources.size() - 1;
        beginEdit(EditKind::SourceUrl, sourceIndex, "");
    }
}
std::vector<std::string> Ui::options() const {
    return {"Refresh library", "Search" + (query.empty() ? std::string() : ": " + query),
        "View: " + layoutName(settings.layout), "Group: " + groupName(settings.grouping),
        settings.sizeSort ? "Sort: Largest first" : "Sort: Name A-Z",
        settings.notInstalledOnly ? "Available: Not installed" : "Available: All packages", "Settings", "About", "Exit"};
}
std::vector<std::string> Ui::settingRows() const {
    return {"Games JSON URL", "Other catalog sources", "View: " + layoutName(draft.layout),
        "Grouping: " + groupName(draft.grouping), draft.sizeSort ? "Sort: Largest first" : "Sort: Name A-Z",
        draft.notInstalledOnly ? "Available: Not installed" : "Available: All packages",
        draft.animations ? "Background animation: On" : "Background animation: Off",
        draft.refreshOnStart ? "Refresh on startup: On" : "Refresh on startup: Off",
        "Test connection", "Clear artwork cache", "Save settings", "Install location (PS4 system setting)"};
}
std::vector<std::string> Ui::actionRows() const {
    if (library == Library::Installed || pending.installed)
        return pending.url.empty() ? std::vector<std::string>{"Launch game", "Details", "Refresh artwork"} :
            std::vector<std::string>{"Launch game", "Install remote package", "Details", "Refresh artwork"};
    return {"Install package", "Details", "Refresh artwork"};
}
void Ui::quickSave() { rebuild(); emit(CommandType::Save); }
void Ui::handle(Button b) {
    // Circle and dialogs are handled before dashboard shortcuts.
    if (screen == Screen::Keyboard) return;
    if (screen == Screen::ConfirmExit) {
        if (b == Button::Circle) emit(CommandType::Quit);
        else if (b == Button::Cross || b == Button::Options) screen = Screen::Dashboard;
        return;
    }
    if (screen == Screen::Discard) {
        if (b == Button::Circle) { draft = settings; screen = Screen::Options; }
        else if (b == Button::Cross) screen = Screen::Settings;
        return;
    }
    if (screen == Screen::ConfirmInstall) {
        if (b == Button::Circle) screen = Screen::Dashboard;
        else if (b == Button::Cross) { emit(CommandType::Install, pending); screen = Screen::Dashboard; }
        return;
    }
    if (b == Button::Circle) {
        if (screen == Screen::Splash || screen == Screen::Dashboard) screen = Screen::ConfirmExit;
        else if (screen == Screen::Settings) screen = sameSettings(settings, draft) ? Screen::Options : Screen::Discard;
        else if (screen == Screen::Sources) screen = Screen::Settings;
        else if (screen == Screen::Storage) screen = Screen::Settings;
        else if (screen == Screen::Details) screen = detailReturn;
        else if (screen == Screen::About) screen = Screen::Options;
        else screen = Screen::Dashboard;
        return;
    }
    if (screen == Screen::Splash) return;
    if (screen == Screen::About) return;
    if (screen == Screen::Storage) {
        if (b == Button::Triangle) emit(CommandType::Storage);
        return;
    }
    if (screen == Screen::Details) {
        if (b == Button::Up && detailScroll) --detailScroll;
        if (b == Button::Down && detailScroll + 12 < details().size()) ++detailScroll;
        return;
    }
    if (screen == Screen::Options) {
        if (b == Button::Up && menuIndex) --menuIndex;
        if (b == Button::Down && menuIndex + 1 < options().size()) ++menuIndex;
        if (b != Button::Cross) return;
        switch (menuIndex) {
            case 0: emit(CommandType::Refresh); screen = Screen::Dashboard; break;
            case 1: beginEdit(EditKind::Search, 0, query); break;
            case 2: settings.layout = settings.layout == Layout::Tiles ? Layout::Summary : Layout::Tiles; quickSave(); break;
            case 3: settings.grouping = static_cast<Grouping>((static_cast<int>(settings.grouping) + 1) % 4); quickSave(); break;
            case 4: settings.sizeSort = !settings.sizeSort; quickSave(); break;
            case 5: settings.notInstalledOnly = !settings.notInstalledOnly; quickSave(); break;
            case 6: draft = settings; settingsIndex = 0; screen = Screen::Settings; break;
            case 7: screen = Screen::About; break;
            case 8: screen = Screen::ConfirmExit; break;
        }
        return;
    }
    if (screen == Screen::Settings) {
        if (b == Button::Up && settingsIndex) --settingsIndex;
        if (b == Button::Down && settingsIndex + 1 < settingRows().size()) ++settingsIndex;
        if (b != Button::Cross && b != Button::Left && b != Button::Right) return;
        if (b != Button::Cross && (settingsIndex < 2 || settingsIndex > 7)) return;
        switch (settingsIndex) {
            case 0: beginEdit(EditKind::SourceUrl, 0, draft.sources[0].url); break;
            case 1: sourceIndex = 1; screen = Screen::Sources; break;
            case 2: draft.layout = draft.layout == Layout::Tiles ? Layout::Summary : Layout::Tiles; break;
            case 3: draft.grouping = static_cast<Grouping>((static_cast<int>(draft.grouping) + 1) % 4); break;
            case 4: draft.sizeSort = !draft.sizeSort; break;
            case 5: draft.notInstalledOnly = !draft.notInstalledOnly; break;
            case 6: draft.animations = !draft.animations; break;
            case 7: draft.refreshOnStart = !draft.refreshOnStart; break;
            case 8: emit(CommandType::Test); break;
            case 9: emit(CommandType::ClearArtwork); break;
            case 10: emit(CommandType::Save); break;
            case 11: screen = Screen::Storage; emit(CommandType::Storage); break;
        }
        return;
    }
    if (screen == Screen::Sources) {
        if (b == Button::Up && sourceIndex > 1) --sourceIndex;
        if (b == Button::Down && sourceIndex < draft.sources.size()) ++sourceIndex;
        if (b == Button::Triangle && sourceIndex < draft.sources.size()) {
            draft.sources.erase(draft.sources.begin() + sourceIndex); return;
        }
        if (b == Button::Cross) {
            if (sourceIndex == draft.sources.size()) {
                if (draft.sources.size() >= 32) { status = "Maximum 32 catalog sources"; return; }
                beginEdit(EditKind::SourceCategory, sourceIndex, "");
            } else beginEdit(EditKind::SourceUrl, sourceIndex, draft.sources[sourceIndex].url);
        }
        return;
    }
    if (screen == Screen::Actions) {
        auto actions = actionRows();
        if (b == Button::Up && menuIndex) --menuIndex;
        if (b == Button::Down && menuIndex + 1 < actions.size()) ++menuIndex;
        if (b == Button::Cross) {
            auto action = actions[menuIndex];
            if (action == "Launch game") { emit(CommandType::Launch, pending); screen = Screen::Dashboard; }
            else if (action.find("Install") == 0) screen = Screen::ConfirmInstall;
            else if (action == "Details") { detailReturn = Screen::Actions; detailScroll = 0; screen = Screen::Details; }
            else { emit(CommandType::RefreshArtwork, pending); screen = Screen::Dashboard; }
        }
        return;
    }
    if (b == Button::Options) { menuIndex = 0; screen = Screen::Options; return; }
    if (b == Button::L1 || b == Button::R1) {
        library = library == Library::Installed ? Library::Available : Library::Installed; rebuild(); return;
    }
    if (sidebar) {
        if (b == Button::Up) library = Library::Installed;
        if (b == Button::Down) library = Library::Available;
        if (b == Button::Right || b == Button::Cross) sidebar = false;
        rebuild(); return;
    }
    if (auto* e = current()) {
        if (b == Button::Cross) {
            pending = *e;
            if (library == Library::Installed) {
                if (pending.queued) status = "This title is still queued. Check PS4 Downloads before launching.";
                else emit(CommandType::Launch, pending);
            }
            else if (pending.installed) { menuIndex = 0; screen = Screen::Actions; }
            else if (pending.queued) status = "This package is already queued. See PS4 Notifications > Downloads.";
            else screen = Screen::ConfirmInstall;
        } else if (b == Button::Triangle) { pending = *e; menuIndex = 0; screen = Screen::Actions; }
        else if (b == Button::Square) { pending = *e; detailReturn = Screen::Dashboard; detailScroll = 0; screen = Screen::Details; }
        else if (settings.layout == Layout::Summary) {
            if (b == Button::Up && position()) --selected[static_cast<int>(library)];
            if (b == Button::Down && position() + 1 < visible.size()) ++selected[static_cast<int>(library)];
            if (b == Button::Left) sidebar = true;
        } else {
            const auto& rows = tileRows();
            size_t row = tileRowIndex(), col = 0;
            for (size_t c = 0; c < rows[row].items.size(); ++c) if (rows[row].items[c] == position()) col = c;
            if (b == Button::Left) {
                if (col == 0) sidebar = true; else --selected[static_cast<int>(library)];
            }
            if (b == Button::Right && col + 1 < rows[row].items.size()) ++selected[static_cast<int>(library)];
            if (b == Button::Up && row) selected[static_cast<int>(library)] = rows[row-1].items[std::min(col, rows[row-1].items.size()-1)];
            if (b == Button::Down && row + 1 < rows.size()) selected[static_cast<int>(library)] = rows[row+1].items[std::min(col, rows[row+1].items.size()-1)];
        }
    } else if (b == Button::Left) sidebar = true;
}
}
