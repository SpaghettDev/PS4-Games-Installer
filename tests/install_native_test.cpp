#include "platform.hpp"
#include "storage.hpp"
#include <orbis/libkernel.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {
struct Init { void* heap; size_t size; };
struct Params {
    int32_t user, entitlement;
    const char *id, *url, *exUrl, *name, *icon, *sku;
    int32_t option;
    const char *scenario, *date, *type, *subtype;
    uint64_t size;
};
struct Local { Params params; unsigned slot; };
struct Progress {
    uint32_t bits; int32_t error;
    uint64_t length, done, total, transferred;
    uint32_t index, count, seconds, secondsTotal;
    int32_t preparing, copy;
};
std::string mode, expectedUrl, expectedIcon;
int registrations = 0, starts = 0, removals = 0;
bool local = false;
bool update = false;
int init(Init* p) { assert(p->heap && p->size == 1024 * 1024); return 0; }
int registerPkg(Params* p, int* task) {
    ++registrations;
    assert(p->user == 123 && p->entitlement == 5 && p->option == 0x10000);
    assert(p->size == 0 && std::strcmp(p->url, expectedUrl.c_str()) == 0);
    assert(std::strcmp(p->icon, expectedIcon.c_str()) == 0 && std::strcmp(p->name, "Test game") == 0);
    *task = 45;
    if (mode == "conflict") return static_cast<int32_t>(0x80990088u);
    if (mode == "duplicate") return static_cast<int32_t>(0x80990086u);
    return 0;
}
int registerLocal(Local* p, int* task) { assert(local && p->slot == (update ? 3u : 0u)); return registerPkg(&p->params, task); }
int initApp() { return 0; }
int getSlot(const char* id, unsigned* slot) { assert(std::strcmp(id, "CUSA00001") == 0); *slot = 3; return 0; }
int start(int task) { assert(task == 45); ++starts; return mode == "start-error" ? -3 : 0; }
int unregisterTask(int task) { assert(task == 45); ++removals; return mode == "remove-error" ? -4 : 0; }
int progress(int task, Progress* p) {
    assert(task == 45); *p = {};
    if (mode == "missing") return static_cast<int32_t>(0x80990019u);
    if (mode == "progress-error") return -5;
    if (mode == "failed" || mode == "remove-error") p->error = static_cast<int32_t>(0x80991404u);
    p->total = 100; p->transferred = mode == "active" ? 50 : 100;
    p->preparing = 100; p->copy = mode == "complete" ? 100 : 0;
    if (mode == "local-complete") { p->total = p->transferred = 0; p->length = p->done = 100; p->preparing = p->copy = 100; }
    return 0;
}
template<class F> void rejects(F job, const char* text) {
    bool rejected = false;
    try { job(); } catch (const std::exception& e) { rejected = std::string(e.what()).find(text) != std::string::npos; }
    assert(rejected);
}
}
namespace psgi {
InstallStorage scanInstallStorage() {
    InstallStorage storage; storage.internal = {true, 1000000000};
    storage.destination = mode == "disconnected" ? InstallDestination::External : InstallDestination::Internal;
    return storage;
}
}
extern "C" {
uint32_t sceKernelLoadStartModule(const char* path, size_t, const void*, uint32_t, void*, void*) {
    if (update && std::strcmp(path, "/system/common/lib/libSceAppInstUtil.sprx") == 0) return 63;
    assert(std::strcmp(path, "/system/common/lib/libSceBgft.sprx") == 0); return 62;
}
int32_t sceKernelDlsym(int32_t module, const char* name, void** out) {
#define BIND(n, f) if (std::strcmp(name, n) == 0) { *out = reinterpret_cast<void*>(&f); return 0; }
    if (module == 63) {
        BIND("sceAppInstUtilInitialize", initApp)
        BIND("sceAppInstUtilGetPrimaryAppSlot", getSlot)
        assert(false);
    }
    assert(module == 62);
    BIND("sceBgftServiceInit", init)
    BIND("sceBgftServiceIntDebugDownloadRegisterPkg", registerPkg)
    BIND("sceBgftServiceIntDownloadRegisterTaskByStorageEx", registerLocal)
    BIND("sceBgftServiceDownloadStartTask", start)
    BIND("sceBgftServiceIntDownloadUnregisterTask", unregisterTask)
    BIND("sceBgftServiceDownloadGetProgress", progress)
#undef BIND
    assert(false); return -1;
}
}
int main() {
    using namespace psgi;
    Entry entry; entry.name = "Test game"; entry.url = "http://fixture/A%20Game%20%5BUS%5D.pkg?token=ok";
    entry.coverUrl = "http://fixture/icon.png"; entry.size = 999;
    expectedUrl = entry.url; expectedIcon = entry.coverUrl;
    assert(queueInstall(entry, 123) == 45 && registrations == 1 && starts == 1);
    mode = "disconnected"; rejects([&] { queueInstall(entry, 123); }, "Extended Storage is unavailable"); assert(registrations == 1);
    mode = "conflict"; rejects([&] { queueInstall(entry, 123); }, "Existing title was kept"); assert(removals == 0);
    mode = "duplicate"; rejects([&] { queueInstall(entry, 123); }, "Already queued"); assert(removals == 0);
    mode = "start-error"; rejects([&] { queueInstall(entry, 123); }, "registration removed"); assert(removals == 1);
    mode.clear(); local = true; expectedUrl = "/data/PS4-Game-Installer/downloads/test.pkg"; expectedIcon.clear();
    assert(queueLocalInstall(entry, expectedUrl, 123) == 45);
    update = true; entry.category = "updates"; entry.titleId = "CUSA00001";
    assert(queueLocalInstall(entry, expectedUrl, 123) == 45); update = false;
    rejects([&] { queueLocalInstall(entry, "../test.pkg", 123); }, "Invalid local");
    mode = "active"; assert(!taskProgress(45).completed && !retireInactiveTask(45) && removals == 1);
    mode.clear(); assert(!retireInactiveTask(45)); // 100% transferred does not prove installation finished.
    mode = "failed"; assert(taskProgress(45).error && retireInactiveTask(45) && removals == 2);
    mode = "remove-error"; assert(!retireInactiveTask(45) && removals == 3);
    mode = "complete"; assert(taskProgress(45).completed && retireInactiveTask(45) && removals == 4);
    mode = "local-complete"; assert(taskProgress(45).completed && taskProgress(45).total == 100 && retireInactiveTask(45) && removals == 5);
    mode = "missing"; assert(!taskProgress(45).found && !retireInactiveTask(45));
    mode = "progress-error"; rejects([] { taskProgress(45); }, "Read installation progress"); assert(!retireInactiveTask(45));
    std::puts("BGFT remote/local registration, URL preservation, conflicts, retry cleanup and completion checks passed");
}
