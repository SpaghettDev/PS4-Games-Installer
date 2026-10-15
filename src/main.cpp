#include "catalog.hpp"
#include "display.hpp"
#include "platform.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

namespace {
using namespace psgi;
const char* ConfigPath = "/data/PS4-Games-Installer/config.json";
constexpr uint32_t Background = 0x0d1522, Panel = 0x152236, Accent = 0x4793ff,
                   Foreground = 0xeaf0f8, Muted = 0x9baec7;
constexpr size_t Rows = 10;

// One worker owns HTTP and BGFT. Only completed results cross back to the UI.
struct Result {
    bool refresh = false;
    std::vector<Entry> catalog;
    std::vector<std::string> categories;
    std::string message;
    int task = -1;
};
class Worker {
    std::mutex mutex;
    std::thread thread;
    bool running = false, done = false;
    Result result;
public:
    std::atomic<bool> cancelled{false};
    ~Worker() { cancelled = true; if (thread.joinable()) thread.join(); }
    bool busy() { std::lock_guard<std::mutex> lock(mutex); return running || done; }
    void start(std::function<Result()> job) {
        if (thread.joinable()) thread.join();
        cancelled = false;
        { std::lock_guard<std::mutex> lock(mutex); running = true; }
        try {
            thread = std::thread([this, job]() {
                Result r;
                try { r = job(); }
                catch (const std::exception& e) { r.message = e.what(); }
                std::lock_guard<std::mutex> lock(mutex);
                result = std::move(r); running = false; done = true;
            });
        } catch (...) { std::lock_guard<std::mutex> lock(mutex); running = false; throw; }
    }
    bool take(Result& out) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!done) return false;
        out = std::move(result); done = false;
        return true;
    }
};

Result loadCatalog(const std::atomic<bool>& cancelled) {
    auto sources = parseConfig(readFile(ConfigPath));
    if (sources.empty()) throw std::runtime_error("Set content_urls in /data/PS4-Games-Installer/config.json, then press Triangle");
    Result r;
    r.refresh = true;
    r.categories.push_back("");
    // Refresh is transactional: a bad source keeps the previous on-screen catalog.
    for (const auto& source : sources) {
        if (cancelled) throw std::runtime_error("Catalog refresh cancelled");
        try { mergeCatalog(r.catalog, parseCatalog(httpGet(source.url), source.category)); }
        catch (const std::exception& e) { throw std::runtime_error(source.category + ": " + e.what()); }
        r.categories.push_back(source.category);
    }
    r.message = std::to_string(r.catalog.size()) + " packages loaded";
    return r;
}

void createConfig() {
    if (mkdir("/data/PS4-Games-Installer", 0777) != 0 && access("/data/PS4-Games-Installer", W_OK) != 0)
        throw std::runtime_error("Cannot create /data/PS4-Games-Installer");
    if (access(ConfigPath, F_OK) == 0) return;
    FILE* f = std::fopen(ConfigPath, "wb");
    if (!f) throw std::runtime_error("Cannot create config.json");
    const char* config = "{\n  \"content_urls\": {\n    \"games\": null,\n    \"homebrew\": null\n  }\n}\n";
    int written = std::fputs(config, f);
    int closed = std::fclose(f);
    if (written < 0 || closed) throw std::runtime_error("Cannot write config.json");
}

void draw(Display& d, const std::vector<Entry>& catalog, const std::vector<size_t>& visible,
          const std::vector<std::string>& categories, size_t category, size_t selected,
          bool sizeSort, bool busy, const std::string& status, const Entry* pending) {
    d.rectangle(0, 0, 1920, 1080, Background);
    d.rectangle(60, 56, 6, 66, Accent);
    d.text("PS4 Games Installer", 88, 106, 48, Foreground);
    d.text("Your remote library", 310, 104, 28, Muted);
    d.text(std::to_string(visible.size()) + " packages", 1560, 102, 28, Muted, 280);
    d.rectangle(60, 156, 1800, 2, Panel);
    d.text("LIBRARY", 80, 218, 24, Muted);
    const size_t categoryStart = category < 13 ? 0 : category - 12;
    for (size_t i = categoryStart; i < categories.size() && i < categoryStart + 13; ++i) {
        int y = 270 + (i - categoryStart) * 48;
        if (i == category) d.rectangle(60, y - 32, 256, 46, Panel);
        d.text(categories[i].empty() ? "All packages" : categories[i], 82, y, 26,
               i == category ? Accent : Muted, 220);
    }
    const std::string categoryName = categories[category].empty() ? "All packages" : categories[category];
    d.text(categoryName, 354, 218, 34, Foreground, 900);
    d.text(sizeSort ? "Largest first" : "Name A-Z", 1610, 216, 24, Muted, 220);
    if (visible.empty()) {
        d.text(busy ? "Loading your library..." : "No packages to show", 390, 365, 36, Foreground, 1300);
        d.text("Edit /data/PS4-Games-Installer/config.json over FTP.", 390, 430, 27, Muted, 1350);
        d.text("Add your catalog URL under content_urls.games, then press Triangle.", 390, 478, 27, Muted, 1350);
    }
    size_t page = selected / Rows * Rows;
    for (size_t i = page; i < std::min(page + Rows, visible.size()); ++i) {
        const auto& e = catalog[visible[i]];
        int y = 264 + (i - page) * 62;
        if (i == selected) {
            d.rectangle(344, y - 8, 1516, 60, Panel);
            d.rectangle(344, y - 8, 4, 60, Accent);
        }
        d.text(e.name, 368, y + 20, 29, i == selected ? Foreground : Muted, 1120);
        d.text(e.titleId + (e.region.empty() ? "" : "  " + e.region) +
               (e.version.empty() ? "" : "  v" + e.version), 369, y + 44, 19, Muted, 1120);
        d.text(formatSize(e.size), 1600, y + 22, 24, Muted, 235);
    }
    if (!visible.empty()) d.text(std::to_string(selected + 1) + " / " + std::to_string(visible.size()), 1640, 934, 22, Muted, 200);
    d.rectangle(60, 961, 1800, 2, Panel);
    d.text(status, 80, 1004, 24, busy ? Accent : Foreground, 1760);
    d.text("D-pad Browse / Page    L1/R1 Category    X Install    Square Sort    Triangle Refresh    Options Exit",
           80, 1048, 23, Muted, 1760);
    if (pending) {
        d.rectangle(300, 290, 1320, 470, 0x22354e);
        d.rectangle(300, 290, 1320, 5, Accent);
        d.text("Install package?", 350, 360, 38, Foreground, 1220);
        d.text(pending->name, 350, 430, 32, Foreground, 1220);
        d.text(pending->titleId + "  " + pending->region + "  " + formatSize(pending->size), 350, 480, 26, Muted, 1220);
        d.text(pending->url, 350, 530, 21, Muted, 1220);
        d.text("The PS4 will download and install this package in the background.", 350, 605, 26, Foreground, 1220);
        d.text("X Confirm install       Circle Cancel", 350, 704, 28, Accent, 1220);
    }
    d.present();
}
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        psgi::Display display;
        std::string status;
        bool privileges = false;
        // Keep setup failures visible on screen instead of entering a silent loop.
        try { psgi::prepareConsole(); privileges = true; createConfig(); }
        catch (const std::exception& e) { status = e.what(); }
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD);
        OrbisUserServiceInitializeParams param{};
        param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&param);
        int user = -1;
        if (sceUserServiceGetInitialUser(&user) < 0 || scePadInit() < 0)
            throw std::runtime_error("Controller initialization failed");
        int pad = scePadOpen(user, 0, 0, nullptr);
        if (pad < 0) throw std::runtime_error("Controller could not be opened");
        Worker worker;
        std::vector<psgi::Entry> catalog;
        std::vector<std::string> categories{ "" };
        std::vector<size_t> visible;
        size_t category = 0, selected = 0;
        bool sizeSort = false, confirm = false;
        psgi::Entry pending;
        uint32_t previous = 0, repeatMask = 0;
        uint64_t repeatAt = 0;
        auto reload = [&worker]() { return loadCatalog(worker.cancelled); };
        if (privileges) { status = "Loading catalog..."; worker.start(reload); }
        auto update = [&]() {
            visible = psgi::filterCatalog(catalog, categories[category], "", sizeSort);
            selected = visible.empty() ? 0 : std::min(selected, visible.size() - 1);
        };
        for (;;) {
            Result result;
            if (worker.take(result)) {
                status = result.message;
                if (result.refresh) {
                    catalog = std::move(result.catalog); categories = std::move(result.categories);
                    category = selected = 0; update();
                }
                // PS4's own notification UI is authoritative for background completion.
                if (result.task >= 0) status = "Task " + std::to_string(result.task) + " started. See PS4 Notifications > Downloads.";
                std::printf("%s\n", status.c_str());
            }
            OrbisPadData data{};
            uint32_t held = 0;
            if (scePadReadState(pad, &data) >= 0 && data.connected) held = data.buttons;
            uint32_t pressed = held & ~previous;
            uint32_t directions = held & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT);
            uint64_t now = sceKernelGetProcessTime();
            if (directions != repeatMask) { repeatMask = directions; repeatAt = now + 350000; }
            else if (directions && now >= repeatAt) { pressed |= directions; repeatAt = now + 100000; }
            previous = held;
            bool busy = worker.busy();
            if (pressed & ORBIS_PAD_BUTTON_OPTIONS) break;
            if (confirm) {
                if (pressed & ORBIS_PAD_BUTTON_CIRCLE) confirm = false;
                else if ((pressed & ORBIS_PAD_BUTTON_CROSS) && !busy) {
                    confirm = false;
                    const psgi::Entry entry = pending;
                    status = "Registering package with the PS4 installer...";
                    worker.start([entry, user]() {
                        Result r; r.task = psgi::queueInstall(entry, user); return r;
                    });
                }
            } else {
                if (!visible.empty()) {
                    if ((pressed & ORBIS_PAD_BUTTON_UP) && selected) --selected;
                    if ((pressed & ORBIS_PAD_BUTTON_DOWN) && selected + 1 < visible.size()) ++selected;
                    if (pressed & ORBIS_PAD_BUTTON_LEFT) selected = selected < Rows ? 0 : selected - Rows;
                    if (pressed & ORBIS_PAD_BUTTON_RIGHT) selected = std::min(selected + Rows, visible.size() - 1);
                    if ((pressed & ORBIS_PAD_BUTTON_CROSS) && !busy && privileges) {
                        pending = catalog[visible[selected]]; confirm = true;
                    }
                }
                if (pressed & ORBIS_PAD_BUTTON_R1) { category = (category + 1) % categories.size(); selected = 0; update(); }
                if (pressed & ORBIS_PAD_BUTTON_L1) { category = (category + categories.size() - 1) % categories.size(); selected = 0; update(); }
                if (pressed & ORBIS_PAD_BUTTON_SQUARE) { sizeSort = !sizeSort; selected = 0; update(); }
                if ((pressed & ORBIS_PAD_BUTTON_TRIANGLE) && !busy && privileges) {
                    status = "Refreshing catalog..."; worker.start(reload);
                }
            }
            draw(display, catalog, visible, categories, category, selected, sizeSort, worker.busy(), status, confirm ? &pending : nullptr);
        }
        scePadClose(pad);
    } catch (const std::exception& e) {
        std::printf("PS4 Games Installer fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
