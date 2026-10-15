// Exercise the native adapter against SDK declarations; no PS4 service runs here.
#include "platform.hpp"
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
constexpr int MissingFile = static_cast<int32_t>(0x80020002u);
constexpr int AlreadyInitialized = static_cast<int32_t>(0x80940018u);
constexpr int ServiceError = static_cast<int32_t>(0x80940001u);
std::string scenario;
int loads = 0, initializations = 0, users = 0, launches = 0;
bool ready = false;

int initialize() {
    ++initializations;
    if (scenario == "init-failure") return ServiceError;
    ready = true;
    return scenario == "already-initialized" ? AlreadyInitialized : 0;
}
int launch(const char* title, const char* args[], LncAppParam* param) {
    ++launches;
    assert(ready && std::strcmp(title, "CUSA00001") == 0 && args == nullptr);
    assert(param && param->size == sizeof(*param) && param->user_id == 123);
    assert(param->app_opt == 0 && param->crash_report == 0);
    assert(param->LaunchAppCheck_flag == LaunchApp_SkipSystemUpdate);
    if (scenario == "launch-failure") return static_cast<int32_t>(0x80940010u);
    // A successful launch may return a positive app ID, not just zero.
    return scenario == "zero-result" ? 0 : 0x60000001;
}
void mode(const char* value) { scenario = value; ready = false; }
void rejects(const char* title, const char* message, bool reachesLaunch = false) {
    int before = launches;
    bool failed = false;
    try { psgi::launchTitle(title); }
    catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(message) != std::string::npos);
        failed = true;
    }
    assert(failed && launches == before + (reachesLaunch ? 1 : 0));
}
}

extern "C" {
uint32_t sceKernelLoadStartModule(const char* path, size_t size, const void* args,
                                uint32_t flags, void* options, void* result) {
    ++loads;
    assert(size == 0 && !args && flags == 0 && !options && !result);
    // Model the reported console: only the SystemService file exists.
    if (std::strcmp(path, "/system/common/lib/libSceSystemService.sprx") != 0)
        return static_cast<uint32_t>(MissingFile);
    return scenario == "load-failure" ? static_cast<uint32_t>(MissingFile) : 42;
}
int32_t sceKernelDlsym(int32_t module, const char* name, void** address) {
    assert(module == 42 && address);
    if (std::strcmp(name, "sceLncUtilInitialize") == 0) {
        if (scenario == "resolve-init-failure") return MissingFile;
        *address = scenario == "null-init" ? nullptr : reinterpret_cast<void*>(&initialize);
    } else {
        assert(std::strcmp(name, "sceLncUtilLaunchApp") == 0);
        if (scenario == "resolve-launch-failure") return MissingFile;
        *address = scenario == "null-launch" ? nullptr : reinterpret_cast<void*>(&launch);
    }
    return 0;
}
int32_t sceUserServiceGetForegroundUser(int32_t* user) {
    assert(ready && user); ++users;
    *user = scenario == "no-user" ? -1 : 123;
    return scenario == "user-failure" ? ServiceError : 0;
}
}

int main() {
    for (const char* title : {"", "CUSA00001/", "cusa00001", "NPXS00001", "PGIN00001"})
        rejects(title, "cannot be launched");
    assert(loads == 0 && initializations == 0 && users == 0);
    mode("load-failure"); rejects("CUSA00001", "Load game launcher service: 0x80020002");
    assert(loads == 1 && initializations == 0);
    mode("resolve-init-failure"); rejects("CUSA00001", "Resolve game launcher: 0x80020002");
    assert(loads == 2 && initializations == 0);
    mode("null-init"); rejects("CUSA00001", "initialization function is unavailable");
    mode("init-failure"); rejects("CUSA00001", "Initialize game launcher: 0x80940001");
    assert(users == 0);
    mode("already-initialized"); psgi::launchTitle("CUSA00001");
    assert(launches == 1);
    mode("user-failure"); rejects("CUSA00001", "Get foreground user: 0x80940001");
    mode("no-user"); rejects("CUSA00001", "Sign in to a PS4 user");
    mode("resolve-launch-failure"); rejects("CUSA00001", "Resolve title launch: 0x80020002");
    mode("null-launch"); rejects("CUSA00001", "Game launcher function is unavailable");
    mode("launch-failure"); rejects("CUSA00001", "Launch failed (0x80940010)", true);
    mode("zero-result"); psgi::launchTitle("CUSA00001");
    mode("success"); psgi::launchTitle("CUSA00001");
    assert(loads == 2 && launches == 4);
    std::puts("Native launcher service, parameters, initialization, errors and retry passed");
}
