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
int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
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
        canvas.centered("Loading PS4 Games Installer", 530, 48, 0xf2f7ff);
        canvas.centered("Created by e1z0 (c) 2026 Vintage2000.org", 1016, 23, 0x9bbdd8);
        video.present(canvas);
        prepareConsole();
#ifdef PSGI_HOST
        App app(data, true);
#else
        App app(data);
#endif
        Renderer renderer(assets);
#ifdef PSGI_HOST
        if (screenshots) { capture(app, renderer, canvas); return 0; }
#endif
        const auto start = std::chrono::steady_clock::now();
        while (app.running()) {
            auto frameStart = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(frameStart - start).count();
            video.pollKeyboard(app.ui);
            for (Button button : video.input(app.ui)) app.ui.handle(button);
            app.tick(seconds, video);
            renderer.draw(canvas, app.ui, app.artwork, seconds);
            video.present(canvas);
            std::this_thread::sleep_until(frameStart + std::chrono::milliseconds(33));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "PS4 Games Installer: %s\n", e.what());
        return 1;
    }
    return 0;
}
