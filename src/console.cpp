#include "platform.hpp"
#include "installed.hpp"
#include "sfo.hpp"
#include "storage.hpp"
#include <cstdio>
#include <stdexcept>
#include <sys/statvfs.h>
#include <orbis/AppInstUtil.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

// This entry is absent from the pinned SystemService header. Its export belongs
// to libSceLncUtil inside SystemService and must retain that library in the import.
extern "C" int sceLncUtilInitialize(void);

namespace psgi {
namespace {
void require(int result, const char* action) {
    if (result < 0) {
        char code[32]; std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(result));
        throw std::runtime_error(std::string(action) + ": " + code);
    }
}
struct AppService {
    int module = -1;
    int (*exists)(const char*, int*) = nullptr;
    int (*getSize)(const char*, uint64_t*) = nullptr;
    AppService() {
        int (*initialize)() = nullptr;
#ifndef PSGI_EMULATION_MODE
        module = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
        require(module, "Load installed library service");

        require(sceKernelDlsym(module, "sceAppInstUtilInitialize", reinterpret_cast<void**>(&initialize)), "Resolve installed service init");
        require(sceKernelDlsym(module, "sceAppInstUtilAppExists", reinterpret_cast<void**>(&exists)), "Resolve installed registrations");
        require(sceKernelDlsym(module, "sceAppInstUtilAppGetSize", reinterpret_cast<void**>(&getSize)), "Resolve installed size");
#else
        require(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL), "Load App Install Util via sysmodule");

        initialize = sceAppInstUtilInitialize;
        exists = sceAppInstUtilAppExists;
        getSize = reinterpret_cast<decltype(getSize)>(&sceAppInstUtilAppGetSize);
#endif
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
    auto entries = discoverInstalled(
        {"/system_data/priv/appmeta", "/system_data/priv/appmeta/external", "/user/appmeta", "/user/appmeta/external"},
        {"/user/appmeta", "/user/appmeta/external"},
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
    // Kernel dlsym searches the module's default library, so it cannot resolve
    // the packed LncUtil export library. create-fself preserves the proper
    // library/module pair when these calls are linked with the LncUtil stub.
    require(static_cast<int>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE)), "Load game launcher service");
    // The service can already be initialized when returning from another game.
    const int initialized = sceLncUtilInitialize();
    if (static_cast<uint32_t>(initialized) != 0x80940018u) // Already initialized.
        require(initialized, "Initialize game launcher");
    int user = -1;
    require(sceUserServiceGetForegroundUser(&user), "Get foreground user");
    if (user < 0) throw std::runtime_error("Sign in to a PS4 user before launching a game");
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
void uninstallTitle(const std::string& id) {
    if (!validTitleId(id) || id == "PGIN00001") throw std::runtime_error("This title cannot be uninstalled");
    auto& service = appService(); int exists = 0;
    require(service.exists(id.c_str(), &exists), "Check uninstall target");
    if (!exists) throw std::runtime_error("This game is no longer installed; refresh the library");
    decltype(&sceAppInstUtilAppUnInstall) remove = nullptr;
    require(sceKernelDlsym(service.module, "sceAppInstUtilAppUnInstall", reinterpret_cast<void**>(&remove)), "Resolve game uninstall");
    if (!remove) throw std::runtime_error("Game uninstall service is unavailable");
    require(remove(id.c_str()), "Uninstall game");
}
uint64_t freeSpace(const std::string& path) {
    const auto space = filesystemSpace(path);
    return space.sizeKnown ? space.freeBytes : 0;
}
}
