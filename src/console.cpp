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
struct AppService {
    int (*exists)(const char*, int*) = nullptr;
    int (*getSize)(const char*, uint64_t*) = nullptr;
    AppService() {
        int module = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
        require(module, "Load installed library service");
        int (*initialize)() = nullptr;
        require(sceKernelDlsym(module, "sceAppInstUtilInitialize", reinterpret_cast<void**>(&initialize)), "Resolve installed service init");
        require(sceKernelDlsym(module, "sceAppInstUtilAppExists", reinterpret_cast<void**>(&exists)), "Resolve installed registrations");
        require(sceKernelDlsym(module, "sceAppInstUtilAppGetSize", reinterpret_cast<void**>(&getSize)), "Resolve installed size");
        require(initialize(), "Initialize installed library");
    }
};
AppService& appService() {
    // Load privileged services after homebrew credentials have been established.
    static AppService service;
    return service;
}
}
std::vector<Entry> scanInstalled() {
    auto& service = appService();
    size_t queried = 0, failures = 0;
    int lastError = 0;
    auto entries = discoverInstalled({"/user/appmeta", "/system_data/priv/appmeta", "/system_data/priv/appmeta/external"},
        [&](const std::string& id) {
            int exists = 0;
            int result = service.exists(id.c_str(), &exists);
            ++queried;
            if (result < 0) { ++failures; lastError = result; }
            return result == 0 && exists != 0;
        }, [&service](const std::string& id) {
            uint64_t size = 0;
            if (service.getSize(id.c_str(), &size) != 0) return uint64_t(0);
            return size;
        });
    if (queried && failures == queried) require(lastError, "Read installed registrations");
    return entries;
}
void launchTitle(const std::string& id) {
    if (!validTitleId(id) || id == "PGIN00001") throw std::runtime_error("This title cannot be launched");
    static int module = sceKernelLoadStartModule("/system/common/lib/libSceLncUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
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
    using Launch = int (*)(const char*, const char*[], LncAppParam*);
    Launch launch = nullptr;
    require(sceKernelDlsym(module, "sceLncUtilLaunchApp", reinterpret_cast<void**>(&launch)), "Resolve title launch");
    int result = launch(id.c_str(), nullptr, &param);
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
