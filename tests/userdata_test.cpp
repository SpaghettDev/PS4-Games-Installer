#include "ui.hpp"
#include "userdata.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace psgi;
template<class F> void rejects(F job) { bool rejected = false; try { job(); } catch (const std::exception&) { rejected = true; } assert(rejected); }
int main() {
    char root[] = "/tmp/psgi-player-XXXXXX"; assert(mkdtemp(root));
    auto remote = parseCatalog(R"({"DATA":{"http://nas/a.pkg":{"name":"Alpha","title_id":"CUSA00001","rating":5},"http://nas/b.pkg":{"name":"Bravo","title_id":"CUSA00002"}}})", "games");
    PlayerData data;
    assert(data.observeInstalled({remote[0]}, 100));
    assert(!data.observeInstalled({remote[0]}, 200));
    data.rate("CUSA00001", 3); data.noteLaunch("CUSA00001", 300);
    savePlayerData(root, data); data = loadPlayerData(root);
    data.apply(remote);
    assert(remote[0].rating == 3 && remote[0].wasInstalled && remote[0].launches == 1 && remote[0].firstSeenInstalled == 100);
    assert(remote[0].lastLaunch == 300 && !remote[1].wasInstalled && remote[1].rating == 0);
    assert(!data.observeInstalled({}, 400)); // Uninstall/missing catalog never erases history.
    remote = parseCatalog(R"({"DATA":{"http://other/new.pkg":{"name":"Renamed game","title_id":"CUSA00001","rating":1,"was_installed":false}}})", "games");
    data.apply(remote); assert(remote[0].rating == 3 && remote[0].wasInstalled);
    remote[0].installed = false;
    auto fields = entryDetails(remote[0]);
    assert(std::find(fields.begin(), fields.end(), std::make_pair(std::string("State"), std::string("Was installed"))) != fields.end());
    assert(parseCatalog(serializeCatalog(remote), "")[0].rating == 0); // Personal values never enter the catalog format.
    data.rate("CUSA00001", 0); assert(data.games.at("CUSA00001").wasInstalled && data.games.at("CUSA00001").rating == 0);
    rejects([&] { data.rate("CUSA00001", 6); }); rejects([&] { data.rate("../game", 2); });
    rejects([] { parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"rating":-1}}})"); });
    rejects([] { parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"rating":1},"CUSA00001":{"rating":2}}})"); });
    rejects([] { parsePlayerData(R"({"schema":2,"games":{}})"); });

    auto entries = parseCatalog(R"({"DATA":{"http://nas/a.pkg":{"name":"Alpha","title_id":"CUSA00001"},"http://nas/b.pkg":{"name":"Bravo","title_id":"CUSA00002"}}})", "games");
    auto local = entries; for (auto& entry : local) { entry.installed = true; entry.url.clear(); }
    Ui ui; ui.screen = Screen::Dashboard; ui.setLibraries(local, entries); ui.selected[0] = ui.selected[1] = 1;
    ui.library = Library::Available;
    saveViewState(root, ui.captureView()); const auto saved = loadViewState(root);
    assert(saved.available && saved.cursor[0].titleId == "CUSA00002" && saved.cursor[1].url == "http://nas/b.pkg");
    local[1].name = "Aardvark"; entries[1].name = "Aardvark"; entries[1].url = "http://changed/b.pkg";
    Ui reopened; reopened.screen = Screen::Dashboard; reopened.setLibraries(local, entries); reopened.restoreView(saved);
    assert(reopened.library == Library::Available && reopened.current()->titleId == "CUSA00002" && reopened.position() == 0);
    reopened.handle(Button::L1); assert(reopened.current()->titleId == "CUSA00002");
    auto filtered = saved; filtered.query = "Aardvark"; filtered.sidebar = true;
    reopened.restoreView(filtered); assert(reopened.current()->titleId == "CUSA00002" && reopened.sidebar);
    local.erase(local.begin() + 1); entries.erase(entries.begin() + 1);
    reopened.setLibraries(local, entries); reopened.query.clear(); reopened.rebuild(); assert(reopened.position() == 0);
    reopened.handle(Button::L1);
    assert(reopened.librarySummary().find("installed games") != std::string::npos);
    reopened.handle(Button::R1); assert(reopened.librarySummary().find("available to install") != std::string::npos);
    // Optional or malformed server IDs must not stop saving a URL-based cursor.
    reopened.available[0].titleId = "UNKNOWN"; reopened.rebuild();
    saveViewState(root, reopened.captureView());
    assert(loadViewState(root).cursor[1].titleId.empty() && !loadViewState(root).cursor[1].url.empty());
    std::filesystem::remove_all(root);
    std::puts("Title-ID ratings/history, refresh isolation, session restore/reordering and library counts passed");
}
