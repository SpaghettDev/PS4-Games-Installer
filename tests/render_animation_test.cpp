#include "render.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

using namespace psgi;
int main() {
    char temporary[] = "/tmp/psgi-animation-XXXXXX";
    assert(mkdtemp(temporary));
    {
        Canvas canvas("assets/font.ttf"); Renderer renderer("assets"); ArtworkCache artwork(temporary);
        Ui ui; ui.screen = Screen::Dashboard; ui.loading = false; ui.library = Library::Available;
        Entry entry; entry.name = "Animation fixture"; entry.titleId = "CUSA00001";
        entry.localIcon = "assets/icon.png"; entry.url = "http://fixture/game.pkg";
        std::shared_ptr<Image> loaded;
        for (int i = 0; i < 200 && !loaded; ++i) {
            loaded = artwork.request(entry, true);
            if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        assert(loaded);
        ui.setLibraries({}, std::vector<Entry>{entry});
        for (auto layout : {Layout::Tiles, Layout::Summary}) {
            ui.settings.layout = layout; ui.settings.animations = true;
            renderer.draw(canvas, ui, artwork, 0); // Settle the artwork crossfade first.
            renderer.draw(canvas, ui, artwork, 10); const auto before = canvas.pixels;
            renderer.draw(canvas, ui, artwork, 12);
            size_t visibleChanges = 0; unsigned contrast = 0;
            // Measure exposed dashboard regions, outside game cards and text.
            for (int y = 0; y < Canvas::Height; ++y) for (int x = 0; x < Canvas::Width; ++x) {
                const size_t index = y * Canvas::Width + x;
                if (canvas.pixels[index] != before[index]) ++visibleChanges;
                for (unsigned shift : {0u, 8u, 16u}) {
                    int delta = int((canvas.pixels[index] >> shift) & 255) - int((before[index] >> shift) & 255);
                    contrast = std::max(contrast, unsigned(std::abs(delta)));
                }
            }
            // Keep the requested subtle background style while verifying that
            // its clock still moves particles in both dashboard layouts.
            assert(visibleChanges > 1000 && contrast > 0);
            if (layout == Layout::Tiles) {
                for (int y = 280; y < 380; ++y) for (int x = 450; x < 550; ++x)
                    assert(canvas.pixels[y * Canvas::Width + x] == before[y * Canvas::Width + x]);
            }
            ui.settings.animations = false;
            renderer.draw(canvas, ui, artwork, 20); const auto paused = canvas.pixels;
            renderer.draw(canvas, ui, artwork, 30); assert(canvas.pixels == paused);
            std::printf("%s: %zu exposed pixels moved; peak channel contrast %u; disabled animation stays still\n",
                        layout == Layout::Tiles ? "Tiles" : "Summary", visibleChanges, contrast);
        }
        ui.screen = Screen::Settings; ui.draft = ui.settings;
        ui.settings.animations = true; ui.draft.animations = false;
        renderer.draw(canvas, ui, artwork, 40); const auto paused = canvas.pixels;
        renderer.draw(canvas, ui, artwork, 50); assert(canvas.pixels == paused);
    }
    std::filesystem::remove_all(temporary);
}
