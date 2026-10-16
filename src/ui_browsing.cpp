#include "ui.hpp"
#include "text.hpp"
#include <algorithm>
#include <map>

namespace psgi {
namespace {
char initial(const std::string& name) {
    const auto first=name.find_first_not_of(" \t\r\n");
    if (first==std::string::npos) return '#';
    const char c=lower(name.substr(first,1))[0]; return c>='a' && c<='z' ? c-'a'+'A' : '#';
}
}
void Ui::pageJump(bool forward) {
    if (visible.empty()) return;
    auto& cursor=selected[static_cast<int>(library)];
    if (settings.layout==Layout::Summary) {
        const auto page=cursor/8, last=(visible.size()-1)/8;
        const auto next=forward ? std::min(page+1,last) : page ? page-1 : 0;
        cursor=std::min(next*8+cursor%8,visible.size()-1);
    } else {
        const auto row=tileRowIndex(), page=row/2, last=(grid.size()-1)/2;
        const auto next=forward ? std::min(page+1,last) : page ? page-1 : 0;
        if (next==page) return;
        const auto col=cursor-grid[row].items.front();
        const auto target=std::min(next*2+row%2,grid.size()-1);
        cursor=grid[target].items[std::min(col,grid[target].items.size()-1)];
    }
}
std::vector<std::pair<char,size_t>> Ui::letters() const {
    std::map<char,size_t> counts; std::set<size_t> seen;
    for (const auto& item:visible) if (seen.insert(item.index).second) ++counts[initial(entries()[item.index].name)];
    return {counts.begin(),counts.end()};
}
std::vector<std::string> Ui::letterRows() const {
    std::vector<std::string> rows;
    for (const auto& letter:letters()) rows.push_back(std::string(1,letter.first)+" / "+std::to_string(letter.second)+(letter.second==1 ? " title" : " titles"));
    return rows;
}
std::vector<std::string> Ui::searchRows() const {
    std::vector<std::string> rows{"New search", "Clear current search"};
    for (const auto& term:recentSearches) rows.push_back(term);
    return rows;
}
void Ui::applySearch(const std::string& value) {
    try {
        const auto start=value.find_first_not_of(" \t\r\n"), end=value.find_last_not_of(" \t\r\n");
        auto term=start==std::string::npos ? "" : value.substr(start,end-start+1);
        if (term.size()>128) throw std::runtime_error("Search is limited to 128 bytes");
        toUtf16(term);
        for (unsigned char c:term) if (c<32 || c==127) throw std::runtime_error("Search cannot contain control characters");
        query=term;
        if (!term.empty()) {
            recentSearches.erase(std::remove_if(recentSearches.begin(),recentSearches.end(),[&](const std::string& old){return lower(old)==lower(term);}),recentSearches.end());
            recentSearches.insert(recentSearches.begin(),term); if (recentSearches.size()>12) recentSearches.resize(12);
        }
        selected[static_cast<int>(library)]=0; sidebar=false; rebuild(); screen=Screen::Dashboard; browseNotice.clear(); emit(CommandType::SaveSession);
    } catch (const std::exception& e) { browseNotice=status=e.what(); }
}
std::vector<std::string> Ui::profileRows() const {
    std::vector<std::string> rows;
    for (const auto& profile:draft.profiles) {
        size_t enabled=0;for (const auto& source:(profile.name==draft.activeProfile ? draft.sources : profile.sources)) if (!source.url.empty()) ++enabled;
        rows.push_back(profile.name+(profile.name==draft.activeProfile ? " [selected]" : "")+" / "+std::to_string(enabled)+(enabled==1 ? " source" : " sources"));
    }
    return rows;
}
bool Ui::handleBrowsing(Button b) {
    if (screen==Screen::Dashboard) {
        if (!sidebar && (b==Button::L2 || b==Button::R2)) { pageJump(b==Button::R2); return true; }
        if (sidebar && b==Button::Square) { jumpIndex=0; jumpReturn=Screen::Dashboard; screen=Screen::JumpLetter; return true; }
    }
    if (screen==Screen::JumpLetter) {
        if (b==Button::Circle) { screen=jumpReturn; return true; }
        const auto choices=letters(); if (choices.empty()) return true;
        jumpIndex=std::min(jumpIndex,choices.size()-1);
        if (b==Button::Up && jumpIndex) --jumpIndex;
        if (b==Button::Down && jumpIndex+1<choices.size()) ++jumpIndex;
        if (b==Button::L2) jumpIndex=jumpIndex>=9 ? jumpIndex-9 : 0;
        if (b==Button::R2) jumpIndex=std::min(jumpIndex+9,choices.size()-1);
        if (b==Button::Cross) for (size_t i=0;i<visible.size();++i) if (initial(entries()[visible[i].index].name)==choices[jumpIndex].first) {
            selected[static_cast<int>(library)]=i; sidebar=false; screen=Screen::Dashboard; emit(CommandType::SaveSession); break;
        }
        return true;
    }
    if (screen==Screen::Search || screen==Screen::ConfirmClearSearches) {
        if (screen==Screen::ConfirmClearSearches) {
            if (b==Button::Circle) screen=Screen::Search;
            if (b==Button::Cross) { recentSearches.clear(); searchIndex=0; screen=Screen::Search; browseNotice="Recent searches cleared; current search kept"; emit(CommandType::SaveSession); }
            return true;
        }
        if (b==Button::Circle) { screen=Screen::Options; return true; }
        searchIndex=std::min(searchIndex,searchRows().size()-1);
        if (b==Button::Up && searchIndex) --searchIndex;
        if (b==Button::Down && searchIndex+1<searchRows().size()) ++searchIndex;
        if (b==Button::Triangle) beginEdit(EditKind::Search,0,query);
        if (b==Button::Square && !recentSearches.empty()) screen=Screen::ConfirmClearSearches;
        if (b==Button::Cross) {
            if (!searchIndex) beginEdit(EditKind::Search,0,query);
            else applySearch(searchIndex==1 ? "" : recentSearches[searchIndex-2]);
        }
        return true;
    }
    if (screen==Screen::ServerProfiles || screen==Screen::ConfirmDeleteProfile) {
        if (screen==Screen::ConfirmDeleteProfile) {
            if (b==Button::Circle) screen=Screen::ServerProfiles;
            if (b==Button::Cross) {
                try { deleteServerProfile(draft,profileIndex); profileIndex=0; browseNotice="Profile removed from draft. Save settings to keep the change."; screen=Screen::ServerProfiles; }
                catch (const std::exception& e) { browseNotice=e.what(); }
            }
            return true;
        }
        if (b==Button::Circle) { screen=sameSettings(settings,draft) ? profileReturn : Screen::Settings; if (screen==Screen::Settings) settingsNotice="Select Save settings to keep profile/source changes."; return true; }
        if (!draft.profiles.empty()) profileIndex=std::min(profileIndex,draft.profiles.size()-1);
        if (b==Button::Up && profileIndex) --profileIndex;
        if (b==Button::Down && profileIndex+1<draft.profiles.size()) ++profileIndex;
        if (b==Button::Triangle) beginEdit(EditKind::ProfileName,0,"");
        else if (!draft.profiles.empty()) {
            if (b==Button::Square) beginEdit(EditKind::RenameProfile,profileIndex,draft.profiles[profileIndex].name);
            if (b==Button::Options) screen=Screen::ConfirmDeleteProfile;
            if (b==Button::Cross) {
                try { useServerProfile(draft,profileIndex); screen=Screen::Settings; settingsIndex=10; settingsNotice="Profile selected. Save settings to switch servers."; }
                catch (const std::exception& e) { browseNotice=e.what(); }
            }
        }
        return true;
    }
    return false;
}
}
