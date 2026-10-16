#include "backup.hpp"
#include "settings.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
const std::string Prefix = "PS4-Games-Installer-player-";
bool backupName(const std::string& name) {
    return name.size() > Prefix.size() + 5 && name.size() < 100 && name.compare(0, Prefix.size(), Prefix) == 0 &&
        name.compare(name.size() - 5, 5, ".json") == 0 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") == std::string::npos;
}
std::string freshPath(const std::string& folder, const std::string& prefix, uint64_t now) {
    for (unsigned i = 0; i < 1000; ++i) {
        auto path = folder + "/" + prefix + std::to_string(now) + "-" + std::to_string(i) + ".json";
        if (access(path.c_str(), F_OK) && errno == ENOENT) return path;
    }
    throw std::runtime_error("Cannot create a new backup filename");
}
void requireDevice(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) || !S_ISDIR(st.st_mode) || access(path.c_str(), W_OK))
        throw std::runtime_error("USB drive is disconnected or read-only");
}
}
std::vector<BackupDevice> scanBackupDevices(const std::string& root, bool requireMount) {
    std::vector<BackupDevice> devices;
    for (unsigned i = 0; i < 8; ++i) {
        const auto path = root + "/usb" + std::to_string(i);
        if (requireMount && !mountedStorage(path, root).available) continue;
        try { requireDevice(path); } catch (const std::exception&) { continue; }
        auto closeDirectory = [](DIR* value) { closedir(value); };
        std::unique_ptr<DIR, decltype(closeDirectory)> dir(opendir(path.c_str()), closeDirectory);
        if (!dir) continue;
        BackupDevice device{path, {}};
        while (auto* item = readdir(dir.get())) {
            const std::string name = item->d_name;
            if (!backupName(name)) continue;
            struct stat st{};
            if (!lstat((path + "/" + name).c_str(), &st) && S_ISREG(st.st_mode)) device.files.push_back(name);
            if (device.files.size() >= 256) break;
        }
        std::sort(device.files.rbegin(), device.files.rend()); devices.push_back(std::move(device));
    }
    return devices;
}
std::string exportPlayerBackup(const std::string& device, const PlayerData& player, uint64_t now) {
    requireDevice(device);
    const auto text = serializePlayerData(player); parsePlayerData(text);
    const auto path = freshPath(device, Prefix, now);
    atomicWrite(path, text);
    if (readFile(path) != text) throw std::runtime_error("USB backup readback did not match");
    return path;
}
PlayerData readPlayerBackup(const std::string& device, const std::string& filename) {
    if (!backupName(filename)) throw std::runtime_error("Invalid backup filename");
    const auto path = device + "/" + filename;
    struct stat st{};
    if (lstat(path.c_str(), &st) || !S_ISREG(st.st_mode)) throw std::runtime_error("Backup file is unavailable");
    return parsePlayerData(readFile(path));
}
void restorePlayerBackup(const std::string& directory, const PlayerData& player, uint64_t now) {
    const auto text = serializePlayerData(player); parsePlayerData(text);
    const auto current = directory + "/player.json";
    if (!access(current.c_str(), F_OK)) {
        const auto previous = freshPath(directory, "player-before-restore-", now);
        atomicWrite(previous, readFile(current));
    } else if (errno != ENOENT) throw std::runtime_error("Cannot preserve the current player profile");
    atomicWrite(current, text);
}
}
