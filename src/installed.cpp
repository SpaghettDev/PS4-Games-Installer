#include "installed.hpp"
#include "sfo.hpp"
#include <algorithm>
#include <dirent.h>
#include <memory>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
std::vector<Entry> discoverInstalled(const std::vector<std::string>& metadataRoots,
    const std::vector<std::string>& iconRoots,
    const std::function<bool(const std::string&)>& registered,
    const std::function<uint64_t(const std::string&)>& installedSize) {
    std::vector<Entry> entries;
    std::set<std::string> seen;
    auto artworkRoots = iconRoots;
    for (const auto& root : metadataRoots)
        if (std::find(artworkRoots.begin(), artworkRoots.end(), root) == artworkRoots.end()) artworkRoots.push_back(root);
    for (const auto& root : metadataRoots) {
        auto closeDirectory = [](DIR* dir) { closedir(dir); };
        std::unique_ptr<DIR, decltype(closeDirectory)> dir(opendir(root.c_str()), closeDirectory);
        if (!dir) continue;
        while (dirent* item = readdir(dir.get())) {
            std::string id = item->d_name;
            if (!validTitleId(id) || id == "PGIN00001" || seen.count(id)) continue;
            try {
                auto sfo = parseSfo(readFile(root + "/" + id + "/param.sfo"));
                const std::string category = sfo["CATEGORY"];
                if (category != "gd" && category != "gda" && category != "gdc") continue;
                if (!registered(id)) continue;
                Entry entry;
                entry.titleId = id;
                entry.name = sfo["TITLE"].empty() ? id : sfo["TITLE"];
                entry.contentId = sfo["CONTENT_ID"];
                entry.version = sfo["APP_VER"];
                entry.installed = true;
                entry.category = id.compare(0, 4, "CUSA") == 0 ? "games" : "homebrew";
                if (entry.contentId.compare(0, 2, "UP") == 0) entry.region = "USA";
                else if (entry.contentId.compare(0, 2, "EP") == 0) entry.region = "EUR";
                else if (entry.contentId.compare(0, 2, "JP") == 0) entry.region = "JAP";
                for (const auto& iconRoot : artworkRoots) {
                    std::string icon = iconRoot + "/" + id + "/icon0.png";
                    struct stat st{};
                    if (stat(icon.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && access(icon.c_str(), R_OK) == 0) {
                        entry.localIcon = std::move(icon);
                        break;
                    }
                }
                entry.installedSize = installedSize(id);
                for (const auto& m : sfo) entry.metadata.push_back(m);
                seen.insert(id);
                entries.push_back(std::move(entry));
            } catch (const std::exception&) {
                // One broken metadata record must not hide the rest of the library.
            }
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return lower(a.name) < lower(b.name); });
    return entries;
}
}
