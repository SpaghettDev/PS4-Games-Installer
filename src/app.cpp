#include "app.hpp"
#include "video.hpp"
#include "platform.hpp"
#include "download.hpp"
#include "cJSON.h"
#include <chrono>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
enum class ResultKind { Local, Remote, Test, Storage, Install, Uninstall, Poll };
struct Result {
    ResultKind kind = ResultKind::Test;
    std::optional<std::vector<Entry>> local, remote;
    Entry entry;
    int task = -1;
    uint64_t freeBytes = 0;
    std::optional<InstallStorage> storage;
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
uint64_t timestamp() { const auto now = std::time(nullptr); return now > 0 ? uint64_t(now) : 0; }
}
struct App::Worker {
    TaskThread thread;
    std::mutex mutex;
    bool running = false;
    std::optional<Result> result;
    std::string update;
    std::atomic<bool> cancelled{false};
    ~Worker() { cancelled = true; if (thread.joinable()) thread.join(); }
    bool busy() { std::lock_guard<std::mutex> lock(mutex); return running || result.has_value(); }
    bool start(std::function<Result()> job) {
        if (busy()) return false;
        if (thread.joinable()) thread.join();
        { std::lock_guard<std::mutex> lock(mutex); running = true; update.clear(); }
        try { thread = TaskThread([this, job] {
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
    void report(const std::string& message) { std::lock_guard<std::mutex> lock(mutex); update = message; }
    std::string takeUpdate() { std::lock_guard<std::mutex> lock(mutex); std::string message; message.swap(update); return message; }
};

App::App(const std::string& data, bool preview)
    : worker(new Worker), directory(ensureDirectory(data)), demo(preview), artwork(directory + "/artwork") {
    try { ui.settings = loadSettings(directory); }
    catch (const std::exception& e) { ui.status = std::string("Settings: ") + e.what(); }
    saved = ui.settings; ui.draft = saved;
    try { ui.player = loadPlayerData(directory); }
    catch (const std::exception& e) { playerWritable = false; log(std::string("Local records kept unchanged: ") + e.what()); }
    try { resume = loadViewState(directory); }
    catch (const std::exception& e) { log(std::string("Saved position: ") + e.what()); }
    if (demo) {
        auto entries = demoLibrary();
        std::vector<Entry> local;
        for (size_t i = 0; i < std::min(size_t(6), entries.size()); ++i) {
            auto e = entries[i]; e.installed = true; e.installedSize = e.size + 1500000000ULL; e.url.clear(); local.push_back(e);
        }
        ui.player.observeInstalled(local, 1792108800);
        if (!local.empty()) ui.player.rate(local[0].titleId, 5);
        if (entries.size() > 8) { ui.player.observeInstalled({entries[8]}, 1792108800); ui.player.rate(entries[8].titleId, 4); }
        ui.setLibraries(local, entries);
        ui.freeBytes = 423000000000ULL;
        ui.storage.internal = {true, ui.freeBytes}; ui.storage.external = {true, 1200000000000ULL};
        ui.storage.destination = InstallDestination::Internal;
        ui.storageChecked = true;
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
    std::lock_guard<std::mutex> lock(logMutex);
    std::string path = directory + "/app.log";
    struct stat st{};
    bool truncate = stat(path.c_str(), &st) == 0 && st.st_size > 256 * 1024;
    FILE* file = std::fopen(path.c_str(), truncate ? "wb" : "ab");
    if (file) { std::fprintf(file, "%s\n", message.c_str()); std::fclose(file); }
}
void App::savePosition() { if (!demo) saveViewState(directory, ui.captureView()); }
void App::savePlayer(const PlayerData& next) {
    if (!playerWritable) throw std::runtime_error("Local player.json needs repair; the original file was kept unchanged");
    if (!demo) savePlayerData(directory, next);
    ui.player = next; ui.rebuild();
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
            result.storage = scanInstallStorage();
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
    if (auto message = worker->takeUpdate(); !message.empty()) { ui.status = message; log(message); }
    if (auto result = worker->take()) {
        ui.loading = false;
        ui.status = result->message;
        if (initialScan) { initialScan = false; bootReady = true; }
        if (result->local) { lastLocalScan = elapsed; ui.freeBytes = result->freeBytes; }
        if (result->storage) { ui.storage = *result->storage; ui.storageChecked = true; }
        if (result->local && playerWritable) {
            auto next = ui.player;
            if (next.observeInstalled(*result->local, timestamp())) {
                try { savePlayer(next); }
                catch (const std::exception& e) { log(std::string("Save installation history: ") + e.what()); ui.status += " (history could not be saved)"; }
            }
        }
        const bool receivedLocal = result->local.has_value();
        if (result->local || result->remote) ui.setLibraries(std::move(result->local), std::move(result->remote));
        if (resume && (resume->available ? !ui.available.empty() : receivedLocal)) { ui.restoreView(*resume); resume.reset(); }
        if (result->kind == ResultKind::Install && result->task >= 0) {
            tasks[result->entry.url] = result->task; ui.queued.insert(result->entry.url);
            ui.rebuild();
            ui.status = "Installation queued. Manage it in PS4 Notifications > Downloads.";
            try { saveTasks(); } catch (const std::exception&) { ui.status += " Task tracking could not be saved."; }
        }
        if (result->kind == ResultKind::Poll) {
            bool changed = false;
            for (const auto& item : result->progress) {
                if (item.second.error || item.second.completed || !item.second.found) {
                    if (item.second.error) ui.status = "Install error " + errorCode(item.second.error) +
                        (item.second.retired ? "; failed task removed, installation can be retried." : "; cancel the failed task in PS4 Downloads before retrying.");
                    else ui.status = item.second.completed ? "PS4 installation complete." : "Task finished or removed. See PS4 Downloads for its final status.";
                    if (!item.second.found || item.second.retired) {
                        removeStagedPackage(directory, item.first);
                        tasks.erase(item.first); ui.queued.erase(item.first); changed = true;
                    }
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
    if (bootReady && ui.screen == Screen::Splash) ui.screen = Screen::Dashboard;
    Command command;
    while (ui.take(command)) {
        try {
            switch (command.type) {
                case CommandType::Quit:
                    try { savePosition(); } catch (const std::exception& e) { log(e.what()); }
                    quitting = true; worker->cancelled = true; break;
                case CommandType::Keyboard:
                    try { video.startKeyboard(ui); }
                    catch (...) { ui.finishEdit({}); throw; }
                    break;
                case CommandType::Save: {
                    Settings next = command.configuration.value();
                    bool changedSource = next.sources.size() != saved.sources.size();
                    for (size_t i = 0; !changedSource && i < next.sources.size(); ++i)
                        changedSource = next.sources[i].url != saved.sources[i].url || next.sources[i].category != saved.sources[i].category;
                    try { saveSettings(directory, next); }
                    catch (const std::exception& e) {
                        ui.settings = saved; ui.rebuild();
                        ui.settingsNotice = std::string("Save failed: ") + e.what();
                        throw;
                    }
                    ui.settings = saved = next; ui.draft = saved; ui.rebuild();
                    ui.settingsNotice = "Saved to " + directory + "/config.json";
                    ui.status = "Settings saved";
                    log(ui.settingsNotice);
                    if (changedSource) { startRemote = true; if (!worker->busy()) { startRemote = false; refresh(true); } }
                    break;
                }
                case CommandType::Refresh: startRemote = true; refresh(false); break;
                case CommandType::Storage:
                case CommandType::SetStorage: {
                    if (demo) { ui.status = "Desktop preview — sample storage capacities"; break; }
                    const bool apply = command.type == CommandType::SetStorage;
                    const auto destination = command.source ? InstallDestination::External : InstallDestination::Internal;
                    if (!worker->start([apply, destination] {
                        if (apply) setInstallDestination(destination);
                        Result r; r.kind = ResultKind::Storage; r.storage = scanInstallStorage();
                        r.message = !r.storage->error.empty() ? r.storage->error : apply ?
                            std::string("Installation destination: ") + (destination == InstallDestination::External ? "External USB" : "Internal storage") : "Installation storage checked";
                        return r;
                    })) throw std::runtime_error("Wait for the current operation, then press Triangle to check storage");
                    ui.status = "Checking installation storage..."; ui.loading = true; break;
                }
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
                case CommandType::Rate: {
                    auto next = ui.player; next.rate(command.entry.titleId, command.source); savePlayer(next);
                    ui.status = command.source ? "Local rating saved: " + std::to_string(command.source) + "/5" : "Local rating removed";
                    log(ui.status); break;
                }
                case CommandType::Uninstall: {
                    if (demo) { ui.status = "Desktop preview cannot uninstall PS4 games"; break; }
                    for (const auto& local : ui.installed) if (local.titleId == command.entry.titleId && local.queued)
                        throw std::runtime_error("Finish or cancel this title's queued download before uninstalling");
                    if (worker->busy()) throw std::runtime_error("Wait for the current operation before uninstalling");
                    savePosition(); auto next = ui.player; next.observeInstalled({command.entry}, timestamp()); savePlayer(next);
                    const auto entry = command.entry;
                    worker->start([entry] {
                        uninstallTitle(entry.titleId);
                        Result r; r.kind = ResultKind::Uninstall; r.entry = entry;
                        r.message = "Uninstalled: " + entry.name + ". Local rating and history kept.";
                        try { r.local = scanInstalled(); r.freeBytes = freeSpace("/user"); r.storage = scanInstallStorage(); }
                        catch (const std::exception& e) { r.message += " Refresh: " + std::string(e.what()); }
                        return r;
                    });
                    ui.status = "Uninstalling " + entry.name + "..."; ui.loading = true; log(ui.status); break;
                }
                case CommandType::Launch: {
                    if (demo) ui.status = "Desktop preview cannot launch PS4 games";
                    else {
                        if (ui.loading) throw std::runtime_error("Wait for the current operation before launching");
                        savePosition(); // The PS4 may terminate us inside the launch call.
                        const auto previous = ui.player;
                        bool recorded = false;
                        if (playerWritable) {
                            try { auto next = previous; next.noteLaunch(command.entry.titleId, timestamp()); savePlayer(next); recorded = true; }
                            catch (const std::exception& e) { log(std::string("Save launch record: ") + e.what()); }
                        }
                        log("Launch requested: " + command.entry.titleId + "; library position saved");
                        try { launchTitle(command.entry.titleId); }
                        catch (...) {
                            if (recorded) { try { savePlayer(previous); } catch (const std::exception& e) { log(e.what()); } }
                            throw;
                        }
                        ui.status = "Launch requested: " + command.entry.name; lastLocalScan = -30;
                    }
                    break;
                }
                case CommandType::Install: {
                    if (demo) { ui.status = "Desktop preview cannot install PS4 packages"; break; }
                    if (tasks.count(command.entry.url)) throw std::runtime_error("This package is already queued");
                    savePosition(); log("Install requested: " + command.entry.titleId);
                    Entry entry = command.entry;
                    int user = video.userId;
                    if (!worker->start([this, entry, user] {
                        Result r; r.kind = ResultKind::Install; r.entry = entry;
                        int lastPercent = -1;
                        r.task = installPackage(entry, user, directory, &worker->cancelled, [&](uint64_t bytes, uint64_t total) {
                            const int percent = total ? std::min(100.0, double(bytes) / total * 100.0) : 0;
                            if (lastPercent != percent) {
                                worker->report("Server has no range support. Direct download: " + std::to_string(percent) + "% — keep this app open.");
                                lastPercent = percent;
                            }
                        }, [this](const std::string& message) { log(message); worker->report(message); });
                        return r;
                    })) throw std::runtime_error("Wait for the current operation, then retry installation");
                    ui.status = "Checking package URL and server range support..."; ui.loading = true; break;
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
                    if (progress.found && (progress.error || progress.completed)) progress.retired = retireInactiveTask(item.second);
                    completed = completed || !progress.found || progress.error || progress.completed || (progress.total && progress.transferred >= progress.total);
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
