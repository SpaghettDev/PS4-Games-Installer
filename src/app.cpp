#include "app.hpp"
#include "video.hpp"
#include "platform.hpp"
#include "cJSON.h"
#include <chrono>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
enum class ResultKind { Local, Remote, Test, Install, Poll };
struct Result {
    ResultKind kind = ResultKind::Test;
    std::optional<std::vector<Entry>> local, remote;
    Entry entry;
    int task = -1;
    uint64_t freeBytes = 0;
    std::string message;
    std::map<std::string, TaskProgress> progress;
};
constexpr size_t CacheLimit = 64 * 1024 * 1024;
std::string ensureDirectory(const std::string& path) {
    if (mkdir(path.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create application data directory");
    return path;
}
std::string errorCode(int result) {
    char code[20]; std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(result)); return code;
}
}
struct App::Worker {
    std::thread thread;
    std::mutex mutex;
    bool running = false;
    std::optional<Result> result;
    std::atomic<bool> cancelled{false};
    ~Worker() { cancelled = true; if (thread.joinable()) thread.join(); }
    bool busy() { std::lock_guard<std::mutex> lock(mutex); return running || result.has_value(); }
    bool start(std::function<Result()> job) {
        if (busy()) return false;
        if (thread.joinable()) thread.join();
        { std::lock_guard<std::mutex> lock(mutex); running = true; }
        try { thread = std::thread([this, job] {
            Result out;
            try { out = job(); } catch (const std::exception& e) { out.message = e.what(); }
            std::lock_guard<std::mutex> lock(mutex);
            result = std::move(out); running = false;
        }); } catch (...) { std::lock_guard<std::mutex> lock(mutex); running = false; throw; }
        return true;
    }
    std::optional<Result> take() {
        std::lock_guard<std::mutex> lock(mutex);
        auto out = std::move(result); result.reset(); return out;
    }
};

App::App(const std::string& data, bool preview)
    : worker(new Worker), directory(ensureDirectory(data)), demo(preview), artwork(directory + "/artwork") {
    try { ui.settings = loadSettings(directory, preview ? "" : "/data/FPKGi-openorbis/config.json"); }
    catch (const std::exception& e) { ui.status = std::string("Settings: ") + e.what(); }
    saved = ui.settings; ui.draft = saved;
    if (demo) {
        auto entries = demoLibrary();
        std::vector<Entry> local;
        for (size_t i = 0; i < std::min(size_t(6), entries.size()); ++i) {
            auto e = entries[i]; e.installed = true; e.installedSize = e.size + 1500000000ULL; e.url.clear(); local.push_back(e);
        }
        ui.setLibraries(local, entries);
        ui.freeBytes = 423000000000ULL;
        ui.status = "Desktop preview — sample library; PS4 operations are disabled";
        ui.loading = false; bootReady = true; initialScan = false;
        return;
    }
    try { ui.available = parseCatalog(readFile(directory + "/catalog.json", CacheLimit), "", CacheLimit); }
    catch (const std::exception&) { /* A missing/corrupt offline cache is not fatal. */ }
    try {
        auto text = readFile(directory + "/tasks.json");
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_ParseWithLengthOpts(text.c_str(), text.size()+1, nullptr, true), cJSON_Delete);
        if (cJSON_IsObject(root.get())) for (auto* item = root->child; item; item = item->next)
            if (isHttpUrl(item->string) && cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 2147483647 && item->valuedouble == item->valueint) {
                tasks[item->string] = item->valueint; ui.queued.insert(item->string);
            }
    } catch (const std::exception&) {}
    ui.rebuild();
    startRemote = saved.refreshOnStart;
    refresh(false);
}
App::~App() { worker.reset(); }
void App::log(const std::string& message) {
    std::string path = directory + "/app.log";
    struct stat st{};
    bool truncate = stat(path.c_str(), &st) == 0 && st.st_size > 256 * 1024;
    FILE* file = std::fopen(path.c_str(), truncate ? "wb" : "ab");
    if (file) { std::fprintf(file, "%s\n", message.c_str()); std::fclose(file); }
}
void App::saveTasks() {
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    for (const auto& item : tasks) cJSON_AddNumberToObject(root.get(), item.first.c_str(), item.second);
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(root.get()), cJSON_free);
    if (text) atomicWrite(directory + "/tasks.json", text.get());
}
void App::refresh(bool remote) {
    if (worker->busy()) { ui.status = "A background operation is running. Please try again shortly."; return; }
    if (demo) { ui.status = "Desktop preview library refreshed"; return; }
    Settings settings = ui.settings;
    bool started = worker->start([this, remote, settings] {
        Result result;
        result.kind = remote ? ResultKind::Remote : ResultKind::Local;
        if (!remote) {
            result.local = scanInstalled(); result.freeBytes = freeSpace("/user");
            result.message = std::to_string(result.local->size()) + " installed games found";
            return result;
        }
        std::vector<Entry> combined;
        size_t count = 0;
        for (const auto& source : settings.sources) if (!source.url.empty()) {
            if (worker->cancelled) throw std::runtime_error("Refresh cancelled");
            try { mergeCatalog(combined, parseCatalog(httpGet(source.url, MaxCatalogBytes, &worker->cancelled), source.category)); }
            catch (const std::exception& e) { throw std::runtime_error(source.category + ": " + e.what() + ". Keeping the last valid library."); }
            ++count;
        }
        if (!count) throw std::runtime_error("Open Options > Settings and enter your Games JSON URL.");
        result.message = std::to_string(combined.size()) + " remote packages loaded";
        std::string cached = serializeCatalog(combined);
        if (cached.size() <= CacheLimit) {
            try { atomicWrite(directory + "/catalog.json", cached); }
            catch (const std::exception&) { result.message += " (offline cache could not be saved)"; }
        } else result.message += " (library exceeds offline cache limit)";
        result.remote = std::move(combined);
        return result;
    });
    if (started) { ui.loading = true; ui.status = remote ? "Refreshing remote library..." : "Reading installed games..."; }
}
void App::tick(double elapsed, Video& video) {
    if (auto result = worker->take()) {
        ui.loading = false;
        ui.status = result->message;
        if (initialScan) { initialScan = false; bootReady = true; }
        if (result->local) { lastLocalScan = elapsed; ui.freeBytes = result->freeBytes; }
        if (result->local || result->remote) ui.setLibraries(std::move(result->local), std::move(result->remote));
        if (result->kind == ResultKind::Install && result->task >= 0) {
            tasks[result->entry.url] = result->task; ui.queued.insert(result->entry.url);
            ui.rebuild();
            ui.status = "Installation queued. Manage it in PS4 Notifications > Downloads.";
            try { saveTasks(); } catch (const std::exception&) { ui.status += " Task tracking could not be saved."; }
        }
        if (result->kind == ResultKind::Poll) {
            bool changed = false;
            for (const auto& item : result->progress) {
                if (item.second.error || !item.second.found) {
                    if (item.second.error) ui.status = "Install error " + errorCode(item.second.error) + "; see PS4 Downloads.";
                    else ui.status = "Task finished or removed. See PS4 Downloads for its final status.";
                    tasks.erase(item.first); ui.queued.erase(item.first); changed = true;
                } else if (item.second.total) {
                    unsigned percent = std::min(100.0, double(item.second.transferred) / item.second.total * 100.0);
                    ui.status = percent >= 100 ? "Transfer complete; the PS4 may still be installing. See Downloads." : "Background package transfer: " + std::to_string(percent) + "%";
                } else ui.status = "Background installer is preparing a package...";
            }
            if (changed) { ui.rebuild(); try { saveTasks(); } catch (const std::exception&) {} }
        }
        if (!ui.status.empty()) log(ui.status);
        if (startRemote && !worker->busy()) { startRemote = false; refresh(true); }
    }
    if (bootReady && elapsed >= 1.2 && ui.screen == Screen::Splash) ui.screen = Screen::Dashboard;
    Command command;
    while (ui.take(command)) {
        try {
            switch (command.type) {
                case CommandType::Quit: quitting = true; worker->cancelled = true; break;
                case CommandType::Keyboard:
                    try { video.startKeyboard(ui); }
                    catch (...) { ui.finishEdit({}); throw; }
                    break;
                case CommandType::Save: {
                    Settings next = ui.screen == Screen::Settings ? ui.draft : ui.settings;
                    bool changedSource = next.sources.size() != saved.sources.size();
                    for (size_t i = 0; !changedSource && i < next.sources.size(); ++i)
                        changedSource = next.sources[i].url != saved.sources[i].url || next.sources[i].category != saved.sources[i].category;
                    try { saveSettings(directory, next); }
                    catch (...) { ui.settings = saved; ui.rebuild(); throw; }
                    ui.settings = saved = next; ui.draft = saved; ui.rebuild();
                    if (ui.screen == Screen::Settings) ui.screen = Screen::Options;
                    ui.status = "Settings saved";
                    if (changedSource) { startRemote = true; if (!worker->busy()) { startRemote = false; refresh(true); } }
                    break;
                }
                case CommandType::Refresh: startRemote = true; refresh(false); break;
                case CommandType::Test: {
                    Settings next = ui.draft;
                    if (!worker->start([this, next] {
                        size_t count = 0, sources = 0;
                        for (const auto& source : next.sources) if (!source.url.empty()) {
                            if (worker->cancelled) throw std::runtime_error("Connection test cancelled");
                            count += parseCatalog(httpGet(source.url, MaxCatalogBytes, &worker->cancelled), source.category).size(); ++sources;
                        }
                        if (!sources) throw std::runtime_error("Enter a Games JSON URL before testing");
                        Result r; r.message = "Connection OK: " + std::to_string(count) + " packages in " + std::to_string(sources) + " catalog(s)"; return r;
                    })) throw std::runtime_error("Wait for the current operation, then test again");
                    ui.status = "Testing catalog connection..."; ui.loading = true; break;
                }
                case CommandType::ClearArtwork: artwork.clear(); ui.status = "Artwork cache cleared"; break;
                case CommandType::RefreshArtwork: artwork.refresh(command.entry); ui.status = "Artwork will be reloaded"; break;
                case CommandType::Launch:
                    if (demo) ui.status = "Desktop preview cannot launch PS4 games";
                    else { launchTitle(command.entry.titleId); ui.status = "Launch requested: " + command.entry.name; lastLocalScan = -30; }
                    break;
                case CommandType::Install: {
                    if (demo) { ui.status = "Desktop preview cannot install PS4 packages"; break; }
                    if (tasks.count(command.entry.url)) throw std::runtime_error("This package is already queued");
                    Entry entry = command.entry;
                    int user = video.userId;
                    if (!worker->start([entry, user] {
                        Result r; r.kind = ResultKind::Install; r.entry = entry; r.task = queueInstall(entry, user); return r;
                    })) throw std::runtime_error("Wait for the current operation, then retry installation");
                    ui.status = "Submitting package to the PS4 background installer..."; ui.loading = true; break;
                }
            }
        } catch (const std::exception& e) { ui.status = e.what(); log(ui.status); }
    }
    if (!demo && !quitting && bootReady && !worker->busy()) {
        if (!tasks.empty() && elapsed - lastPoll >= 3) {
            auto tracked = tasks;
            bool rescan = elapsed - lastLocalScan >= 15;
            worker->start([tracked, rescan] {
                Result r; r.kind = ResultKind::Poll;
                bool completed = false;
                for (const auto& item : tracked) {
                    auto progress = taskProgress(item.second);
                    completed = completed || !progress.found || progress.error || (progress.total && progress.transferred >= progress.total);
                    r.progress[item.first] = progress;
                }
                if (rescan || completed) { r.local = scanInstalled(); r.freeBytes = freeSpace("/user"); }
                return r;
            });
            lastPoll = elapsed;
        } else if (elapsed - lastLocalScan >= 30) refresh(false);
    }
}
}
