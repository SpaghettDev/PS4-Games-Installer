#include "app.hpp"
#include "platform.hpp"
#include "download.hpp"
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
std::atomic<bool> allowInstalls{false}, allowRetirement{false};
std::atomic<int> submissions{0}, progressMode{0};
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
int queueInstall(const Entry&, int) {
    if (!allowInstalls) throw std::runtime_error("PS4 required");
    ++submissions; return 45;
}
int queueLocalInstall(const Entry&, const std::string&, int) { throw std::runtime_error("PS4 required"); }
bool retireInactiveTask(int task) { assert(task == 45); return allowRetirement; }
HttpTransferInfo httpTransfer(const std::string&, const HttpSink& sink, bool range, const std::atomic<bool>*) {
    assert(allowInstalls && range); sink("\x7f" "CNT", 4, 4); return {206, 4};
}
TaskProgress taskProgress(int task) {
    assert(task == 45);
    TaskProgress result{100, 100, progressMode == 1 ? int32_t(0x80991404u) : 0, true};
    result.completed = progressMode == 2;
    return result;
}
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
void waitForStatus(App& app, Video& video, double elapsed, const char* text) {
    for (int i = 0; i < 200; ++i) {
        app.tick(elapsed, video);
        if (app.ui.status.find(text) != std::string::npos) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false && "Expected installation state was not observed");
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
        assert(loadSettings(directory).sources[0].url.empty()); // Accepting text is still a draft.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice == "Saved to " + directory + "/config.json");
        assert(loadSettings(directory).sources[0].url == url);
        settle(app, video);
        assert(ui.available.size() == 1 && ui.available[0].name == "Saved source game");
        { std::lock_guard<std::mutex> lock(requestMutex); assert(lastUrl == url); }

        // Save the draft captured when Save was pressed, even if the screen changes.
        const std::string next = "http://fixture/updated";
        editUrl(app, video, next);
        ui.settingsIndex = 10; ui.handle(Button::Cross); ui.screen = Screen::Options;
        app.tick(0, video); settle(app, video);
        assert(loadSettings(directory).sources[0].url == next);

        ui.screen = Screen::Settings;
        editUrl(app, video, url);
        std::filesystem::create_directory(directory + "/config.json.tmp"); // Deterministic write failure, including as root.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice.find("Save failed:") == 0);
        assert(ui.settings.sources[0].url == next && ui.draft.sources[0].url == url);
        assert(loadSettings(directory).sources[0].url == next);
        std::filesystem::remove(directory + "/config.json.tmp");
        ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(loadSettings(directory).sources[0].url == url);
        assert(ui.settingsNotice == "Saved to " + directory + "/config.json");
    }
    {
        App restarted(directory);
        assert(restarted.ui.settings.sources[0].url == url);
    }
    {
        auto settings = loadSettings(directory); settings.refreshOnStart = false; saveSettings(directory, settings);
        App app(directory); settle(app, video);
        allowInstalls = true;
        app.ui.handle(Button::R1); app.ui.handle(Button::Cross); app.ui.handle(Button::Cross);
        app.tick(0, video); settle(app, video);
        assert(submissions == 1 && app.ui.queued.size() == 1);
        const auto packageUrl = app.ui.available[0].url;
        assert(readFile(directory + "/tasks.json").find(packageUrl) != std::string::npos);
        std::filesystem::create_directory(directory + "/downloads");
        const auto staged = stagedPackagePath(directory, packageUrl); atomicWrite(staged, "staging fixture");

        // 100% transferred alone must not release a package still being installed.
        waitForStatus(app, video, 4, "may still be installing");
        assert(app.ui.queued.size() == 1 && std::filesystem::exists(staged));
        progressMode = 1; allowRetirement = false;
        waitForStatus(app, video, 8, "cancel the failed task");
        assert(app.ui.queued.size() == 1 && std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json").find(packageUrl) != std::string::npos);

        allowRetirement = true;
        waitForStatus(app, video, 12, "failed task removed");
        assert(app.ui.queued.empty() && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");

        // Retry becomes possible only after the owned task was actually retired.
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross);
        app.tick(12, video); settle(app, video); assert(submissions == 2);
        atomicWrite(staged, "staging fixture"); progressMode = 2;
        waitForStatus(app, video, 16, "installation complete");
        assert(app.ui.queued.empty() && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");
    }
    std::filesystem::remove_all(directory);
    std::puts("Settings persistence and tracked installation progress, failed-task recovery, retry and staging cleanup passed");
}
