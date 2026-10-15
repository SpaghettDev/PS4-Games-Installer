#include "artwork.hpp"
#include "canvas.hpp"
#include "text.hpp"
#include "installed.hpp"
#include "settings.hpp"
#include "sfo_fixture.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>

using namespace psgi;
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } assert(failed); }
int main() {
    auto image = loadImage("assets/icon.png");
    assert(image->width == 384 && image->height == 384 && image->pixels.size() == 384 * 384);
    rejects([] { decodeImage("not a PNG"); });
    rejects([] { decodeImage(std::string(MaxCatalogBytes + 1, 'x')); });
    const std::string text = "URL / Žaidimai / 日本語 / \xf0\x9f\x8e\xae";
    auto utf16 = toUtf16(text);
    assert(fromUtf16(utf16.data(), utf16.size()) == text);
    rejects([] { toUtf16("\xc0\xaf"); });
    rejects([] { toUtf16("\xf4\x90\x80\x80"); });
    const char16_t broken[] = {0xd800};
    rejects([&] { fromUtf16(broken, 1); });
    Canvas canvas("assets/font.ttf");
    canvas.background(1, false);
    auto still = canvas.pixels;
    canvas.background(10, false); assert(still == canvas.pixels);
    canvas.background(10, true); assert(still != canvas.pixels);
    // These exercise clipping at every edge under AddressSanitizer/UBSan.
    canvas.rectangle(5000, 0, 100, 100, 0xffffff);
    canvas.rectangle(-5000, 0, 100, 100, 0xffffff);
    canvas.rectangle(-10, -10, 100, 100, 0xffffff);
    canvas.rectangle(1900, 1070, 100, 100, 0xffffff, 128);
    canvas.rounded(-50, 1050, 200, 100, 40, 0xabcdef, 128);
    canvas.image(*image, -200, -100, 500, 500);
    canvas.image(*image, 1900, 1070, 400, 400, 100);
    canvas.text(text, -15, 30, 26, 0xffffff, 120);
    canvas.text("invalid \xff UTF8", 0, 80, 24, 0xffffff, 1900);
    assert(canvas.textWidth("PS4 Games Installer", 24) > 0);

    char root[] = "/tmp/psgi-art-XXXXXX";
    assert(mkdtemp(root));
    {
        ArtworkCache cache(root);
        Entry entry; entry.localIcon = "assets/icon.png";
        std::shared_ptr<Image> loaded;
        for (int i = 0; i < 100 && !loaded; ++i) {
            loaded = cache.request(entry, true);
            if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(loaded && loaded->width == 384);
        cache.refresh(entry);
        cache.clear();
        entry.localIcon = "/missing/icon.png";
        assert(!cache.request(entry));
    }
    // Match the console's separate metadata/icon trees, including Extended Storage.
    {
        const std::string internalMeta = std::string(root) + "/system_data/priv/appmeta";
        const std::string externalMeta = internalMeta + "/external";
        const std::string internalIcons = std::string(root) + "/user/appmeta";
        const std::string externalIcons = internalIcons + "/external";
        auto metadata = [&](const std::string& directory, const std::string& id) {
            std::filesystem::create_directories(directory + "/" + id);
            atomicWrite(directory + "/" + id + "/param.sfo", fixtureSfo(id, "gd", id));
        };
        auto icon = [&](const std::string& directory, const std::string& id) {
            std::filesystem::create_directories(directory + "/" + id);
            atomicWrite(directory + "/" + id + "/icon0.png", readFile("assets/icon.png"));
        };
        metadata(internalMeta, "CUSA00001"); icon(internalIcons, "CUSA00001");
        metadata(externalMeta, "CUSA00001"); // Duplicate metadata must not hide its public icon.
        metadata(externalMeta, "CUSA00002"); icon(externalIcons, "CUSA00002");
        metadata(internalMeta, "CUSA00003"); // Missing artwork must not hide the title.
        metadata(internalMeta, "CUSA00004"); icon(internalMeta, "CUSA00004"); // Co-located fallback.
        // Ignore an empty icon in an earlier tree, and prefer public over private art.
        std::filesystem::create_directories(internalIcons + "/CUSA00002");
        atomicWrite(internalIcons + "/CUSA00002/icon0.png", "");
        atomicWrite(internalMeta + "/CUSA00001/icon0.png", "not the public PNG");
        auto entries = discoverInstalled({internalMeta, externalMeta}, {internalIcons, externalIcons},
            [](const std::string&) { return true; }, [](const std::string&) { return uint64_t(32); });
        assert(entries.size() == 4);
        assert(entries[0].localIcon == internalIcons + "/CUSA00001/icon0.png");
        assert(entries[1].localIcon == externalIcons + "/CUSA00002/icon0.png");
        assert(entries[2].localIcon.empty());
        assert(entries[3].localIcon == internalMeta + "/CUSA00004/icon0.png");
        ArtworkCache cache(std::string(root) + "/offline-cache");
        for (size_t index : {0u, 1u, 3u}) {
            const auto& entry = entries[index];
            assert(entry.coverUrl.empty()); // No server/catalog is needed for installed icons.
            std::shared_ptr<Image> loaded;
            for (int i = 0; i < 100 && !loaded; ++i) {
                loaded = cache.request(entry, true);
                if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            assert(loaded && loaded->pixels == image->pixels);
        }
    }
    std::filesystem::remove_all(root);
    std::puts("Artwork, installed icons across metadata trees, Unicode and renderer bounds tests passed");
}
