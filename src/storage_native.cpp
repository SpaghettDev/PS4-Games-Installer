#include "storage.hpp"
#include <orbis/libkernel.h>
#include <cstdio>
#include <stdexcept>

namespace {
// Application Install Location, documented from a console registry comparison:
// DirectPackageInstaller-Enhanced, Payload/main_experimental.c (September 2026).
// This is a system preference, independent of BGFT's logical application slot.
constexpr int InstallLocation = 0x02880200;
void check(int rc, const char* action) {
    if (!rc) return;
    char code[24]; std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(rc));
    throw std::runtime_error(std::string(action) + ": " + code);
}
struct Registry {
    int (*get)(int, int*) = nullptr;
    int (*set)(int, int) = nullptr;
    Registry() {
        int module = sceKernelLoadStartModule("/system/common/lib/libSceRegMgr.sprx", 0, nullptr, 0, nullptr, nullptr);
        if (module < 0) check(module, "Load storage settings");
        check(sceKernelDlsym(module, "sceRegMgrGetInt", reinterpret_cast<void**>(&get)), "Read storage settings binding");
        check(sceKernelDlsym(module, "sceRegMgrSetInt", reinterpret_cast<void**>(&set)), "Write storage settings binding");
        if (!get || !set) throw std::runtime_error("Storage settings service is unavailable");
    }
};
Registry& registry() { static Registry service; return service; }
}
namespace psgi {
InstallDestination readInstallDestination() {
    int value = -1;
    check(registry().get(InstallLocation, &value), "Read installation destination");
    if (value != 0 && value != 1) throw std::runtime_error("Unknown PS4 installation destination; system setting was kept");
    return value == 0 ? InstallDestination::External : InstallDestination::Internal;
}
void setInstallDestination(InstallDestination destination) {
    if (destination != InstallDestination::Internal && destination != InstallDestination::External)
        throw std::runtime_error("Select Internal or External USB storage");
    const bool external = destination == InstallDestination::External;
    if (!mountedStorage(external ? "/mnt/ext0" : "/user", external ? "/mnt" : "/").available)
        throw std::runtime_error(external ? "Connect a writable PS4 Extended Storage drive" : "Internal storage is unavailable");
    const auto previous = readInstallDestination(); // Refuse an unrecognized registry value.
    if (previous == destination) return;
    check(registry().set(InstallLocation, external ? 0 : 1), "Change installation destination");
    try {
        if (readInstallDestination() != destination)
            throw std::runtime_error("PS4 did not accept the installation destination");
    } catch (...) {
        const int restored = registry().set(InstallLocation, previous == InstallDestination::External ? 0 : 1);
        if (restored) check(restored, "Restore installation destination; check PS4 Storage settings");
        throw;
    }
}
}
