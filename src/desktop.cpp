#include "platform.hpp"
#include <stdexcept>
#include <sys/statvfs.h>
namespace psgi {
void prepareConsole() {}
int finishApplication(int status) { return status; }
int queueInstall(const Entry&, int) { throw std::runtime_error("Installation requires a PS4"); }
int queueLocalInstall(const Entry&, const std::string&, int) { throw std::runtime_error("Installation requires a PS4"); }
bool retireInactiveTask(int) { return false; }
TaskProgress taskProgress(int) { return {0, 0, 0, false}; }
std::vector<Entry> scanInstalled() { return {}; }
void launchTitle(const std::string&) { throw std::runtime_error("Launching requires a PS4"); }
uint64_t freeSpace(const std::string& path) {
    struct statvfs info{};
    return statvfs(path.c_str(), &info) == 0 ? uint64_t(info.f_bavail) * info.f_frsize : 0;
}
}
