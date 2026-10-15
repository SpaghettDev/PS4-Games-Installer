#include "library.hpp"
#include <algorithm>
#include <map>

namespace psgi {
void matchInstalled(std::vector<Entry>& remote, const std::vector<Entry>& installed,
                    const std::set<std::string>& queuedUrls) {
    std::map<std::string, const Entry*> titles;
    for (const auto& e : installed) if (!e.titleId.empty()) titles[e.titleId] = &e;
    for (auto& e : remote) {
        auto match = titles.find(e.titleId);
        e.installed = !e.titleId.empty() && match != titles.end();
        e.installedSize = e.installed ? match->second->installedSize : 0;
        if (e.installed && !match->second->localIcon.empty()) e.localIcon = match->second->localIcon;
        e.queued = queuedUrls.count(e.url) != 0;
    }
}
std::vector<VisibleEntry> makeView(const std::vector<Entry>& entries, const Settings& s,
                                 Library library, const std::string& query) {
    std::vector<VisibleEntry> out;
    auto q = lower(query);
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (library == Library::Available && s.notInstalledOnly && e.installed) continue;
        if (!q.empty() && lower(e.name + " " + e.titleId).find(q) == std::string::npos) continue;
        std::string group;
        if (s.grouping == Grouping::Category) group = e.category.empty() ? "Unknown" : e.category;
        else if (s.grouping == Grouping::Region) group = e.region.empty() || e.region == "UNK" ? "Unknown" : e.region;
        else if (s.grouping == Grouping::Letter) {
            char c = lower(e.name).empty() ? '#' : lower(e.name)[0];
            group = c >= 'a' && c <= 'z' ? std::string(1, c - 'a' + 'A') : "#";
        }
        out.push_back({i, group});
    }
    std::stable_sort(out.begin(), out.end(), [&](const VisibleEntry& a, const VisibleEntry& b) {
        if (a.group != b.group) return a.group < b.group;
        const auto& x = entries[a.index]; const auto& y = entries[b.index];
        uint64_t xs = library == Library::Installed ? x.installedSize : x.size;
        uint64_t ys = library == Library::Installed ? y.installedSize : y.size;
        if (s.sizeSort && xs != ys) return xs > ys;
        if (lower(x.name) != lower(y.name)) return lower(x.name) < lower(y.name);
        return x.titleId + x.url < y.titleId + y.url;
    });
    return out;
}
std::vector<std::pair<std::string, std::string>> entryDetails(const Entry& e) {
    auto unknown = [](const std::string& s) { return s.empty() ? "Unknown" : s; };
    std::vector<std::pair<std::string, std::string>> out{
        {"Title ID", unknown(e.titleId)}, {"Version", unknown(e.version)},
        {"Region", unknown(e.region)}, {"Category", unknown(e.category)},
        {"Package size", formatSize(e.size)}, {"Installed size", formatSize(e.installedSize)},
        {"State", e.queued ? "Queued" : e.installed ? "Installed" : e.titleId.empty() ? "Installation status unknown" : "Available"},
        {"Minimum firmware", unknown(e.minFirmware)}, {"Release", unknown(e.release)},
        {"Content ID", unknown(e.contentId)}, {"Package URL", unknown(e.url)}
    };
    if (!e.description.empty()) out.emplace_back("Description", e.description);
    const std::set<std::string> known{"title_id","name","version","region","category","size","min_fw","release","cover_url","content_id","description"};
    for (const auto& m : e.metadata) if (!known.count(lower(m.first))) out.push_back(m);
    return out;
}
}
