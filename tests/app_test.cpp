#include "app.hpp"
#include "platform.hpp"
#include "video.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
std::mutex requestMutex;
std::string lastUrl;
}
namespace psgi {
Video::Video(bool) {}
Video::~Video() {}
void Video::present(const Canvas&) {}
void Video::startKeyboard(Ui&) {}
void Video::pollKeyboard(Ui&) {}
void Video::flushInput() {}
std::vector<Button> Video::input(Ui&) { return {}; }
void prepareConsole() {}
int finishApplication(int status) { return status; }
std::vector<Entry> scanInstalled() { return {}; }
std::vector<Entry> demoLibrary() { return {}; }
uint64_t freeSpace(const std::string&) { return 0; }
void launchTitle(const std::string&) { throw std::runtime_error("PS4 required"); }
int queueInstall(const Entry&, int) { throw std::runtime_error("PS4 required"); }
TaskProgress taskProgress(int) { return {0, 0, 0, false}; }
std::string httpGet(const std::string& url, size_t, const std::atomic<bool>*) {
    std::lock_guard<std::mutex> lock(requestMutex); lastUrl = url;
    return R"({"DATA":{"http://fixture/game.pkg":{"name":"Saved source game"}}})";
}
}

using namespace psgi;
void settle(App& app, Video& video) {
    for (int i = 0; i < 200; ++i) {
        app.tick(0, video);
        if (!app.ui.loading && app.ui.screen != Screen::Splash) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false && "Application worker did not finish");
}
void editUrl(App& app, Video& video, const std::string& url) {
    auto& ui = app.ui;
    ui.settingsIndex = 0; ui.handle(Button::Cross);
    assert(ui.screen == Screen::Keyboard);
    app.tick(0, video); // Process the real keyboard command.
    ui.finishEdit(url);
    assert(ui.screen == Screen::Settings && ui.draft.sources[0].url == url);
    assert(ui.settingsNotice.find("Select Save settings") != std::string::npos);
}
int main() {
    char temporary[] = "/tmp/psgi-save-XXXXXX";
    assert(mkdtemp(temporary));
    const std::string directory = temporary, url = "http://fixture/games";
    Video video;
    {
        App app(directory); settle(app, video);
        auto& ui = app.ui;
        ui.handle(Button::Options);
        for (int i = 0; i < 6; ++i) ui.handle(Button::Down);
        ui.handle(Button::Cross); assert(ui.screen == Screen::Settings);
        editUrl(app, video, url);
        assert(loadSettings(directory, "").sources[0].url.empty()); // Accepting text is still a draft.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice == "Saved to " + directory + "/config.json");
        assert(loadSettings(directory, "").sources[0].url == url);
        settle(app, video);
        assert(ui.available.size() == 1 && ui.available[0].name == "Saved source game");
        { std::lock_guard<std::mutex> lock(requestMutex); assert(lastUrl == url); }

        // Save the draft captured when Save was pressed, even if the screen changes.
        const std::string next = "http://fixture/updated";
        editUrl(app, video, next);
        ui.settingsIndex = 10; ui.handle(Button::Cross); ui.screen = Screen::Options;
        app.tick(0, video); settle(app, video);
        assert(loadSettings(directory, "").sources[0].url == next);

        ui.screen = Screen::Settings;
        editUrl(app, video, url);
        std::filesystem::create_directory(directory + "/config.json.tmp"); // Deterministic write failure, including as root.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice.find("Save failed:") == 0);
        assert(ui.settings.sources[0].url == next && ui.draft.sources[0].url == url);
        assert(loadSettings(directory, "").sources[0].url == next);
        std::filesystem::remove(directory + "/config.json.tmp");
        ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(loadSettings(directory, "").sources[0].url == url);
        assert(ui.settingsNotice == "Saved to " + directory + "/config.json");
    }
    {
        App restarted(directory);
        assert(restarted.ui.settings.sources[0].url == url);
    }
    std::filesystem::remove_all(directory);
    std::puts("URL editor, save acknowledgement, draft snapshot, write failure/retry and restart persistence passed");
}
