#include "app.hpp"
#include "platform.hpp"
#include "download.hpp"
#include "video.hpp"
#include <algorithm>
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
std::atomic<bool> rejectInstall{false};
std::atomic<int> submissions{0}, progressMode{0};
std::vector<psgi::Entry> localGames;
std::string launchDirectory;
bool allowLaunch = false, failLaunch = false, allowUninstall = false;
int launchCalls = 0, uninstallCalls = 0;
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
std::vector<Entry> scanInstalled() { std::lock_guard<std::mutex> lock(requestMutex); return localGames; }
std::vector<Entry> demoLibrary() { return {}; }
uint64_t freeSpace(const std::string&) { return 0; }
void launchTitle(const std::string& id) {
    if (!allowLaunch) throw std::runtime_error("PS4 required");
    const auto saved = loadViewState(launchDirectory);
    assert(saved.cursor[saved.available ? 1 : 0].titleId == id); // Must exist before shell handoff.
    assert(loadPlayerData(launchDirectory).games.at(id).launches == (failLaunch ? 2 : 1));
    ++launchCalls;
    if (failLaunch) throw std::runtime_error("Launch fixture failed");
}
void uninstallTitle(const std::string& id) {
    assert(allowUninstall && loadPlayerData(launchDirectory).games.at(id).wasInstalled);
    std::lock_guard<std::mutex> lock(requestMutex); ++uninstallCalls;
    localGames.erase(std::remove_if(localGames.begin(), localGames.end(), [&](const Entry& entry) { return entry.titleId == id; }), localGames.end());
}
int queueInstall(const Entry&, int) {
    if (!allowInstalls) throw std::runtime_error("PS4 required");
    if (rejectInstall) throw std::runtime_error("PS4 refused installation (0x80990086). No matching task could be confirmed. Downloaded package kept.");
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
void settleAt(App& app, Video& video, int line) {
    for (int i = 0; i < 200; ++i) {
        app.tick(0, video);
        if (!app.ui.loading && app.ui.screen != Screen::Splash) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::fprintf(stderr, "Worker timeout at test line %d: screen=%d loading=%d status=%s\n", line, int(app.ui.screen), int(app.ui.loading), app.ui.status.c_str());
    assert(false && "Application worker did not finish");
}
#define settle(app, video) settleAt(app, video, __LINE__)
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
void action(Ui& ui, const std::string& label) {
    ui.screen = Screen::Dashboard; ui.handle(Button::Triangle);
    const auto rows = ui.actionRows();
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const std::string& row) { return row.find(label) == 0; });
    assert(found != rows.end()); ui.menuIndex = found - rows.begin(); ui.handle(Button::Cross);
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

        assert(app.ui.screen == Screen::InstallError);
        app.ui.handle(Button::Circle); app.tick(8, video);
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty());
        app.ui.status = "Awaiting repeated task failure";
        waitForStatus(app, video, 11, "cancel the failed task");
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty()); // Acknowledged errors do not reopen on every poll.
        allowRetirement = true;
        waitForStatus(app, video, 15, "failed task removed");
        assert(app.ui.queued.empty() && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");

        // Retry becomes possible only after the owned task was actually retired.
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross);
        app.tick(15, video); settle(app, video); assert(submissions == 2);
        atomicWrite(staged, "staging fixture"); progressMode = 2;
        waitForStatus(app, video, 19, "installation complete");
        assert(app.ui.queued.empty() && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");
    }
    const auto refusal = directory + "/refusal"; std::filesystem::create_directory(refusal);
    Settings offline; offline.refreshOnStart = false; saveSettings(refusal, offline);
    atomicWrite(refusal + "/catalog.json", R"({"DATA":{"http://fixture/game.pkg":{"name":"Rejected package","title_id":"CUSA00001"}}})");
    {
        App app(refusal); settle(app, video); app.ui.handle(Button::R1); rejectInstall = true;
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross); app.tick(0, video);
        app.ui.screen = Screen::Keyboard; settle(app, video);
        assert(app.ui.screen == Screen::Keyboard && app.ui.installError.find("0x80990086") != std::string::npos);
        app.ui.screen = Screen::Dashboard; app.tick(0, video);
        assert(app.ui.screen == Screen::InstallError && app.ui.failedInstall.name == "Rejected package");
        assert(app.ui.queued.empty() && !std::filesystem::exists(refusal + "/tasks.json"));
        // Routine discovery must not dismiss the installation error.
        app.tick(31, video); settle(app, video);
        assert(app.ui.screen == Screen::InstallError && app.ui.installError.find("0x80990086") != std::string::npos);
        app.ui.handle(Button::Cross); assert(app.ui.screen == Screen::ConfirmInstall);
        rejectInstall = false; app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty() && !app.ui.queued.empty());
    }
    const auto personal = directory + "/personal"; std::filesystem::create_directory(personal);
    Settings personalSettings; personalSettings.refreshOnStart = false; saveSettings(personal, personalSettings);
    auto games = parseCatalog(R"({"DATA":{"http://fixture/a.pkg":{"name":"Alpha","title_id":"CUSA00001"},"http://fixture/b.pkg":{"name":"Bravo","title_id":"CUSA00002"}}})", "games");
    atomicWrite(personal + "/catalog.json", serializeCatalog(games));
    { std::lock_guard<std::mutex> lock(requestMutex); localGames = games; for (auto& entry : localGames) { entry.installed = true; entry.url.clear(); } }
    launchDirectory = personal; allowLaunch = true;
    {
        App app(personal); settle(app, video); app.ui.selected[0] = 1;
        assert(app.ui.current()->titleId == "CUSA00002");
        app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 1 && loadPlayerData(personal).games.at("CUSA00002").launches == 1);
    }
    { std::lock_guard<std::mutex> lock(requestMutex); localGames[1].name = "Aardvark"; }
    {
        App app(personal); settle(app, video);
        assert(app.ui.library == Library::Installed && app.ui.position() == 0 && app.ui.current()->titleId == "CUSA00002");
        // Rating goes through the action menu and is durable independently of the catalog.
        action(app.ui, "Rate game"); assert(app.ui.screen == Screen::Rating);
        app.ui.ratingIndex = 3; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.current()->rating == 4 && loadPlayerData(personal).games.at("CUSA00002").rating == 4);
        failLaunch = true;
        app.ui.screen = Screen::Dashboard; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 2 && app.ui.status == "Launch fixture failed");
        assert(loadPlayerData(personal).games.at("CUSA00002").launches == 1);
        failLaunch = false;
        std::filesystem::create_directory(personal + "/session.json.tmp");
        app.ui.screen = Screen::Dashboard; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 2 && app.ui.status.find("Cannot write") != std::string::npos);
        std::filesystem::remove(personal + "/session.json.tmp");

        action(app.ui, "Uninstall"); assert(app.ui.screen == Screen::ConfirmUninstall);
        app.ui.handle(Button::Circle); app.tick(0, video); assert(uninstallCalls == 0);
        action(app.ui, "Uninstall"); allowUninstall = true;
        app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(uninstallCalls == 1 && app.ui.installed.size() == 1);
        const auto removed = std::find_if(app.ui.available.begin(), app.ui.available.end(), [](const Entry& entry) { return entry.titleId == "CUSA00002"; });
        assert(removed != app.ui.available.end() && !removed->installed && removed->wasInstalled && removed->rating == 4);
        const auto records = loadPlayerData(personal);
        assert(records.games.at("CUSA00002").wasInstalled && records.games.at("CUSA00002").rating == 4);
        app.ui.handle(Button::R1); assert(app.ui.librarySummary().find("available to install") != std::string::npos);
    }
    {
        App app(personal); settle(app, video);
        assert(app.ui.player.games.at("CUSA00002").rating == 4 && app.ui.player.games.at("CUSA00002").wasInstalled);
    }
    const auto corrupt = directory + "/corrupt"; std::filesystem::create_directory(corrupt);
    saveSettings(corrupt, personalSettings); atomicWrite(corrupt + "/player.json", "broken personal records");
    {
        App app(corrupt); settle(app, video); action(app.ui, "Rate game");
        app.ui.ratingIndex = 2; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.status.find("original file was kept") != std::string::npos);
        assert(readFile(corrupt + "/player.json") == "broken personal records");
    }
    std::filesystem::remove_all(directory);
    std::puts("Settings persistence and tracked installation progress, failed-task recovery, retry and staging cleanup passed");
    std::puts("Launch handoff/session restart, rating persistence, uninstall confirmation/history and corrupt-record protection passed");
    std::puts("Registration refusal, persistent error/retry, IME deferral and acknowledged-task-error suppression passed");
}
