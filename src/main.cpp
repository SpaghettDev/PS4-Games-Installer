#include "app.hpp"
#include "render.hpp"
#include "video.hpp"
#include "platform.hpp"
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
    ui.settings.layout = Layout::Summary; ui.rebuild(); shot("04-summary");
    ui.screen = Screen::Options; ui.menuIndex = 0; shot("05-options");
    ui.screen = Screen::Settings; ui.draft = ui.settings; ui.draft.sources[0].url = "http://192.168.1.10:3000/games"; shot("06-settings");
    ui.screen = Screen::Storage; shot("13-storage-available");
    ui.storage.external = {}; shot("14-storage-unavailable");
    ui.screen = Screen::Keyboard; ui.keyboardText = ui.draft.sources[0].url; shot("07-url-editor");
    ui.screen = Screen::Details; ui.pending = ui.available[0]; shot("08-details");
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
        const std::string assets = "/app0/assets", data = "/data/PS4-Games-Installer";
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
        App app(data);
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
