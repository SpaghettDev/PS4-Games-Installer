#include "platform.hpp"
#include "storage.hpp"
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {
std::string mode; int removals = 0, loads = 0;
int initialize() { return mode == "init-error" ? -4 : 0; }
int exists(const char* id, int* out) { assert(std::strcmp(id, "CUSA00001") == 0); *out = mode != "missing"; return mode == "exists-error" ? -5 : 0; }
int size(const char*, uint64_t* out) { *out = 0; return 0; }
int removeTitle(const char* id) { assert(std::strcmp(id, "CUSA00001") == 0); ++removals; return mode == "remove-error" ? -6 : 0; }
void rejects(const char* id, const char* message) {
    bool rejected = false;
    try { psgi::uninstallTitle(id); } catch (const std::exception& error) { rejected = std::string(error.what()).find(message) != std::string::npos; }
    assert(rejected);
}
}
namespace psgi { StorageSpace filesystemSpace(const std::string&) { return {}; } }
extern "C" {
uint32_t sceKernelLoadStartModule(const char* path, size_t, const void*, uint32_t, void*, void*) {
    assert(std::strcmp(path, "/system/common/lib/libSceAppInstUtil.sprx") == 0); ++loads; return 43;
}
int32_t sceKernelDlsym(int32_t module, const char* name, void** out) {
    assert(module == 43);
    if (mode == "resolve-error" && std::strcmp(name, "sceAppInstUtilAppUnInstall") == 0) return -7;
#define BIND(n, f) if (std::strcmp(name, n) == 0) { *out = reinterpret_cast<void*>(&f); return 0; }
    BIND("sceAppInstUtilInitialize", initialize)
    BIND("sceAppInstUtilAppExists", exists)
    BIND("sceAppInstUtilAppGetSize", size)
    BIND("sceAppInstUtilAppUnInstall", removeTitle)
#undef BIND
    assert(false); return -1;
}
int sceLncUtilInitialize() { assert(false); return -1; }
int32_t sceLncUtilLaunchApp(const char*, const char*[], LncAppParam*) { assert(false); return -1; }
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal) { assert(false); return 0; }
int32_t sceUserServiceGetForegroundUser(int32_t*) { assert(false); return -1; }
}
int main() {
    for (const char* id : {"", "NPXS00001", "PGIN00001", "CUSA00001/"}) rejects(id, "cannot be uninstalled");
    assert(loads == 0 && removals == 0);
    mode = "init-error"; rejects("CUSA00001", "Initialize installed library");
    mode = "missing"; rejects("CUSA00001", "no longer installed");
    mode = "exists-error"; rejects("CUSA00001", "Check uninstall target");
    mode = "resolve-error"; rejects("CUSA00001", "Resolve game uninstall"); assert(removals == 0);
    mode = "remove-error"; rejects("CUSA00001", "Uninstall game"); assert(removals == 1);
    mode.clear(); psgi::uninstallTitle("CUSA00001"); assert(removals == 2);
    std::puts("Native uninstall target validation, registration check, service failure and retry passed");
}
