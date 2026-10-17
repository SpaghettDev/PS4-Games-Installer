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
    data.setInfo("CUSA00001", {" Adventure ", "Great story. Save the final chapter for the weekend.", {"Weekend", "Story", "weekend"}});
    data.createTerm(false, "Unassigned category"); data.createTerm(true, "Unassigned tag");
    savePlayerData(root, data); data = loadPlayerData(root); data.apply(remote);
    assert(remote[0].info.category == "Adventure" && remote[0].info.tags.size() == 2);
    assert(data.categories.size() == 2 && data.tags.size() == 3);
    auto localCopy = remote[0]; localCopy.url.clear(); data.apply(localCopy);
    assert(sameInfo(remote[0].info, localCopy.info));
    assert(remote[0].category == "games"); // Local classification never becomes the install package type.
    Ui details; details.pending = remote[0]; details.pending.info.description = "First paragraph\nSecond paragraph";
    const auto detailLines = details.details();
    assert(std::find(detailLines.begin(), detailLines.end(), "Second paragraph") != detailLines.end());
    assert(!data.observeInstalled({}, 400)); // Uninstall/missing catalog never erases history.
    remote = parseCatalog(R"({"DATA":{"http://other/new.pkg":{"name":"Renamed game","title_id":"CUSA00001","rating":1,"was_installed":false}}})", "games");
    data.apply(remote); assert(remote[0].rating == 3 && remote[0].wasInstalled);
    assert(remote[0].info.category == "Adventure" && remote[0].info.description.find("weekend") != std::string::npos);
    assert(parseCatalog(serializeCatalog(remote), "")[0].info.category.empty());
    data.renameTerm(false, "adventure", "Story games"); data.renameTerm(true, "weekend", "Evenings");
    assert(data.games.at("CUSA00001").info.category == "Story games");
    rejects([&] { data.renameTerm(true, "Evenings", "Story"); });
    data.deleteTerm(false, "Story games"); data.deleteTerm(true, "Evenings");
    assert(data.games.at("CUSA00001").info.category.empty() && data.games.at("CUSA00001").info.tags.size() == 1);
    assert(data.games.at("CUSA00001").rating == 3 && data.games.at("CUSA00001").wasInstalled);
    rejects([&] { data.setInfo("CUSA00001", {std::string(65, 'x'), "", {}}); });
    rejects([&] { data.setInfo("CUSA00001", {"", std::string(2049, 'x'), {}}); });
    // Older label fields are ignored regardless of type; all other records survive.
    auto legacy=parsePlayerData(R"({"schema":3,"games":{"CUSA00001":{"label":{"obsolete":true},"category":"Adventure","description":"Keep my notes","tags":["Weekend"],"rating":4,"was_installed":true,"launches":7}}})");
    assert(legacy.games.at("CUSA00001").info.description=="Keep my notes"&&legacy.games.at("CUSA00001").rating==4);
    assert(legacy.games.at("CUSA00001").launches==7&&legacy.games.at("CUSA00001").wasInstalled);
    const auto cleaned=serializePlayerData(legacy);
    assert(cleaned.find("label")==std::string::npos&&parsePlayerData(cleaned).games.at("CUSA00001").info.tags==std::vector<std::string>{"Weekend"});
    rejects([] { parsePlayerData(R"({"schema":1,"games":{},"tags":["same","SAME"]})"); });
    rejects([] { parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"tags":"bad"}}})"); });
    assert(parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"rating":4,"was_installed":true}}})").games.at("CUSA00001").info.category.empty());
    remote[0].installed = false;
    auto fields = entryAdditionalDetails(remote[0]);
    assert(std::find(fields.begin(), fields.end(), std::make_pair(std::string("State"), std::string("Was installed"))) != fields.end());
    assert(parseCatalog(serializeCatalog(remote), "")[0].rating == 0); // Personal values never enter the catalog format.
    data.rate("CUSA00001", 0); assert(data.games.at("CUSA00001").wasInstalled && data.games.at("CUSA00001").rating == 0);
    rejects([&] { data.rate("CUSA00001", 6); }); rejects([&] { data.rate("../game", 2); });
    rejects([] { parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"rating":-1}}})"); });
    rejects([] { parsePlayerData(R"({"schema":1,"games":{"CUSA00001":{"rating":1},"CUSA00001":{"rating":2}}})"); });
    rejects([] { parsePlayerData(R"({"schema":4,"games":{}})"); });

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

    Ui grouped; grouped.screen = Screen::Dashboard; grouped.library = Library::Available;
    auto groupEntries = parseCatalog(R"({"DATA":{"http://nas/a":{"name":"Alpha","title_id":"CUSA00001"},"http://nas/b":{"name":"Bravo","title_id":"CUSA00002"}}})", "games");
    grouped.player.setInfo("CUSA00001", {"Action", "Weekend replay", {"Solo", "Weekend"}});
    grouped.player.setInfo("CUSA00002", {"Adventure", "", {"Weekend"}});
    grouped.settings.grouping = Grouping::Tag; grouped.setLibraries({}, groupEntries);
    assert(grouped.visible.size() == 3 && grouped.shownGames() == 2 && grouped.librarySummary().find("2 available") == 0);
    grouped.selected[1] = 1; assert(grouped.visible[1].group == "Weekend");
    auto tagView = grouped.captureView(); saveViewState(root, tagView);
    grouped.selected[1] = 0; grouped.restoreView(loadViewState(root));
    assert(grouped.position() == 1 && grouped.current()->titleId == "CUSA00001");
    grouped.filter.tag = "Weekend"; grouped.filter.category = "Action"; grouped.rebuild();
    assert(grouped.visible.size() == 1 && grouped.current()->titleId == "CUSA00001");
    saveViewState(root, grouped.captureView()); const auto filteredView = loadViewState(root);
    assert(filteredView.filter.tag == "Weekend" && filteredView.filter.category == "Action");
    grouped.filter = {}; grouped.query = "replay"; grouped.rebuild(); assert(grouped.visible.size() == 2 && grouped.shownGames() == 1);
    grouped.query = ""; grouped.filter.uncategorized = true; grouped.rebuild(); assert(grouped.visible.empty());
    grouped.settings.grouping = Grouping::LocalCategory; assert(parseSettings(serializeSettings(grouped.settings)).grouping == Grouping::LocalCategory);
    grouped.settings.grouping = Grouping::Tag; assert(parseSettings(serializeSettings(grouped.settings)).grouping == Grouping::Tag);
    std::filesystem::remove_all(root);
    std::puts("Title-ID ratings/history, refresh isolation, session restore/reordering and library counts passed");
}
