#include "ui.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace psgi;
void drain(Ui& ui) { Command c; while (ui.take(c)) {} }
Ui fixture() {
    Ui ui; ui.screen = Screen::Dashboard; ui.loading = false;
    auto remote = parseCatalog(R"({"DATA":{"http://nas/a":{"name":"Alpha","title_id":"CUSA00001","size":"32000000000"},"http://nas/b":{"name":"Beta","title_id":"CUSA00002","size":4}}})", "games");
    Entry local = remote[0]; local.url.clear(); local.installed = true; local.installedSize = 40000000000ULL;
    ui.setLibraries(std::vector<Entry>{local}, remote);
    return ui;
}
void wrapNavigation() {
    auto ui = fixture();
    ui.library = Library::Available; ui.rebuild(); ui.pending = ui.available.front();
    ui.draft = ui.settings;
    ui.draft.sources = {{"games", "http://nas/games"}, {"updates", "http://nas/updates"}, {"dlc", "http://nas/dlc"}};
    addServerProfile(ui.draft, "Home"); addServerProfile(ui.draft, "Away");
    ui.player.createTerm(false, "Action"); ui.player.createTerm(false, "Adventure");
    ui.player.createTerm(true, "Co-op"); ui.player.createTerm(true, "Weekend");
    ui.recentSearches = {"Alpha", "Beta"};
    QueueItem first; first.entry = ui.pending; ui.downloads = {first, first, first};
    ui.backupDevices = {{"/mnt/usb0", {"one.json", "two.json", "three.json"}}, {"/mnt/usb1", {}}};
    ui.packageTitle = ui.pending.titleId;
    Entry patch = ui.pending; patch.category = "updates"; patch.url = "http://nas/patch"; ui.available.push_back(patch); ui.rebuild();
    struct Menu { Screen screen; size_t Ui::*cursor; size_t count, first = 0; };
    const Menu menus[] = {
        {Screen::Options, &Ui::menuIndex, ui.options().size()},
        {Screen::Settings, &Ui::settingsIndex, ui.settingRows().size()},
        {Screen::Sources, &Ui::sourceIndex, ui.draft.sources.size(), 1},
        {Screen::Storage, &Ui::storageIndex, 2},
        {Screen::Actions, &Ui::menuIndex, ui.actionRows().size()},
        {Screen::Rating, &Ui::ratingIndex, 6},
        {Screen::LocalInfo, &Ui::localIndex, ui.localRows().size()},
        {Screen::PickCategory, &Ui::pickerIndex, ui.termChoices(false, true).size() + 1},
        {Screen::PickTags, &Ui::pickerIndex, ui.termChoices(true, true).size() + 1},
        {Screen::Collections, &Ui::collectionIndex, ui.collectionRows().size()},
        {Screen::BulkMenu, &Ui::bulkIndex, ui.bulkRows().size()},
        {Screen::SavedCollections, &Ui::savedIndex, ui.savedRows().size()},
        {Screen::EditSaved, &Ui::savedEditIndex, ui.savedEditRows().size()},
        {Screen::Queue, &Ui::queueIndex, ui.downloads.size()},
        {Screen::Backups, &Ui::backupIndex, ui.backupDevices.size()},
        {Screen::BackupFiles, &Ui::backupFileIndex, ui.backupDevices.front().files.size()},
        {Screen::Diagnostics, &Ui::backupIndex, ui.backupDevices.size()},
        {Screen::SourceSetup, &Ui::setupIndex, 3},
        {Screen::Packages, &Ui::packageIndex, ui.packageRows().size()},
        {Screen::Search, &Ui::searchIndex, ui.searchRows().size()},
        {Screen::JumpLetter, &Ui::jumpIndex, ui.letters().size()},
        {Screen::ServerProfiles, &Ui::profileIndex, ui.profileRows().size()}
    };
    for (const auto& menu : menus) {
        ui.screen = menu.screen; ui.backupIndex = 0;
        auto& cursor = ui.*menu.cursor;
        cursor = menu.first;
        ui.handle(Button::Up); assert(cursor == menu.first + menu.count - 1);
        ui.handle(Button::Down); assert(cursor == menu.first);
        for (size_t i = 0; i < menu.count; ++i) ui.handle(Button::Down);
        assert(cursor == menu.first);
        for (size_t i = 0; i < menu.count; ++i) ui.handle(Button::Up);
        assert(cursor == menu.first && ui.screen == menu.screen);
        Command command; assert(!ui.take(command)); // Navigation never activates an entry.
    }
    // Dynamic lists may shrink or become empty while a menu is open.
    ui.screen = Screen::ServerProfiles; ui.profileIndex = 100;
    ui.draft.profiles.resize(1); ui.handle(Button::Up); assert(ui.profileIndex == 0);
    ui.draft.profiles.clear(); ui.handle(Button::Down); assert(ui.profileIndex == 0);
    ui.screen = Screen::Queue; ui.downloads.clear(); ui.queueIndex = 100;
    ui.handle(Button::Up); ui.handle(Button::Down); assert(ui.queueIndex == 0);
    ui.screen = Screen::Diagnostics; ui.backupDevices.clear(); ui.backupIndex = 100;
    ui.handle(Button::Down); assert(ui.backupIndex == 0);
    ui.screen = Screen::PickTags; ui.player = {}; ui.localDraft = {}; ui.pickerIndex = 0;
    ui.handle(Button::Up); ui.handle(Button::Down); assert(ui.pickerIndex == 0);
    // Additional sources omit the primary source, and still contain Add when empty.
    ui.screen = Screen::Sources; ui.draft.sources.resize(1); ui.sourceIndex = 1;
    ui.handle(Button::Up); ui.handle(Button::Down); assert(ui.sourceIndex == 1);
    ui.draft.sources.push_back({"dlc", "http://nas/dlc"}); ui.sourceIndex = 1;
    ui.handle(Button::Up); assert(ui.sourceIndex == 2);
    ui.handle(Button::Triangle); // Add row must not delete the primary or last source.
    assert(ui.draft.sources.size() == 2);
    ui.handle(Button::Down); ui.handle(Button::Triangle); assert(ui.draft.sources.size() == 1);
    ui.handle(Button::Up); assert(ui.sourceIndex == 1);
    ui = fixture(); ui.sidebar = true; ui.sidebarIndex = 0;
    ui.handle(Button::Up); assert(ui.sidebarIndex == 2 && ui.sidebar && ui.updatesOnly);
    ui.handle(Button::Down); assert(ui.sidebarIndex == 0 && ui.library == Library::Installed && !ui.updatesOnly);
    ui.handle(Button::Up); ui.handle(Button::Cross); assert(ui.screen == Screen::Dashboard && !ui.sidebar && ui.updatesOnly);
    ui.handle(Button::Options); ui.menuIndex = 12; ui.handle(Button::Cross); assert(ui.screen == Screen::Queue);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Options);
    // A ragged final tile row wraps vertically and clamps to its nearest column.
    ui = {}; ui.screen = Screen::Dashboard; ui.library = Library::Available;
    std::vector<Entry> games;
    for (int i = 0; i < 13; ++i) { Entry game; game.name = "Game " + std::to_string(100 + i); game.url = "http://nas/" + std::to_string(i); games.push_back(game); }
    ui.setLibraries({}, games); ui.settings.grouping = Grouping::None; ui.rebuild();
    ui.selected[1] = 0; ui.handle(Button::Up); assert(ui.position() == 12);
    ui.handle(Button::Down); assert(ui.position() == 0);
    ui.selected[1] = 3; ui.handle(Button::Up); assert(ui.position() == 12);
    ui.handle(Button::Left); assert(ui.sidebar); // Left at a row start still reaches the sidebar.
    ui.handle(Button::Right); assert(!ui.sidebar);
    ui.settings.layout = Layout::Summary; ui.selected[1] = 0; ui.rebuild();
    ui.handle(Button::Up); assert(ui.position() == 12);
    ui.handle(Button::Down); assert(ui.position() == 0);
    games.resize(1); ui.setLibraries({}, games);
    for (Layout layout : {Layout::Tiles, Layout::Summary}) {
        ui.settings.layout = layout; ui.rebuild(); ui.handle(Button::Up); ui.handle(Button::Down); assert(ui.position() == 0);
    }
    games.clear(); ui.setLibraries({}, games); ui.handle(Button::Up); ui.handle(Button::Down); assert(ui.position() == 0);
    // Directional navigation cannot confirm destructive dialogs.
    for (Screen screen : {Screen::ConfirmExit, Screen::ConfirmUninstall, Screen::ConfirmCancelQueue, Screen::ConfirmRestore}) {
        ui.screen = screen; ui.handle(Button::Up); ui.handle(Button::Down);
        Command command; assert(ui.screen == screen && !ui.take(command));
    }
    std::puts("Wraparound navigation passed for 22 menus, sidebar, ragged tiles, summary, shrinking/empty lists and confirmation guards");
}
int main() {
    wrapNavigation();
    Command command;
    auto ui = fixture();
    ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::Launch && command.entry.titleId == "CUSA00001");
    ui.handle(Button::Square); assert(ui.screen == Screen::Details);
    ui.handle(Button::Options); assert(ui.screen == Screen::Details);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Dashboard && !ui.take(command));
    ui.handle(Button::Circle); assert(ui.screen == Screen::ConfirmExit && !ui.take(command));
    ui.handle(Button::Cross); assert(ui.screen == Screen::Dashboard);
    ui.handle(Button::Circle); ui.handle(Button::Circle);
    assert(ui.take(command) && command.type == CommandType::Quit && !ui.take(command));

    ui = fixture();
    ui.handle(Button::R1); assert(ui.library == Library::Available && ui.visible.size() == 2);
    ui.handle(Button::Right); assert(ui.position() == 1);
    ui.handle(Button::Cross); assert(ui.screen == Screen::ConfirmInstall && !ui.take(command));
    ui.handle(Button::Options); assert(ui.screen == Screen::ConfirmInstall);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Dashboard && !ui.take(command));
    ui.handle(Button::Cross); ui.handle(Button::Cross);
    assert(ui.screen == Screen::Dashboard && ui.take(command) && command.type == CommandType::Install);
    assert(command.entry.url == "http://nas/b" && !ui.take(command));
    ui.screen = Screen::InstallError; ui.installError = "Register local package: 0x80990086";
    ui.failedInstall = command.entry; ui.handle(Button::Options);
    assert(ui.screen == Screen::InstallError && !ui.take(command));
    ui.handle(Button::Cross); assert(ui.screen == Screen::ConfirmInstall && ui.installError.empty() && !ui.take(command));
    ui.handle(Button::Circle); assert(ui.screen == Screen::Dashboard);
    ui.screen = Screen::InstallError; ui.installError = "Fixture failure"; ui.handle(Button::Circle);
    assert(ui.screen == Screen::Dashboard && ui.installError.empty() && !ui.take(command));
    ui.handle(Button::Triangle); assert(ui.screen == Screen::Actions);
    ui.handle(Button::Down); ui.handle(Button::Cross); assert(ui.screen == Screen::Details);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Actions);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Dashboard);
    ui.handle(Button::L1); assert(ui.library == Library::Installed);
    ui.handle(Button::R1); assert(ui.position() == 1);
    ui.handle(Button::Left); ui.handle(Button::Left); assert(ui.sidebar);
    ui.handle(Button::Up); assert(ui.library == Library::Installed);
    ui.handle(Button::Cross); assert(!ui.sidebar);

    ui = fixture();
    ui.handle(Button::Options);
    for (int i = 0; i < 6; ++i) ui.handle(Button::Down);
    ui.handle(Button::Cross); assert(ui.screen == Screen::Settings);
    ui.handle(Button::Cross); assert(ui.screen == Screen::Keyboard);
    assert(ui.take(command) && command.type == CommandType::Keyboard);
    ui.finishEdit({}); assert(ui.screen == Screen::Settings && ui.draft.sources[0].url.empty());
    ui.handle(Button::Cross); drain(ui); ui.finishEdit("file:///bad");
    assert(ui.draft.sources[0].url.empty() && ui.screen == Screen::Settings);
    ui.handle(Button::Cross); drain(ui); ui.finishEdit("http://nas/games.json");
    assert(ui.draft.sources[0].url == "http://nas/games.json" && ui.settings.sources[0].url.empty());
    ui.handle(Button::Circle); assert(ui.screen == Screen::Discard);
    ui.handle(Button::Cross); assert(ui.screen == Screen::Settings);
    ui.handle(Button::Circle); ui.handle(Button::Circle);
    assert(ui.screen == Screen::Options && ui.draft.sources[0].url.empty());

    ui = fixture();
    ui.screen = Screen::Settings; ui.draft = ui.settings; ui.settingsIndex = 1;
    ui.handle(Button::Cross); assert(ui.screen == Screen::Sources);
    ui.handle(Button::Cross); assert(ui.screen == Screen::Keyboard);
    drain(ui); ui.finishEdit("homebrew");
    assert(ui.screen == Screen::Keyboard && ui.draft.sources.size() == 2);
    drain(ui); ui.finishEdit("http://nas/homebrew.json");
    assert(ui.screen == Screen::Sources && ui.draft.sources[1].url == "http://nas/homebrew.json");
    ui.handle(Button::Triangle); assert(ui.draft.sources.size() == 1);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Settings);
    ui.settingsIndex = 10; ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::Save);

    ui.settingsIndex = 11; ui.handle(Button::Cross);
    assert(ui.screen == Screen::Storage);
    assert(ui.take(command) && command.type == CommandType::Storage);
    // Inspecting storage must not apply/discard unsaved configuration or queue an install.
    auto draft = serializeSettings(ui.draft);
    ui.storage.internal = {true, 500000000000ULL}; ui.storage.external = {true, 1000000000000ULL};
    ui.storageChecked = true;
    ui.handle(Button::Cross); ui.handle(Button::Right);
    assert(!ui.take(command) && serializeSettings(ui.draft) == draft);
    ui.storage.destination = InstallDestination::Internal;
    ui.handle(Button::Down); ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::SetStorage && command.source == 1);
    assert(serializeSettings(ui.draft) == draft); // Registry choice does not save/discard URL drafts.
    ui.handle(Button::Up); ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::SetStorage && command.source == 0);
    ui.storage.external = {}; ui.handle(Button::Down); ui.handle(Button::Cross);
    assert(!ui.take(command)); // Never silently fall back to the other physical drive.
    ui.storage.external = {}; ui.handle(Button::Triangle);
    assert(ui.take(command) && command.type == CommandType::Storage);
    ui.handle(Button::Circle);
    assert(ui.screen == Screen::Settings && serializeSettings(ui.draft) == draft);

    ui = fixture(); ui.screen = Screen::Options; ui.menuIndex = 7; ui.handle(Button::Cross);
    assert(ui.screen == Screen::About); ui.handle(Button::Circle); assert(ui.screen == Screen::Options);
    ui.menuIndex = 8; ui.handle(Button::Cross); assert(ui.screen == Screen::ConfirmExit);
    ui.handle(Button::Options); assert(ui.screen == Screen::Dashboard);
    ui.queued.insert("http://nas/b"); ui.rebuild(); ui.library = Library::Available; ui.rebuild();
    ui.selected[1] = 1; ui.handle(Button::Cross); assert(ui.screen == Screen::Dashboard && !ui.take(command));
    ui.available.clear(); ui.rebuild(); ui.handle(Button::Cross); ui.handle(Button::Triangle); ui.handle(Button::Square);
    assert(ui.screen == Screen::Dashboard); ui.handle(Button::Left); assert(ui.sidebar);
    // Personal actions require explicit confirmation, and ratings remain local commands.
    ui = fixture();
    auto choose = [&](const std::string& prefix) {
        ui.screen = Screen::Dashboard; ui.handle(Button::Triangle);
        const auto rows = ui.actionRows();
        const auto found = std::find_if(rows.begin(), rows.end(), [&](const std::string& row) { return row.find(prefix) == 0; });
        assert(found != rows.end()); ui.menuIndex = found - rows.begin(); ui.handle(Button::Cross);
    };
    choose("Uninstall"); assert(ui.screen == Screen::ConfirmUninstall && !ui.take(command));
    ui.handle(Button::Circle); assert(ui.screen == Screen::Actions && !ui.take(command));
    choose("Uninstall"); ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::Uninstall && command.entry.titleId == "CUSA00001");
    assert(ui.screen == Screen::Dashboard && !ui.take(command));
    ui.installed[0].queued = true;
    choose("Uninstall"); assert(ui.screen == Screen::Actions && !ui.take(command));
    ui.installed[0].queued = false;
    for (unsigned stars = 1; stars <= 5; ++stars) {
        choose("Rate game"); assert(ui.screen == Screen::Rating);
        ui.ratingIndex = stars - 1; ui.handle(Button::Cross);
        assert(ui.screen == Screen::Actions && ui.take(command) && command.type == CommandType::Rate && command.source == stars);
    }
    choose("Rate game"); ui.handle(Button::Circle);
    assert(ui.screen == Screen::Actions && !ui.take(command));
    choose("Rate game"); ui.ratingIndex = 5; ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::Rate && command.source == 0);
    ui.handle(Button::Circle); ui.handle(Button::R1); ui.selected[1] = 1;
    const auto remoteRows = [&] { ui.handle(Button::Triangle); return ui.actionRows(); }();
    assert(std::find(remoteRows.begin(), remoteRows.end(), "Uninstall") == remoteRows.end());
    ui = fixture();
    choose("Edit local info"); assert(ui.screen == Screen::LocalInfo && !ui.localDirty());
    ui.handle(Button::Cross); assert(ui.screen == Screen::PickCategory);
    ui.handle(Button::Triangle); assert(ui.screen == Screen::Keyboard);
    drain(ui); ui.finishEdit("Action"); assert(ui.screen == Screen::PickCategory && ui.localDraft.category == "Action");
    ui.handle(Button::Circle); ui.handle(Button::Down); ui.handle(Button::Cross);
    assert(ui.editKind == EditKind::GameLabel); drain(ui); ui.finishEdit("Favorite");
    assert(ui.localDraft.label == "Favorite" && ui.player.games.empty());
    ui.handle(Button::Down); ui.handle(Button::Cross); assert(ui.screen == Screen::PickTags);
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("Weekend");
    assert(ui.localDraft.tags.size() == 1);
    ui.handle(Button::Down); ui.handle(Button::Cross); assert(ui.localDraft.tags.empty());
    ui.handle(Button::Cross); // Removing the last draft-only tag must clamp the picker cursor.
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("Weekend"); ui.handle(Button::Circle);
    ui.handle(Button::Down); ui.handle(Button::Cross); drain(ui); ui.finishEdit("My local notes");
    assert(ui.localDraft.description == "My local notes");
    ui.handle(Button::Circle); assert(ui.screen == Screen::DiscardLocal);
    ui.handle(Button::Cross); assert(ui.screen == Screen::LocalInfo && ui.localDirty());
    ui.localIndex = 4; ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::SaveLocalInfo && command.personal->category == "Action");
    assert(ui.screen == Screen::LocalInfo); // Stay open until the app acknowledges disk persistence.
    ui.handle(Button::Circle); ui.handle(Button::Circle);
    assert(ui.screen == Screen::Actions && ui.localDraft.category.empty());
    ui.screen = Screen::Options; ui.menuIndex = 9; ui.handle(Button::Cross);
    assert(ui.screen == Screen::Collections && !ui.collectionTags);
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("Adventure");
    assert(ui.take(command) && command.type == CommandType::CreateCollection && command.text == "Adventure" && command.source == 0);
    ui.player.createTerm(false, "Adventure"); ui.collectionIndex = 2;
    ui.handle(Button::Square); assert(ui.editKind == EditKind::RenameCategory); drain(ui); ui.finishEdit("Story games");
    assert(ui.take(command) && command.type == CommandType::RenameCollection && command.previous == "Adventure");
    ui.handle(Button::Options); assert(ui.screen == Screen::ConfirmDeleteCollection);
    ui.handle(Button::Circle); assert(ui.screen == Screen::Collections && !ui.take(command));
    ui.handle(Button::Options); ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::DeleteCollection && command.text == "Adventure");
    ui.handle(Button::Cross); drain(ui);
    assert(ui.screen == Screen::Dashboard && ui.filter.category == "Adventure" && ui.settings.grouping == Grouping::LocalCategory);
    assert(keyboardMaximum(EditKind::GameDescription) == MaxLocalDescription);
    // Direct assignments reuse local names and never open the full metadata editor.
    ui = fixture();
    LocalInfo info{"Action", "Favorite", "Keep these notes", {"Weekend"}};
    ui.player.setInfo("CUSA00001", info); ui.player.createTerm(false, "Adventure");
    ui.player.createTerm(true, "Co-op"); ui.player.rate("CUSA00001", 5); ui.rebuild();
    choose("Assign category");
    assert(ui.screen == Screen::PickCategory && ui.quickAssignment && ui.pickerIndex == 1);
    ui.handle(Button::Down); ui.handle(Button::Cross);
    assert(ui.take(command) && command.type == CommandType::SaveLocalInfo && command.returnToActions);
    assert(command.entry.titleId == "CUSA00001" && command.personal->category == "Adventure");
    assert(command.personal->label == "Favorite" && command.personal->description == "Keep these notes" && command.personal->tags == info.tags);
    assert(ui.screen == Screen::PickCategory); // Only persistence acknowledges success.
    ui.handle(Button::Circle);
    assert(ui.screen == Screen::Actions && !ui.quickAssignment && sameInfo(ui.localDraft, info) && !ui.take(command));
    choose("Assign category"); ui.handle(Button::Triangle); drain(ui); ui.finishEdit("New category");
    assert(ui.screen == Screen::PickCategory && ui.localDraft.category == "New category" && ui.pickerIndex == 3);
    assert(!ui.take(command)); ui.handle(Button::Cross);
    assert(ui.take(command) && command.personal->category == "New category" && command.returnToActions);
    ui.handle(Button::Circle); // Cancel a draft-only name without creating a registry entry.
    assert(ui.player.categories.size() == 2);
    choose("Assign tags");
    assert(ui.screen == Screen::PickTags && ui.pickerIndex == 2);
    ui.handle(Button::Cross); assert(ui.localDraft.tags.empty() && !ui.take(command));
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("New tag");
    assert(ui.pickerIndex == 3 && ui.localDraft.tags == std::vector<std::string>{"New tag"});
    ui.handle(Button::Square);
    assert(ui.take(command) && command.type == CommandType::SaveLocalInfo && command.returnToActions);
    assert(command.personal->tags == std::vector<std::string>{"New tag"} && command.personal->category == "Action");
    ui.handle(Button::Circle);
    assert(ui.screen == Screen::Actions && sameInfo(ui.localDraft, info) && !ui.take(command));
    choose("Assign tags"); ui.pickerIndex = 0; ui.handle(Button::Cross); ui.handle(Button::Square);
    assert(ui.take(command) && command.personal->tags.empty()); ui.handle(Button::Circle);
    assert(ui.player.games.at("CUSA00001").rating == 5 && sameInfo(ui.player.games.at("CUSA00001").info, info));
    ui.handle(Button::Circle); ui.handle(Button::R1); ui.selected[1] = 1;
    choose("Assign category"); assert(ui.pending.titleId == "CUSA00002" && ui.localDraft.category.empty());
    ui.handle(Button::Cross);
    assert(ui.take(command) && command.entry.titleId == "CUSA00002" && command.personal->category == "Action");
    ui = fixture(); ui.installed[0].titleId.clear(); ui.handle(Button::Triangle);
    const auto noId = ui.actionRows();
    assert(std::none_of(noId.begin(), noId.end(), [](const std::string& row) { return row.find("Assign ") == 0; }));
    // Large-library navigation must retain short last rows and group boundaries.
    ui = Ui(); ui.screen = Screen::Dashboard; ui.library = Library::Available;
    for (int i = 0; i < 50000; ++i) {
        Entry e; e.name = "Game " + std::to_string(i); e.url = "http://nas/" + std::to_string(i);
        e.category = i % 2 ? "games" : "homebrew"; ui.available.push_back(std::move(e));
    }
    ui.settings.grouping = Grouping::Category; ui.rebuild();
    assert(ui.visible.size() == 50000 && ui.tileRows().size() == 12500);
    ui.selected[1] = 49999; ui.handle(Button::Down); assert(ui.position() == 3);
    ui.handle(Button::Up); assert(ui.position() == 49999);
    ui.handle(Button::Up); assert(ui.position() == 49995);
    ui.pending.name = std::string(300, static_cast<char>(0x80));
    assert(!ui.details().empty()); // Invalid metadata must not trap dialog wrapping.
    std::puts("Controller navigation, dialogs, settings and Circle exit tests passed");
}
