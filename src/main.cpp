#include "app.hpp"
#include "render.hpp"
#include "video.hpp"
#include "platform.hpp"
#include "settings.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <thread>

namespace {
using namespace psgi;
#ifdef PSGI_HOST
void capture(App& app, Renderer& renderer, Canvas& canvas) {
    const std::string directory = "build/screenshots";
    mkdir(directory.c_str(), 0755);
    auto& ui = app.ui;
    ui.settings.animations = true; // Captures demonstrate motion regardless of preview preferences.
    for (const auto& e : ui.available) app.artwork.request(e);
    for (int i = 0; i < 100; ++i) {
        bool ready = true;
        for (const auto& e : ui.available) if (!app.artwork.request(e)) ready = false;
        if (ready) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    double time = 12;
    auto shot = [&](const std::string& name) {
        renderer.draw(canvas, ui, app.artwork, time);
        time += 1;
        renderer.draw(canvas, ui, app.artwork, time);
        canvas.writePpm(directory + "/" + name + ".ppm");
        std::printf("Captured %s\n", name.c_str());
        time += 1;
    };
    ui.screen = Screen::Splash; shot("01-splash");
    ui.screen = Screen::Dashboard; ui.library = Library::Installed; ui.rebuild(); shot("02-installed-tiles");
    ui.library = Library::Available; ui.rebuild(); shot("03-available-tiles");
    shot("16-bubbles-tiles"); shot("17-bubbles-tiles-motion");
    ui.settings.layout = Layout::Summary; ui.rebuild(); shot("04-summary");
    shot("18-bubbles-summary"); shot("19-bubbles-summary-motion");
    ui.screen = Screen::Options; ui.menuIndex = 0; shot("05-options");
    ui.screen = Screen::Settings; ui.draft = ui.settings; ui.draft.sources[0].url = "http://192.168.1.10:3000/games"; shot("06-settings");
    ui.screen = Screen::Storage; shot("13-storage-available");
    ui.storage.external = {}; shot("14-storage-unavailable");
    ui.screen = Screen::Keyboard; ui.keyboardText = ui.draft.sources[0].url; shot("07-url-editor");
    ui.screen = Screen::Details; ui.pending = ui.available[0]; shot("08-details");
    ui.library = Library::Installed; ui.rebuild();
    for (size_t i = 0; i < ui.visible.size(); ++i) if (ui.installed[ui.visible[i].index].titleId == ui.installed[0].titleId) ui.selected[0] = i;
    ui.pending = *ui.current(); ui.screen = Screen::Actions; ui.menuIndex = 0; shot("20-installed-actions");
    ui.screen = Screen::ConfirmUninstall; shot("21-uninstall-confirmation");
    ui.screen = Screen::Rating; ui.ratingIndex = 3; shot("22-rating");
    ui.localDraft = ui.pending.info; ui.localIndex = 3; ui.screen = Screen::LocalInfo; shot("24-local-info");
    ui.pickerIndex = 1; ui.screen = Screen::PickTags; shot("25-game-tags");
    ui.screen = Screen::Actions; ui.menuIndex = ui.actionRows().size() - 1; shot("30-actions-last-page");
    ui.quickAssignment = true; ui.screen = Screen::PickCategory; ui.pickerIndex = 1; shot("31-assign-category");
    ui.screen = Screen::PickTags; shot("32-assign-tags"); ui.quickAssignment = false;
    ui.library = Library::Available; ui.rebuild(); ui.pending = *ui.current();
    ui.screen = Screen::ConfirmInstall; shot("15-install-confirmation");
    ui.failedInstall = ui.pending;
    ui.installError = "PS4 refused installation (0x80990086). No matching task could be confirmed. Close the app, restart the PS4, enable GoldHEN and retry. Downloaded package kept.";
    ui.screen = Screen::InstallError; shot("23-installation-failed"); ui.installError.clear();
    ui.collectionTags = false; ui.collectionIndex = 2; ui.screen = Screen::Collections; shot("26-categories");
    ui.collectionTags = true; shot("27-tags");
    ui.screen = Screen::Dashboard; ui.settings.layout = Layout::Tiles; ui.settings.grouping = Grouping::LocalCategory; ui.selected[1] = 0; ui.rebuild(); shot("28-category-view");
    ui.settings.layout = Layout::Summary; ui.settings.grouping = Grouping::Tag; ui.rebuild(); shot("29-tag-view");
    ui.settings.grouping = Grouping::None; ui.rebuild();
    ui.bulkMode = true; ui.markedTitles = {ui.available[0].titleId, ui.available[1].titleId}; ui.screen = Screen::Dashboard; shot("33-bulk-selection");
    ui.screen = Screen::BulkMenu; ui.bulkIndex = 1; shot("34-bulk-actions");
    ui.quickAssignment = ui.bulkEditing = true; ui.bulkOperation = 1; ui.localDraft = {}; ui.localDraft.tags = {"Weekend"};
    ui.pending = {}; ui.pending.name = "2 selected games"; ui.pickerIndex = 3; ui.screen = Screen::PickTags; shot("35-bulk-tags");
    ui.quickAssignment = ui.bulkEditing = ui.bulkMode = false; ui.markedTitles.clear();
    ui.screen = Screen::SavedCollections; ui.savedIndex = 4; shot("36-saved-collections");
    ui.savedDraft = ui.player.collections.front(); ui.savedOriginal = ui.savedDraft.name; ui.savedEditIndex = 5; ui.screen = Screen::EditSaved; shot("37-collection-rules");
    ui.savedEditIndex = 9; shot("47-collection-save");
    ui.screen = Screen::Queue; ui.queueIndex = 1; shot("38-download-queue");
    ui.queueIndex = 3; ui.screen = Screen::QueueDetails; ui.detailScroll = 0; shot("39-queue-error-details");
    ui.queueIndex = 5; shot("48-queue-verification");
    ui.screen = Screen::Backups; ui.backupIndex = 0; shot("40-usb-backup");
    ui.restoreDevice = ui.backupDevices.front().path; ui.restoreFilename = ui.backupDevices.front().files.front(); ui.screen = Screen::ConfirmRestore; shot("41-restore-confirmation");
    ui.setupSource = {"games", "http://192.168.1.10:3000/games"}; ui.sourceReport = {}; ui.sourceReport.success = true; ui.sourceReport.source = ui.setupSource;
    ui.sourceReport.packages = 44; ui.sourceReport.games = 44; ui.featureNotice = "Valid catalog: 44 packages."; ui.screen = Screen::SourceSetup; ui.setupIndex = 2; shot("42-source-setup");
    ui.sourceReport.success = false; ui.featureNotice = "The server returned a web page. Enter the JSON catalog endpoint rather than its home page."; ui.setupIndex = 1; shot("43-source-error");
    ui.featureNotice.clear(); ui.packageTitle = ui.available.front().titleId; ui.packageIndex = 1; ui.screen = Screen::Packages; shot("44-game-packages");
    ui.settings.grouping = Grouping::Title; ui.settings.layout = Layout::Tiles; ui.screen = Screen::Dashboard;
    ui.rebuild();
    for (size_t i = 0; i < ui.visible.size(); ++i) if (ui.available[ui.visible[i].index].titleId == ui.packageTitle) { ui.selected[1] = i; break; }
    shot("45-package-groups");
    ui.screen = Screen::Options; ui.menuIndex = 12; shot("46-more-options");
    ui.settings.grouping = Grouping::None; ui.rebuild();
    ui.screen = Screen::About; shot("09-about");
    ui.screen = Screen::ConfirmExit; shot("10-exit");
    ui.screen = Screen::Dashboard; ui.available.clear(); ui.rebuild(); shot("11-empty");
    ui.status = "Server unavailable. Showing the last valid library; Installed works offline.";
    ui.library = Library::Installed; ui.rebuild(); shot("12-offline");
}
#endif
}
static int runApplication(int argc, char** argv) {
    try {
#ifdef PSGI_HOST
        bool screenshots = argc > 1 && std::strcmp(argv[1], "--capture") == 0;
        const std::string assets = "assets", data = "build/preview-data";
#else
        (void)argc; (void)argv;
        const bool screenshots = false;
        const std::string assets = "/app0/assets";
#endif
        Canvas canvas(assets + "/font.ttf");
        Video video(screenshots);
        canvas.background(0, true);
        canvas.centered("Loading PS4 Games Installer", 560, 48, 0xf2f7ff);
        canvas.centered("Created by e1z0 (c) 2026 Vintage2000.org", 1016, 23, 0x9bbdd8);
        video.present(canvas);
        // Animation time starts at the first presented frame.
        const auto start = std::chrono::steady_clock::now();
        try {
        // libjbc changes the filesystem root. Read /app0 resources while the
        // application's sandbox paths are still accessible.
        Renderer renderer(assets);
        prepareConsole();
#ifdef PSGI_HOST
        App app(data, true);
#else
        App app(applicationDataDirectory());
#endif
#ifdef PSGI_HOST
        if (screenshots) { capture(app, renderer, canvas); return 0; }
#endif
        while (app.running()) {
            auto frameStart = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(frameStart - start).count();
            video.pollKeyboard(app.ui);
            auto buttons = video.input(app.ui);
            if (app.ui.screen != Screen::Splash)
                for (Button button : buttons) app.ui.handle(button);
            app.tick(seconds, video);
            renderer.draw(canvas, app.ui, app.artwork, seconds);
            video.present(canvas);
            std::this_thread::sleep_until(frameStart + std::chrono::milliseconds(33));
        }
        } catch (const std::exception& e) {
#ifdef PSGI_HOST
            throw;
#else
            // Keep startup/runtime failures readable on a television until Circle closes them.
            Ui errorUi;
            bool stay = true;
            video.flushInput();
            while (stay) {
                canvas.background(0, false);
                canvas.centered("PS4 Games Installer could not continue", 430, 40, 0xf2f7ff);
                canvas.text(e.what(), 200, 520, 27, 0x9bbdd8, 1520);
                canvas.centered("Check GoldHEN and application storage, then try again.", 600, 26, 0x9bbdd8);
                canvas.centered("O Exit", 750, 28, 0x7ae5e0);
                video.present(canvas);
                for (Button button : video.input(errorUi)) if (button == Button::Circle) stay = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
            }
            return 1;
#endif
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "PS4 Games Installer: %s\n", e.what());
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int status = runApplication(argc, argv);
    // runApplication's scopes have closed the video/controller and joined all
    // workers before native credentials are restored and the shell takes over.
    return psgi::finishApplication(status);
}
