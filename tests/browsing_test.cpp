#include "ui.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>

using namespace psgi;
template<class F> void rejects(F fn) { bool failed=false;try{fn();}catch(const std::exception&){failed=true;}assert(failed); }
void drain(Ui& ui){Command command;while(ui.take(command)) {}}
int main(){
    Settings settings=parseSettings(R"({"schema":1,"content_urls":{"games":"http://home/games","updates":"http://home/updates","dlc":"http://home/dlc"}})");
    assert(settings.profiles.empty() && settings.activeProfile.empty());
    addServerProfile(settings," Home NAS ");assert(settings.activeProfile=="Home NAS" && settings.profiles[0].sources.size()==3);
    const auto homeKey=catalogCachePath("/tmp",settings);
    settings.sources={{"games","https://remote/games?token=preserved"}};
    addServerProfile(settings,"Remote");assert(settings.profiles[0].sources.size()==3 && settings.profiles[0].sources[0].url=="http://home/games");
    assert(catalogCachePath("/tmp",settings)!=homeKey && catalogCachePath("/tmp",settings).find("preserved")==std::string::npos);
    auto restored=parseSettings(serializeSettings(settings));useServerProfile(restored,0);
    assert(restored.sources.size()==3 && restored.sources[2].category=="dlc" && catalogCachePath("/tmp",restored)==homeKey);
    useServerProfile(restored,1);assert(restored.sources[0].url==settings.sources[0].url);
    restored.sources[0].url="https://new-remote/games";syncActiveProfile(restored);useServerProfile(restored,0);useServerProfile(restored,1);
    assert(restored.sources[0].url=="https://new-remote/games");renameServerProfile(restored,1,"Away");assert(restored.activeProfile=="Away");
    auto before=serializeSettings(restored);
    rejects([&]{addServerProfile(restored,"home nas");});rejects([&]{renameServerProfile(restored,1,"HOME NAS");});
    rejects([&]{addServerProfile(restored,"");});rejects([&]{addServerProfile(restored,"bad\nname");});rejects([&]{addServerProfile(restored,std::string(65,'a'));});
    rejects([&]{useServerProfile(restored,99);});assert(serializeSettings(restored)==before);
    deleteServerProfile(restored,1);assert(restored.activeProfile.empty() && restored.sources[0].url=="https://new-remote/games");
    Settings full;for(int i=0;i<16;++i)addServerProfile(full,"Server "+std::to_string(i));rejects([&]{addServerProfile(full,"Overflow");});
    rejects([]{parseSettings(R"({"content_urls":{},"server_profiles":{},"active_profile":""})");});
    rejects([]{parseSettings(R"({"content_urls":{},"server_profiles":[],"active_profile":"Missing"})");});
    rejects([]{parseSettings(R"({"content_urls":{},"server_profiles":[{"name":"Home","content_urls":{"games":"file:///bad"}}]})");});

    std::vector<Entry> games;
    for(int i=0;i<25;++i){Entry game;game.name=std::string(1,'A'+i)+" Game";game.titleId="CUSA"+std::to_string(10000+i);game.url="http://nas/"+std::to_string(i);game.info.tags={"One","Two"};games.push_back(game);}
    Ui ui;ui.screen=Screen::Dashboard;ui.library=Library::Available;ui.setLibraries(games,games);
    ui.handle(Button::Right);ui.handle(Button::R2);assert(ui.position()==9);ui.handle(Button::L2);assert(ui.position()==1);
    ui.settings.layout=Layout::Summary;ui.selected[1]=7;ui.rebuild();ui.handle(Button::R2);assert(ui.position()==15);ui.handle(Button::R2);assert(ui.position()==23);ui.handle(Button::R2);assert(ui.position()==24);ui.handle(Button::R2);assert(ui.position()==24);
    ui.handle(Button::L2);assert(ui.position()==16);
    ui.settings.layout=Layout::Tiles;ui.settings.grouping=Grouping::Letter;ui.selected[1]=0;ui.rebuild();ui.handle(Button::R2);assert(ui.position()==2);ui.handle(Button::L2);assert(ui.position()==0);
    ui.settings.grouping=Grouping::None;ui.rebuild();ui.handle(Button::Left);assert(ui.sidebar);ui.handle(Button::Square);assert(ui.screen==Screen::JumpLetter);
    ui.jumpIndex=0;ui.handle(Button::R2);assert(ui.jumpIndex==9);ui.handle(Button::L2);assert(ui.jumpIndex==0);
    ui.jumpIndex=12;ui.handle(Button::Cross);assert(!ui.sidebar && ui.screen==Screen::Dashboard && ui.current()->name=="M Game");drain(ui);
    ui.handle(Button::Options);ui.menuIndex=19;ui.handle(Button::Cross);ui.handle(Button::Circle);assert(ui.screen==Screen::Options);
    ui.query="Y Game";ui.rebuild();assert(ui.letters().size()==1 && ui.letterRows()[0]=="Y / 1 title");
    ui.query="missing";ui.rebuild();ui.screen=Screen::JumpLetter;ui.handle(Button::Cross);assert(ui.screen==Screen::JumpLetter);
    ui.query.clear();ui.rebuild();
    ui.screen=Screen::Options;ui.menuIndex=1;ui.handle(Button::Cross);ui.handle(Button::Cross);assert(ui.screen==Screen::Keyboard);drain(ui);ui.finishEdit({});assert(ui.recentSearches.empty() && ui.screen==Screen::Search);
    auto search=[&](const std::string& value){ui.screen=Screen::Search;ui.handle(Button::Triangle);drain(ui);ui.finishEdit(value);drain(ui);};
    search("  A Game  ");search("B Game");search("a game");assert(ui.recentSearches==std::vector<std::string>({"a game","B Game"}) && ui.visible.size()==1);
    const auto query=ui.query;search(std::string("bad\xff",4));assert(ui.query==query);
    for(int i=0;i<15;++i) { search("Term "+std::to_string(i)); }
    assert(ui.recentSearches.size()==12 && ui.recentSearches.front()=="Term 14");
    ui.screen=Screen::Search;ui.searchIndex=3;ui.handle(Button::Cross);assert(ui.query=="Term 13" && ui.recentSearches[0]=="Term 13");drain(ui);
    const auto state=parseViewState(serializeViewState(ui.captureView()));Ui reopened;reopened.setLibraries(games,games);reopened.restoreView(state);assert(reopened.recentSearches==ui.recentSearches && reopened.query==ui.query);
    ui.screen=Screen::Search;ui.handle(Button::Square);ui.handle(Button::Circle);assert(ui.recentSearches.size()==12);ui.handle(Button::Square);ui.handle(Button::Cross);assert(ui.recentSearches.empty() && ui.query=="Term 13");drain(ui);
    ui.searchIndex=1;ui.handle(Button::Cross);assert(ui.query.empty() && ui.recentSearches.empty() && ui.visible.size()==25);
    ui.screen=Screen::Options;ui.menuIndex=20;ui.handle(Button::Cross);assert(ui.screen==Screen::ServerProfiles);ui.handle(Button::Triangle);drain(ui);ui.finishEdit("Home");
    assert(ui.draft.profiles.size()==1 && ui.settings.profiles.empty());ui.handle(Button::Circle);assert(ui.screen==Screen::Settings);ui.handle(Button::Circle);assert(ui.screen==Screen::Discard);ui.handle(Button::Circle);assert(ui.draft.profiles.empty());
    ui.screen=Screen::Settings;ui.settingsIndex=16;ui.handle(Button::Cross);ui.handle(Button::Triangle);drain(ui);ui.finishEdit("Home");ui.handle(Button::Square);drain(ui);ui.finishEdit("NAS");assert(ui.draft.profiles[0].name=="NAS");
    ui.handle(Button::Options);ui.handle(Button::Circle);assert(ui.draft.profiles.size()==1);ui.handle(Button::Options);ui.handle(Button::Cross);assert(ui.draft.profiles.empty());
    char temporary[]="/tmp/psgi-browsing-XXXXXX";assert(mkdtemp(temporary));const std::string root=temporary;
    saveViewState(root,state);assert(loadViewState(root).recentSearches==state.recentSearches);
    saveSettings(root,settings);assert(loadSettings(root).profiles.size()==2 && loadSettings(root).activeProfile=="Remote");
    std::filesystem::create_directory(root+"/session.json.tmp");rejects([&]{saveViewState(root,ViewState{});});assert(loadViewState(root).recentSearches==state.recentSearches);
    std::filesystem::remove_all(root);
    std::puts("Page/letter navigation, recent search persistence, profile drafts/validation and source cache identity passed");
}
