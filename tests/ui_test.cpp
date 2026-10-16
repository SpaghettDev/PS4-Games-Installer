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
int main() {
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
    // Large-library navigation must retain short last rows and group boundaries.
    ui = Ui(); ui.screen = Screen::Dashboard; ui.library = Library::Available;
    for (int i = 0; i < 50000; ++i) {
        Entry e; e.name = "Game " + std::to_string(i); e.url = "http://nas/" + std::to_string(i);
        e.category = i % 2 ? "games" : "homebrew"; ui.available.push_back(std::move(e));
    }
    ui.settings.grouping = Grouping::Category; ui.rebuild();
    assert(ui.visible.size() == 50000 && ui.tileRows().size() == 12500);
    ui.selected[1] = 49999; ui.handle(Button::Down); assert(ui.position() == 49999);
    ui.handle(Button::Up); assert(ui.position() == 49995);
    ui.pending.name = std::string(300, static_cast<char>(0x80));
    assert(!ui.details().empty()); // Invalid metadata must not trap dialog wrapping.
    std::puts("Controller navigation, dialogs, settings and Circle exit tests passed");
}
