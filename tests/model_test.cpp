#include "settings.hpp"
#include "library.hpp"
#include "sfo.hpp"
#include "installed.hpp"
#include "storage.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace psgi;
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } assert(failed); }
std::string fixtureSfo(std::string title = "A game", std::string category = "gd") {
    std::vector<std::pair<std::string, std::string>> fields{{"TITLE", title}, {"TITLE_ID", "CUSA00001"}, {"CATEGORY", category}, {"APP_VER", "1.00"}};
    std::string keys, values;
    std::string out(20 + fields.size() * 16, '\0');
    auto u32 = [&](size_t offset, uint32_t n) { for (int i = 0; i < 4; ++i) out[offset + i] = n >> (i * 8); };
    u32(0, 0x46535000); u32(4, 0x101); u32(8, out.size()); u32(16, fields.size());
    for (size_t i = 0; i < fields.size(); ++i) {
        size_t offset = 20 + i * 16;
        u32(offset, uint32_t(keys.size()) | 0x02040000);
        u32(offset + 4, fields[i].second.size() + 1);
        u32(offset + 8, fields[i].second.size() + 1);
        u32(offset + 12, values.size());
        keys += fields[i].first + '\0'; values += fields[i].second + '\0';
    }
    u32(12, out.size() + keys.size());
    return out + keys + values;
}
int main() {
    Settings defaults;
    assert(sameSettings(defaults, parseSettings(serializeSettings(defaults))));
    auto old = parseSettings(R"({"preferences":{"CONTENT_URLS":{"PS5":"http://nas/ps5","games":"http://nas/games","DLC":null}}})");
    assert(old.sources.size() == 2 && old.sources[0].url == "http://nas/games");
    rejects([] { parseSettings(R"({"content_urls":{},"animations":"yes"})"); });
    rejects([] { parseSettings(R"({"content_urls":{"games":"ftp://nas/a"}})"); });
    rejects([] { parseSettings(R"({"content_urls":{"games":null,"Games":null}})"); });
    auto sfo = fixtureSfo();
    assert(parseSfo(sfo)["TITLE"] == "A game");
    rejects([&] { parseSfo(sfo.substr(0, 40)); });
    sfo[8] = 0; rejects([&] { parseSfo(sfo); });
    assert(validTitleId("CUSA00001") && !validTitleId("../../etc") && !validTitleId("NPXS00001"));

    char directory[] = "/tmp/psgi-model-XXXXXX";
    assert(mkdtemp(directory));
    std::string root(directory);
    // A stale mount directory on the parent filesystem is not a connected drive.
    assert(!mountedStorage(root, "/tmp").available);
    assert(!mountedStorage(root + "/missing", root).available);
    assert(!mountedStorage("/dev/null", "/dev").available);
    auto mounted = mountedStorage("/dev/shm", "/dev");
    assert(mounted.available && mounted.freeBytes > 0);
    atomicWrite(root + "/legacy.json", R"({"content_urls":{"games":"http://nas/catalog"}})");
    auto migrated = loadSettings(root + "/data", root + "/legacy.json");
    assert(migrated.sources[0].url == "http://nas/catalog");
    migrated.layout = Layout::Summary; migrated.animations = false;
    saveSettings(root + "/data", migrated);
    assert(sameSettings(migrated, loadSettings(root + "/data", "")));
    auto invalid = migrated; invalid.sources[0].url = "bad";
    rejects([&] { saveSettings(root + "/data", invalid); });
    assert(sameSettings(migrated, loadSettings(root + "/data", "")));

    for (const std::string id : {"CUSA00001", "CUSA00002", "CUSA00003", "NPXS00001", "PGIN00001"}) {
        mkdir((root + "/" + id).c_str(), 0700);
        atomicWrite(root + "/" + id + "/param.sfo", fixtureSfo(id, id == "CUSA00002" ? "gp" : "gd"));
    }
    auto installed = discoverInstalled({root, root}, [](const std::string& id) { return id != "CUSA00003"; }, [](const std::string&) { return uint64_t(32000000000); });
    assert(installed.size() == 1 && installed[0].titleId == "CUSA00001");
    assert(installed[0].installedSize == 32000000000ULL);
    auto remote = parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","title_id":"CUSA00001","cover_url":"http://nas/icon","release":"2026","size":"32000000000"},"http://nas/b":{"name":"Beta","title_id":"CUSA00002"},"http://nas/c":{"name":"42","size":1}}})", "games");
    matchInstalled(remote, installed, {"http://nas/b"});
    assert(remote[0].installed && remote[1].queued && !remote[2].installed);
    assert(remote[0].coverUrl == "http://nas/icon" && remote[0].release == "2026");
    auto cached = parseCatalog(serializeCatalog(remote), "");
    assert(cached[0].category == "games" && cached[0].size == remote[0].size);
    Settings settings;
    settings.notInstalledOnly = true;
    auto view = makeView(remote, settings, Library::Available);
    assert(view.size() == 2 && view[0].index == 2);
    settings.notInstalledOnly = false; settings.grouping = Grouping::Letter;
    view = makeView(remote, settings, Library::Available);
    assert(view.size() == 3 && view[0].group == "#" && view[1].group == "A");
    assert(makeView(remote, settings, Library::Available, "cusa00002").size() == 1);
    assert(entryDetails(remote[0]).size() >= 11);
    std::puts("Settings, SFO, installed discovery and library model tests passed");
}
