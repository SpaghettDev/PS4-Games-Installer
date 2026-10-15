#include "platform.hpp"
#include "installed.hpp"
#include "sfo.hpp"
#include <cstdio>
#include <stdexcept>
#include <sys/statvfs.h>
#include <orbis/AppInstUtil.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

namespace psgi {
namespace {
void require(int result, const char* action) {
    if (result < 0) {
        char code[32]; std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(result));
        throw std::runtime_error(std::string(action) + ": " + code);
    }
}
int appModule() {
    static int module = [] {
        int id = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
        require(id, "Load installed library service");
        require(sceAppInstUtilInitialize(), "Initialize installed library");
        return id;
    }();
    return module;
}
}
std::vector<Entry> scanInstalled() {
    int module = appModule();
    using GetSize = int (*)(const char*, uint64_t*);
    GetSize getSize = nullptr;
    require(sceKernelDlsym(module, "sceAppInstUtilAppGetSize", reinterpret_cast<void**>(&getSize)), "Resolve installed size");
    return discoverInstalled({"/user/appmeta", "/system_data/priv/appmeta", "/system_data/priv/appmeta/external"},
        [](const std::string& id) {
            int exists = 0;
            return sceAppInstUtilAppExists(id.c_str(), &exists) == 0 && exists != 0;
        }, [getSize](const std::string& id) {
            uint64_t size = 0;
            if (getSize(id.c_str(), &size) != 0) return uint64_t(0);
            return size;
        });
}
void launchTitle(const std::string& id) {
    if (!validTitleId(id) || id == "PGIN00001") throw std::runtime_error("This title cannot be launched");
    static int module = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, nullptr, 0, nullptr, nullptr);
    require(module, "Load game launcher");
    using Init = int (*)();
    Init init = nullptr;
    require(sceKernelDlsym(module, "sceLncUtilInitialize", reinterpret_cast<void**>(&init)), "Resolve game launcher");
    // The service can already be initialized when returning from another game.
    init();
    int user = -1;
    require(sceUserServiceGetForegroundUser(&user), "Get foreground user");
    LncAppParam param{};
    param.size = sizeof(param);
    param.user_id = user;
    param.LaunchAppCheck_flag = LaunchApp_SkipSystemUpdate;
    int result = sceLncUtilLaunchApp(id.c_str(), nullptr, &param);
    if (result < 0) {
        char message[180];
        std::snprintf(message, sizeof(message), "Launch failed (0x%08X). Close any running game or check that its storage is connected.", static_cast<unsigned>(result));
        throw std::runtime_error(message);
    }
}
uint64_t freeSpace(const std::string& path) {
    struct statvfs info{};
    return statvfs(path.c_str(), &info) == 0 ? uint64_t(info.f_bavail) * info.f_frsize : 0;
}
}
