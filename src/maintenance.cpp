#include "maintenance.hpp"
#include "library.hpp"
#include "settings.hpp"
#include "sfo.hpp"
#include "json.hpp"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
struct File { std::string path; uint64_t bytes = 0; bool removable = false; };
bool named(const std::string& name, const std::string& prefix, const std::string& suffix) {
    if (name.size() != prefix.size() + 16 + suffix.size() || name.compare(0, prefix.size(), prefix) || name.compare(prefix.size() + 16, suffix.size(), suffix)) return false;
    return name.substr(prefix.size(), 16).find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::vector<File> files(const std::string& directory, bool artwork, const std::set<std::string>& protectedPaths, bool trusted) {
    std::vector<File> found;
    // Do not follow a replaced directory or per-file symlinks.
    struct stat st{};
    if (lstat(directory.c_str(), &st)) { if (errno == ENOENT) return found; throw std::runtime_error("Cannot inspect cache directory"); }
    if (!S_ISDIR(st.st_mode)) throw std::runtime_error("Cache path is not a directory");
    auto closeDirectory = [](DIR* directory) { closedir(directory); };
    std::unique_ptr<DIR, decltype(closeDirectory)> dir(opendir(directory.c_str()), closeDirectory);
    if (!dir) throw std::runtime_error("Cannot read cache directory");
    while (auto* item = readdir(dir.get())) {
        const std::string name = item->d_name;
        const bool partial = named(name, "", ".pkg.part");
        if (!(artwork ? named(name, "art-", ".img") : (partial || named(name, "", ".pkg")))) continue;
        File file; file.path = directory + "/" + name;
        if (lstat(file.path.c_str(), &st) || !S_ISREG(st.st_mode)) continue;
        FILE* input = std::fopen(file.path.c_str(), "rb");
        if (!input) { if (errno == ENOENT) continue; throw std::runtime_error("Cannot measure cache file"); }
        // The toolchain's stat size ABI differs from the PS4 kernel; use stdio.
        const bool measured = !fseeko(input, 0, SEEK_END); const auto size = measured ? ftello(input) : -1; std::fclose(input);
        if (size < 0) throw std::runtime_error("Cannot measure cache file");
        file.bytes = uint64_t(size);
        const auto base = partial ? file.path.substr(0, file.path.size() - 5) : file.path;
        bool released = false;
        struct stat lease{};
        if (!lstat((base + ".lease").c_str(), &lease) && S_ISREG(lease.st_mode)) {
            FILE* marker = std::fopen((base + ".lease").c_str(), "rb");
            if (marker) { char text[16]{}; const auto count = std::fread(text, 1, sizeof(text), marker); std::fclose(marker); released = std::string(text, count) == "released\n"; }
        }
        file.removable = artwork || (trusted && !protectedPaths.count(base) && (partial || released));
        found.push_back(std::move(file));
    }
    return found;
}
void numeric(cJSON* object, const char* name, uint64_t value) { cJSON_AddNumberToObject(object, name, double(value)); }
std::string print(cJSON* root) {
    std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root,true), cJSON_free);
    if (!text) throw std::runtime_error("Cannot serialize diagnostics");
    return text.get();
}
}
StorageUsage scanStorageUsage(const std::string& directory, const std::set<std::string>& protectedPaths, bool trusted) {
    StorageUsage usage;
    for (const auto& f : files(directory + "/artwork", true, {}, true)) usage.artwork += f.bytes;
    for (const auto& f : files(directory + "/downloads", false, protectedPaths, trusted)) {
        usage.staging += f.bytes;
        if (f.removable) { usage.reclaimable += f.bytes; ++usage.removableFiles; }
        else { usage.protectedBytes += f.bytes; ++usage.protectedFiles; }
    }
    usage.checked = true; return usage;
}
CleanupResult cleanStaging(const std::string& directory, const std::set<std::string>& protectedPaths, bool trusted) {
    if (!trusted) throw std::runtime_error("Queue tracking needs repair; staged packages were kept");
    CleanupResult result;
    for (const auto& f : files(directory + "/downloads", false, protectedPaths, trusted)) if (f.removable) {
        if (unlink(f.path.c_str())) { ++result.failed; continue; }
        result.bytes += f.bytes; ++result.files;
        if (f.path.size() >= 4 && f.path.compare(f.path.size() - 4, 4, ".pkg") == 0) unlink((f.path + ".lease").c_str());
    }
    return result;
}
std::string diagnosticEvent(const std::string& message) {
    // Allowlisted operation names/codes only. Never export arbitrary text, URLs,
    // credentials, title names or personal notes, even for unrecognized errors.
    const auto folded = lower(message);
    for (const char* secret : {"password", "passwd", "token", "secret", "authorization", "cookie", "api_key", "apikey", "access_key", "credential", "bearer"})
        if (folded.find(secret) != std::string::npos) return "";
    auto safe = folded;
    const auto scheme = safe.find("://");
    if (scheme != std::string::npos) {
        auto start = safe.find_last_of(" \t\r\n", scheme);
        safe.resize(start == std::string::npos ? 0 : start);
    }
    std::string event;
    for (const char* kind : {"install", "download", "launch", "refresh", "storage", "queue", "source", "save", "cancel", "http", "bgft"})
        if (safe.find(kind) != std::string::npos) { event = kind; break; }
    if (event.empty()) return "";
    for (size_t i = 0; i < safe.size(); ++i) {
        if (safe.compare(i, 2, "0x") == 0 && i + 10 <= safe.size() && (i == 0 || !std::isalnum(static_cast<unsigned char>(safe[i-1]))) &&
            safe.substr(i+2, 8).find_first_not_of("0123456789abcdef") == std::string::npos &&
            (i+10 == safe.size() || !std::isalnum(static_cast<unsigned char>(safe[i+10])))) { event += " " + safe.substr(i, 10); i += 9; }
        else if (safe.compare(i, 3, "ce-") == 0 && i+10 <= safe.size() && safe.substr(i+3,5).find_first_not_of("0123456789") == std::string::npos && safe[i+8]=='-' && std::isdigit(static_cast<unsigned char>(safe[i+9])) && (i+10==safe.size() || !std::isalnum(static_cast<unsigned char>(safe[i+10])))) { event += " " + safe.substr(i,10); i += 9; }
    }
    // HTTP status must have an explicit status prefix, never an arbitrary number.
    for (const char* prefix : {"http status code: ", "http status: "}) {
        const auto index = safe.find(prefix);
        if (index != std::string::npos) {
            const auto digits = safe.substr(index + std::string(prefix).size(), 3);
            if (digits.size() == 3 && digits[0] >= '1' && digits[0] <= '5' && digits.find_first_not_of("0123456789") == std::string::npos) event += " HTTP " + digits;
        }
    }
    return event;
}
std::string diagnosticReport(const DiagnosticSnapshot& s, uint64_t now) {
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    numeric(root.get(), "schema", 1); numeric(root.get(), "revision", s.revision); numeric(root.get(), "created", now);
    cJSON_AddStringToObject(root.get(), "application", "PS4 Games Installer");
    cJSON_AddStringToObject(root.get(), "privacy", "Selected counts, stages and error codes only. Raw logs, URLs, names, config and personal records omitted.");
    numeric(root.get(), "installed_count", s.installed); numeric(root.get(), "available_count", s.available);
    auto* storage = cJSON_AddObjectToObject(root.get(), "storage");
    numeric(storage, "destination", unsigned(s.storage.destination));
    for (const auto& pair : {std::make_pair("internal",s.storage.internal),std::make_pair("external",s.storage.external)}) {
        auto* device = cJSON_AddObjectToObject(storage, pair.first); cJSON_AddBoolToObject(device,"available",pair.second.available);
        cJSON_AddBoolToObject(device,"size_known",pair.second.sizeKnown); numeric(device,"free_bytes",pair.second.freeBytes);
    }
    auto* usage = cJSON_AddObjectToObject(root.get(), "cache");
    cJSON_AddBoolToObject(usage, "checked", s.usage.checked); numeric(usage,"artwork_bytes",s.usage.artwork); numeric(usage,"staging_bytes",s.usage.staging); numeric(usage,"reclaimable_bytes",s.usage.reclaimable); numeric(usage,"protected_bytes",s.usage.protectedBytes);
    auto* jobs = cJSON_AddArrayToObject(root.get(), "queue");
    for (const auto& job : s.queue) {
        auto* item = cJSON_CreateObject(); cJSON_AddItemToArray(jobs,item);
        if (validTitleId(job.entry.titleId)) cJSON_AddStringToObject(item,"title_id",job.entry.titleId.c_str());
        const auto version = compareVersions(job.entry.version,job.entry.version) ? job.entry.version : "unknown";
        cJSON_AddStringToObject(item,"version",version.c_str()); cJSON_AddStringToObject(item,"kind",packageKindName(job.entry).c_str());
        cJSON_AddStringToObject(item,"stage",queueStageName(job.stage).c_str());
        cJSON_AddNumberToObject(item,"task",job.task); numeric(item,"transferred",job.transferred); numeric(item,"total",job.total);
        cJSON_AddBoolToObject(item,"background_transfer",job.backgroundTransfer); cJSON_AddBoolToObject(item,"has_dependency",!job.dependsOn.empty());
        cJSON_AddStringToObject(item,"event",diagnosticEvent(job.message).c_str());
    }
    auto* events = cJSON_AddArrayToObject(root.get(), "events");
    const auto tail = s.logTail.substr(s.logTail.size() > 262144 ? s.logTail.size()-262144 : 0);
    size_t start=0, count=0;
    while (start < tail.size() && count < 1000) {
        const auto end=tail.find('\n',start);
        const auto event=diagnosticEvent(tail.substr(start,end==std::string::npos ? end : end-start));
        if (!event.empty()) { cJSON_AddItemToArray(events,cJSON_CreateString(event.c_str())); ++count; }
        if (end==std::string::npos) break;
        start=end+1;
    }
    return print(root.get());
}
std::string exportDiagnostics(const std::string& device, const DiagnosticSnapshot& s, uint64_t now) {
    struct stat st{};
    if (stat(device.c_str(),&st) || !S_ISDIR(st.st_mode) || access(device.c_str(),W_OK)) throw std::runtime_error("Diagnostic USB drive is disconnected or read-only");
    const auto text = diagnosticReport(s,now);
    for (unsigned i=0;i<1000;++i) {
        const auto name = "PS4-Games-Installer-diagnostics-" + std::to_string(now) + "-" + std::to_string(i) + ".json";
        const auto path = device + "/" + name;
        if (!lstat(path.c_str(),&st) || errno != ENOENT) continue;
        atomicWrite(path,text);
        if (readFile(path) != text) throw std::runtime_error("Diagnostic USB read-back failed");
        return name;
    }
    throw std::runtime_error("Cannot create a new diagnostic filename");
}
}
