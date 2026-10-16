#include "library.hpp"
#include <algorithm>
#include <map>
#include <ctime>
#include <limits>

namespace psgi {
std::optional<int> compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& text) -> std::vector<unsigned> {
        std::vector<unsigned> out; unsigned value = 0, digits = 0;
        if (text.empty() || text.size() > 64) return {};
        for (char c : text + ".") {
            if (c == '.') {
                if (!digits || out.size() >= 8) return {};
                out.push_back(value); value = digits = 0;
            } else if (c >= '0' && c <= '9' && ++digits <= 6) value = value * 10 + unsigned(c - '0');
            else return {};
        }
        while (out.size() > 1 && out.back() == 0) out.pop_back();
        return out;
    };
    auto x = parts(a), y = parts(b);
    if (x.empty() || y.empty()) return {};
    x.resize(std::max(x.size(), y.size())); y.resize(x.size());
    return x == y ? 0 : x < y ? -1 : 1;
}
unsigned packageKind(const Entry& e) {
    const auto category = lower(e.category);
    return category == "updates" || category == "update" || category == "patch" ? 1 :
        category == "dlc" || category == "addons" ? 2 : category == "games" || category == "homebrew" || category.empty() ? 0 : 3;
}
std::string packageKindName(const Entry& e) {
    const char* names[] = {"Base game", "Update", "DLC", "Other"}; return names[packageKind(e)];
}
std::string packageStatus(const Entry& e) {
    if (e.queued) return "Queued";
    if (packageKind(e) == 2) return e.baseInstalled ? "Base installed; DLC status unknown" : "DLC; base game required";
    if (e.installed) return "Installed";
    if (e.baseInstalled && packageKind(e) == 1) {
        auto version = compareVersions(e.version, e.installedVersion);
        return version && *version > 0 ? "Newer update available" : "Base installed; version unconfirmed";
    }
    if (e.wasInstalled) return "Was installed";
    return "Available";
}
std::vector<size_t> relatedPackages(const std::vector<Entry>& entries, const std::string& id) {
    std::vector<size_t> out;
    if (id.empty()) return out;
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].titleId == id) out.push_back(i);
    std::stable_sort(out.begin(), out.end(), [&](size_t a, size_t b) {
        const auto& x = entries[a]; const auto& y = entries[b];
        if (packageKind(x) != packageKind(y)) return packageKind(x) < packageKind(y);
        const bool knownX = compareVersions(x.version, x.version).has_value(), knownY = compareVersions(y.version, y.version).has_value();
        if (knownX != knownY) return knownX;
        auto version = compareVersions(x.version, y.version);
        if (version && *version) return *version > 0;
        return lower(x.name) < lower(y.name);
    });
    return out;
}
void matchInstalled(std::vector<Entry>& remote, const std::vector<Entry>& installed,
                    const std::set<std::string>& queuedUrls) {
    std::map<std::string, const Entry*> titles;
    for (const auto& e : installed) if (!e.titleId.empty()) titles[e.titleId] = &e;
    for (auto& e : remote) {
        const bool previouslyPresent = e.installed;
        auto match = titles.find(e.titleId);
        e.baseInstalled = !e.titleId.empty() && match != titles.end();
        e.installedVersion = e.baseInstalled ? match->second->version : "";
        e.installed = e.baseInstalled && packageKind(e) == 0;
        if (e.baseInstalled && packageKind(e) == 1) {
            auto version = compareVersions(e.installedVersion, e.version); e.installed = version && *version >= 0;
        }
        e.installedSize = e.baseInstalled ? match->second->installedSize : 0;
        if (e.baseInstalled && !match->second->localIcon.empty()) e.localIcon = match->second->localIcon;
        else if (!e.baseInstalled && previouslyPresent) e.localIcon.clear();
        e.queued = queuedUrls.count(e.url) != 0;
    }
}
bool matchesLocal(const Entry& entry, const LocalFilter& filter) {
    if (entry.rating < filter.minRating || entry.rating > filter.maxRating) return false;
    if ((filter.launchState == 1 && entry.launches) || (filter.launchState == 2 && !entry.launches)) return false;
    if ((filter.installState == 1 && !entry.installed) || (filter.installState == 2 && entry.installed) ||
        (filter.installState == 3 && (entry.installed || !entry.wasInstalled))) return false;
    if (filter.uncategorized && !entry.info.category.empty()) return false;
    if (filter.untagged && !entry.info.tags.empty()) return false;
    if (!filter.category.empty() && lower(filter.category) != lower(entry.info.category)) return false;
    if (!filter.tag.empty() && std::none_of(entry.info.tags.begin(), entry.info.tags.end(), [&](const std::string& tag) { return lower(tag) == lower(filter.tag); })) return false;
    return true;
}
std::vector<VisibleEntry> makeView(const std::vector<Entry>& entries, const Settings& s,
                                 Library library, const std::string& query, const LocalFilter& filter) {
    std::vector<VisibleEntry> out;
    auto q = lower(query);
    std::map<std::string, std::string> titleNames;
    if (s.grouping == Grouping::Title) for (const auto& e : entries) if (!e.titleId.empty() && packageKind(e) == 0) titleNames[e.titleId] = e.name;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (!matchesLocal(e, filter)) continue;
        if (library == Library::Available && s.notInstalledOnly && e.installed) continue;
        std::string searchable = e.name + " " + e.titleId + " " + e.info.category + " " + e.info.label + " " + e.info.description;
        for (const auto& tag : e.info.tags) searchable += " " + tag;
        if (!q.empty() && lower(searchable).find(q) == std::string::npos) continue;
        std::string group;
        if (s.grouping == Grouping::Category) group = e.category.empty() ? "Unknown" : e.category;
        else if (s.grouping == Grouping::Region) group = e.region.empty() || e.region == "UNK" ? "Unknown" : e.region;
        else if (s.grouping == Grouping::Letter) {
            char c = lower(e.name).empty() ? '#' : lower(e.name)[0];
            group = c >= 'a' && c <= 'z' ? std::string(1, c - 'a' + 'A') : "#";
        }
        else if (s.grouping == Grouping::LocalCategory) group = e.info.category.empty() ? "Uncategorized" : e.info.category;
        else if (s.grouping == Grouping::Title) group = e.titleId.empty() ? "No Title ID: " + e.name : e.titleId + (titleNames.count(e.titleId) ? " / " + titleNames.at(e.titleId) : "");
        else if (s.grouping == Grouping::Tag) {
            if (e.info.tags.empty()) group = "Untagged";
            else { for (const auto& tag : e.info.tags) if (filter.tag.empty() || lower(tag) == lower(filter.tag)) out.push_back({i, tag}); continue; }
        }
        out.push_back({i, group});
    }
    std::stable_sort(out.begin(), out.end(), [&](const VisibleEntry& a, const VisibleEntry& b) {
        if (a.group != b.group) return a.group < b.group;
        const auto& x = entries[a.index]; const auto& y = entries[b.index];
        if (s.grouping == Grouping::Title) {
            if (packageKind(x) != packageKind(y)) return packageKind(x) < packageKind(y);
            const bool knownX = compareVersions(x.version, x.version).has_value(), knownY = compareVersions(y.version, y.version).has_value();
            if (knownX != knownY) return knownX;
            auto version = compareVersions(x.version, y.version);
            if (version && *version) return *version > 0;
        }
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
        {"Name", e.name},
        {"Local category", e.info.category.empty() ? "Uncategorized" : e.info.category},
        {"Local label", e.info.label.empty() ? "Unlabeled" : e.info.label},
        {"Local tags", e.info.tags.empty() ? "Untagged" : [&] { std::string value; for (const auto& tag : e.info.tags) { if (!value.empty()) value += ", "; value += tag; } return value; }()},
        {"Local description", e.info.description.empty() ? "Not set" : e.info.description},
        {"Title ID", unknown(e.titleId)}, {"Version", unknown(e.version)},
        {"Package type", packageKindName(e)}, {"Installed version", unknown(e.installedVersion.empty() && e.installed ? e.version : e.installedVersion)},
        {"Latest catalog update", unknown(e.latestVersion)},
        {"Region", unknown(e.region)}, {"Category", unknown(e.category)},
        {"Package size", formatSize(e.size)}, {"Installed size", formatSize(e.installedSize)},
        {"State", packageStatus(e)},
        {"Your local rating", e.rating ? std::to_string(e.rating) + "/5 stars" : "Not rated"},
        {"Installation history", e.wasInstalled ? "Previously observed installed" : "Not recorded"},
        {"Launch requests", std::to_string(e.launches)},
        {"Minimum firmware", unknown(e.minFirmware)}, {"Release", unknown(e.release)},
        {"Content ID", unknown(e.contentId)}, {"Package URL", unknown(e.url)}
    };
    for (const auto& date : std::vector<std::pair<std::string, uint64_t>>{{"First recorded installed", e.firstSeenInstalled}, {"Last launch request", e.lastLaunch}}) {
        if (!date.second) continue;
        const std::time_t timestamp = date.second; const auto* utc = std::gmtime(&timestamp); char text[40];
        if (utc && std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", utc)) out.emplace_back(date.first, text);
    }
    if (!e.description.empty()) out.emplace_back("Description", e.description);
    const std::set<std::string> known{"title_id","name","version","region","category","size","min_fw","release","cover_url","content_id","description","rating","was_installed","launches","first_seen_installed","last_launch"};
    for (const auto& m : e.metadata) if (!known.count(lower(m.first))) out.push_back(m);
    return out;
}
}
