#include "storage.hpp"
#include <orbis/libkernel.h>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <cstdio>

namespace {
int value = 1, writes = 0;
bool external = true;
std::string mode;
int get(int key, int* out) {
    assert(key == 0x02880200);
    if (mode == "read-error") return -7;
    *out = value; return 0;
}
int set(int key, int next) {
    assert(key == 0x02880200 && (next == 0 || next == 1)); ++writes;
    if (mode == "write-error") return -8;
    if (mode != "ignored-write") value = next;
    return 0;
}
template<class F> void rejects(F job, const char* text) {
    bool rejected = false;
    try { job(); } catch (const std::exception& e) { rejected = std::string(e.what()).find(text) != std::string::npos; }
    assert(rejected);
}
}
namespace psgi {
StorageSpace mountedStorage(const std::string& path, const std::string& parent) {
    assert((path == "/mnt/ext0" && parent == "/mnt") || (path == "/user" && parent == "/"));
    return {path == "/user" || external, 1000000000};
}
}
extern "C" {
uint32_t sceKernelLoadStartModule(const char* path, size_t, const void*, uint32_t, void*, void*) {
    assert(std::strcmp(path, "/system/common/lib/libSceRegMgr.sprx") == 0);
    return mode == "load-error" ? 0x80020002u : 71;
}
int32_t sceKernelDlsym(int32_t module, const char* name, void** out) {
    assert(module == 71);
    if (mode == "resolve-error") return -9;
    if (std::strcmp(name, "sceRegMgrGetInt") == 0) *out = reinterpret_cast<void*>(&get);
    else { assert(std::strcmp(name, "sceRegMgrSetInt") == 0); *out = reinterpret_cast<void*>(&set); }
    return 0;
}
}
int main() {
    using namespace psgi;
    mode = "load-error"; rejects([] { readInstallDestination(); }, "Load storage settings");
    mode = "resolve-error"; rejects([] { readInstallDestination(); }, "binding");
    mode.clear(); assert(readInstallDestination() == InstallDestination::Internal);
    setInstallDestination(InstallDestination::Internal); assert(writes == 0);
    setInstallDestination(InstallDestination::External); assert(value == 0 && writes == 1);
    setInstallDestination(InstallDestination::Internal); assert(value == 1 && writes == 2);
    external = false; rejects([] { setInstallDestination(InstallDestination::External); }, "Extended Storage");
    assert(value == 1 && writes == 2); external = true;
    value = 2; rejects([] { setInstallDestination(InstallDestination::Internal); }, "Unknown PS4"); assert(writes == 2);
    value = 1; mode = "read-error"; rejects([] { setInstallDestination(InstallDestination::External); }, "Read installation");
    mode = "write-error"; rejects([] { setInstallDestination(InstallDestination::External); }, "Change installation");
    mode = "ignored-write"; rejects([] { setInstallDestination(InstallDestination::External); }, "did not accept");
    assert(value == 1); // Readback mismatch attempts to restore the previous system preference.
    mode.clear(); setInstallDestination(InstallDestination::External); assert(readInstallDestination() == InstallDestination::External);
    std::puts("Native destination selection, USB availability, enum validation, write/readback and recovery passed");
}
