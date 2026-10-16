#pragma once
#include "userdata.hpp"

namespace psgi {
struct BackupDevice { std::string path; std::vector<std::string> files; };
std::vector<BackupDevice> scanBackupDevices(const std::string& root = "/mnt", bool requireMount = true);
std::string exportPlayerBackup(const std::string& device, const PlayerData& player, uint64_t now);
PlayerData readPlayerBackup(const std::string& device, const std::string& filename);
// Validate before replacing anything; preserve the current raw profile even if corrupt.
void restorePlayerBackup(const std::string& directory, const PlayerData& player, uint64_t now);
}
