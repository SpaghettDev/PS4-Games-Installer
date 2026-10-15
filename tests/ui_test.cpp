#include "ui.hpp"
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

    ui = fixture(); ui.screen = Screen::Options; ui.menuIndex = 7; ui.handle(Button::Cross);
    assert(ui.screen == Screen::About); ui.handle(Button::Circle); assert(ui.screen == Screen::Options);
    ui.menuIndex = 8; ui.handle(Button::Cross); assert(ui.screen == Screen::ConfirmExit);
    ui.handle(Button::Options); assert(ui.screen == Screen::Dashboard);
    ui.queued.insert("http://nas/b"); ui.rebuild(); ui.library = Library::Available; ui.rebuild();
    ui.selected[1] = 1; ui.handle(Button::Cross); assert(ui.screen == Screen::Dashboard && !ui.take(command));
    ui.available.clear(); ui.rebuild(); ui.handle(Button::Cross); ui.handle(Button::Triangle); ui.handle(Button::Square);
    assert(ui.screen == Screen::Dashboard); ui.handle(Button::Left); assert(ui.sidebar);
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
