#include "settings.hpp"
#include "library.hpp"
#include "sfo.hpp"
#include "installed.hpp"
#include "storage.hpp"
#include "sfo_fixture.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace psgi;
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } assert(failed); }
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
    auto configured = loadSettings(root + "/data");
    assert(configured.sources[0].url.empty());
    configured.sources[0].url = "http://nas/catalog";
    configured.layout = Layout::Summary; configured.animations = false;
    saveSettings(root + "/data", configured);
    assert(sameSettings(configured, loadSettings(root + "/data")));
    auto invalid = configured; invalid.sources[0].url = "bad";
    rejects([&] { saveSettings(root + "/data", invalid); });
    assert(sameSettings(configured, loadSettings(root + "/data")));

    // Move only this application's existing directory, preserving all its data.
    const std::string parent = root + "/console";
    const std::string previous = parent + "/PS4-Games-Installer";
    const std::string canonical = parent + "/PS4-Game-Installer";
    std::filesystem::create_directories(previous + "/artwork");
    saveSettings(previous, configured);
    atomicWrite(previous + "/catalog.json", "cached catalog");
    atomicWrite(previous + "/tasks.json", "tracked tasks");
    atomicWrite(previous + "/artwork/retained.img", "cached artwork");
    assert(applicationDataDirectory(parent) == canonical);
    assert(!std::filesystem::exists(previous));
    assert(sameSettings(configured, loadSettings(canonical)));
    assert(readFile(canonical + "/catalog.json") == "cached catalog");
    assert(readFile(canonical + "/tasks.json") == "tracked tasks");
    assert(readFile(canonical + "/artwork/retained.img") == "cached artwork");
    // Repeated launches and an existing destination must never overwrite it.
    assert(applicationDataDirectory(parent) == canonical);
    std::filesystem::create_directory(previous);
    saveSettings(previous, defaults);
    assert(applicationDataDirectory(parent) == canonical);
    assert(sameSettings(configured, loadSettings(canonical)));
    assert(std::filesystem::exists(previous + "/config.json"));
    const std::string fresh = root + "/fresh";
    std::filesystem::create_directory(fresh);
    assert(applicationDataDirectory(fresh) == fresh + "/PS4-Game-Installer");
    assert(loadSettings(fresh + "/PS4-Game-Installer").sources[0].url.empty());
    const std::string blocked = root + "/blocked";
    std::filesystem::create_directory(blocked);
    atomicWrite(blocked + "/PS4-Game-Installer", "not a directory");
    rejects([&] { applicationDataDirectory(blocked); });

    for (const std::string id : {"CUSA00001", "CUSA00002", "CUSA00003", "NPXS00001", "PGIN00001"}) {
        mkdir((root + "/" + id).c_str(), 0700);
        atomicWrite(root + "/" + id + "/param.sfo", fixtureSfo(id, id == "CUSA00002" ? "gp" : "gd", id));
    }
    atomicWrite(root + "/CUSA00001/icon0.png", readFile("assets/icon.png"));
    auto installed = discoverInstalled({root, root}, {}, [](const std::string& id) { return id != "CUSA00003"; }, [](const std::string&) { return uint64_t(32000000000); });
    assert(installed.size() == 1 && installed[0].titleId == "CUSA00001");
    assert(installed[0].installedSize == 32000000000ULL);
    assert(installed[0].localIcon == root + "/CUSA00001/icon0.png");
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
