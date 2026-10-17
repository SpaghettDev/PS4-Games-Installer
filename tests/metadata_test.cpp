#include "ui.hpp"
#include "metadata.hpp"
#include "metadata_fixture.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace psgi;
std::string value(const std::vector<std::pair<std::string,std::string>>& rows,const std::string& key){
    for(const auto& row:rows)if(row.first==key)return row.second;
    return "";
}
int main(){
    auto entries=parseCatalog("{\"DATA\":{\"https://nas.example/Game%20Name%20%5BEUR%5D.pkg\":{\"name\":\"Sample\",\"title_id\":\"CUSA00001\",\"version\":\"1.00\",\"package_type\":\"game\",\"size\":32000000000,\"igdb\":"+MetadataFixture+"}}}","games");
    auto e=entries.front();const auto originalUrl=e.url;
    assert(e.igdb.genres.size()==2&&e.igdb.themes.size()==2&&e.igdb.screenshots.size()==2&&e.igdb.videos.size()==2);
    assert(e.igdb.screenshots[1].url=="https://images.example.org/scene2.jpg");
    assert(e.igdb.videos[0].url=="https://www.youtube.com/watch?v=abcdefghijk");
    assert(std::abs(displayRating(e)-4.2)<0.001&&e.rating==0&&e.igdb.ratingCount==123);
    assert(displayDescription(e)==e.igdb.summary&&value(entryDetails(e),"Publisher")=="Example Publisher");
    assert(value(entryDetails(e),"Release date")=="2020-02-02");
    assert(ratingText(e).find("IGDB users")!=std::string::npos);
    const auto encoded=serializeCatalog(entries);assert(encoded.find("\"igdb\":{")!=std::string::npos);
    auto cached=parseCatalog(encoded,"");assert(cached[0].igdbJson==e.igdbJson&&cached[0].packageType=="game");
    assert(cached[0].igdbJson.find("future_metadata")!=std::string::npos);
    auto extra=entryAdditionalDetails(e);assert(value(extra,"Plot")==e.igdb.storyline);
    assert(value(extra,"languages / interface")=="English, French");
    assert(value(extra,"multiplayer / offline / max players")=="2");
    assert(value(entryTechnicalDetails(e),"Package URL")=="https://nas.example/Game Name [EUR].pkg");
    assert(e.url==originalUrl&&readableUrl("a%20b+%ZZ%0A%00%7F%C3%A9")=="a b+%ZZ%0A%00%7Fé");
    e.description="Remote description";assert(displayDescription(e)==e.description);
    PlayerData player;player.setInfo(e.titleId,{"My category", "My description",{"Action","Weekend"}});player.rate(e.titleId,2);player.apply(e);
    const auto fields=entryDetails(e);std::vector<std::string> keys;for(const auto& field:fields)keys.push_back(field.first);
    assert(keys==std::vector<std::string>({"Name","Category","Genres","Tags","Game modes","Players","Split screen","Rating","Size","Version","Publisher","Release date","Region","Description"}));
    assert(value(fields,"Description")=="My description"&&value(fields,"Category")=="My category");
    assert(value(fields,"Genres")=="Adventure, Platform"&&value(fields,"Tags")=="Action, Weekend"&&displayRating(e)==2&&ratingText(e)=="2/5 (Your rating)");
    assert(value(fields,"Game modes")=="Single player, Co-operative");
    assert(value(fields,"Players")=="Offline: Up to 2 / Online: Not supported");
    assert(value(fields,"Split screen")=="Offline: Yes / Online: No");
    assert(value(entryAdditionalDetails(e),"Themes")=="Science fiction, Action");
    auto unassigned=e;unassigned.info.tags.clear();assert(value(entryDetails(unassigned),"Tags").empty());
    assert(!value(entryDetails(unassigned),"Genres").empty());
    auto unknownPlayers=e;readIgdb(unknownPlayers,R"({"game_modes":["Single player"],"multiplayer":{"offline":{"max_players":-2},"online":{"max_players":"many","split_screen":null}}})");
    assert(playerCountText(unknownPlayers)=="Offline: 1 / Online: Unknown");
    assert(splitScreenText(unknownPlayers)=="Offline: Unknown / Online: Unknown");
    readIgdb(unknownPlayers,R"({"game_modes":["Single player","Co-operative"],"multiplayer":{"offline":{"supported":false},"online":{"supported":true,"max_players":0,"co_op_max_players":4}}})");
    assert(playerCountText(unknownPlayers)=="Offline: 1 / Online: Up to 4");
    e.metadata.emplace_back("APP_TYPE","1");assert(value(entryDetails(e),"APP_TYPE").empty()&&value(entryTechnicalDetails(e),"APP_TYPE")=="1");
    auto update=e;update.packageType="update";update.url="https://nas.example/update.pkg";
    auto dlc=e;dlc.packageType="dlc";dlc.url="https://nas.example/dlc.pkg";
    assert(packageKind(update)==1&&packageKind(dlc)==2);
    entries={e,update,dlc};Settings settings;
    assert(makeView(entries,settings,Library::Available).size()==1&&makeView(entries,settings,Library::Available,"",{},true).size()==2);
    assert(makeView(entries,settings,Library::Available,"",{},false,"Adventure").size()==1);
    assert(makeView(entries,settings,Library::Available,"",{},false,"Shooter").empty());
    assert(makeView(entries,settings,Library::Available,"Science fiction").size()==1);
    std::vector<QueueItem> jobs;enqueueJob(jobs,update,100);enqueueJob(jobs,dlc,101);
    const auto reloaded=parseQueue(serializeQueue(jobs));assert(packageKind(reloaded[0].entry)==1&&packageKind(reloaded[1].entry)==2);
    assert(!installationRequirements(reloaded[0].entry,{}).blockers.empty());
    Ui ui;ui.screen=Screen::Dashboard;ui.setLibraries(std::vector<Entry>{e},entries);ui.library=Library::Available;ui.rebuild();
    assert(ui.installed[0].igdb.userRating==84);ui.sidebar=true;ui.sidebarIndex=1;ui.handle(Button::Right);
    assert(ui.genresExpanded&&ui.sidebar&&ui.sidebarRows().size()==5);
    ui.handle(Button::Down);assert(ui.remoteGenre=="Adventure"&&ui.visible.size()==1);
    const auto session=parseViewState(serializeViewState(ui.captureView()));Ui reopened;reopened.setLibraries(std::vector<Entry>{e},entries);reopened.restoreView(session);reopened.screen=Screen::Dashboard;
    assert(reopened.remoteGenre=="Adventure"&&reopened.sidebarIndex==2&&reopened.genresExpanded);
    reopened.handle(Button::Left);assert(!reopened.genresExpanded&&reopened.remoteGenre.empty()&&reopened.sidebarIndex==1);
    ui.handle(Button::Up);assert(ui.remoteGenre.empty());ui.handle(Button::Up);assert(ui.library==Library::Installed);
    ui.handle(Button::Up);assert(ui.updatesOnly&&ui.visible.size()==2&&ui.sidebarIndex==4);
    ui.handle(Button::Down);assert(ui.library==Library::Installed);
    ui.pending=e;ui.openDetails(Screen::Actions);
    assert(!ui.detailsExpanded&&!ui.technicalExpanded);
    auto rows=ui.detailRows();size_t sections=0;for(const auto& row:rows){if(row.action)++sections;assert(row.key!="APP_TYPE");}assert(sections==4);
    while(!ui.detailRows()[ui.detailIndex].action)ui.handle(Button::Down);
    assert(ui.detailRows()[ui.detailIndex].action==3);ui.handle(Button::Cross);assert(ui.screen==Screen::Screenshots&&ui.screenshot().localIcon.empty());
    ui.handle(Button::Left);assert(ui.mediaIndex==1);ui.handle(Button::Right);assert(ui.mediaIndex==0);
    ui.handle(Button::Circle);assert(ui.screen==Screen::Details);
    ui.handle(Button::Square);assert(ui.screen==Screen::Videos);ui.handle(Button::Cross);Command command;
    assert(ui.take(command)&&command.type==CommandType::OpenMedia&&command.text==e.igdb.videos[0].url);
    ui.handle(Button::Circle);ui.handle(Button::Down);ui.handle(Button::Down);assert(ui.detailRows()[ui.detailIndex].action==1);
    ui.handle(Button::Cross);assert(ui.detailsExpanded);ui.handle(Button::Cross);assert(!ui.detailsExpanded);
    ui.handle(Button::Down);ui.handle(Button::Cross);assert(ui.technicalExpanded);
    ui.handle(Button::Circle);assert(ui.screen==Screen::Actions);
    // Removed genres clear the stale saved filter after a catalog update.
    auto noMetadata=e;noMetadata.igdb={};ui.setLibraries({},std::vector<Entry>{noMetadata});ui.restoreView(session);assert(ui.remoteGenre.empty());
    for(const auto& json:{"null","[]","{bad",R"({"genres":4,"ratings":{"users":{"score":101}},"media":{"screenshots":[{"url":"file:///etc/passwd"}]},"videos":[{"video_id":"bad"},{"url":"javascript:evil"}]})"}){
        auto bad=e;readIgdb(bad,json);assert(bad.igdb.genres.empty()&&bad.igdb.userRating==0&&bad.igdb.screenshots.empty()&&bad.igdb.videos.empty());
    }
    readIgdb(e,R"({"ratings":{"users":{"score":0,"count":1}}})");
    e.rating=0; // Clearing the personal rating reveals even a zero IGDB score.
    assert(e.igdb.userRatingKnown&&ratingText(e).find("0.0/5")==0);
    bool rejected=false;try{readIgdb(e,std::string(262145,'x'));}catch(const std::exception&){rejected=true;}assert(rejected);
    std::puts("Rich IGDB/cache, local overrides, package routing/queue, detail sections/media and remembered genre navigation passed");
}
