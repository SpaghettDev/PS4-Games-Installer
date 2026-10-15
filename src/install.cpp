#include "platform.hpp"
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <orbis/libkernel.h>
#include <libjbc.h>

namespace {
// BGFT ABI uses a 64-bit unsigned long package size on PS4.
struct InitParams { void* heap; size_t heapSize; };
struct DownloadParams {
    int32_t userId, entitlementType;
    const char *id, *contentUrl, *contentExUrl, *contentName, *iconPath, *skuId;
    int32_t option;
    const char *playgoScenarioId, *releaseDate, *packageType, *packageSubType;
    uint64_t packageSize;
};
struct Progress {
    uint32_t bits; int32_t error;
    uint64_t length, transferred, lengthTotal, transferredTotal;
    uint32_t numIndex, numTotal, restSec, restSecTotal;
    int32_t preparingPercent, localCopyPercent;
};
static_assert(sizeof(Progress) == 64, "BGFT progress ABI");
static_assert(offsetof(DownloadParams, packageSize) == 96, "BGFT ABI");
static_assert(sizeof(DownloadParams) == 104, "BGFT download ABI");

std::string hexCode(int rc) {
    char out[16];
    std::snprintf(out, sizeof(out), "0x%08X", static_cast<unsigned>(rc));
    return out;
}
void check(int rc, const char* action) {
    if (rc != 0) throw std::runtime_error(std::string(action) + ": " + hexCode(rc));
}
template<typename T> T symbol(int module, const char* name) {
    T fn = nullptr;
    check(sceKernelDlsym(module, name, reinterpret_cast<void**>(&fn)), name);
    if (!fn) throw std::runtime_error(std::string("Missing ") + name);
    return fn;
}
struct Installer {
    std::vector<unsigned char> heap;
    int (*registerPkg)(DownloadParams*, int*) = nullptr;
    int (*start)(int) = nullptr;
    int (*unregisterTask)(int) = nullptr;
    int (*progress)(int, Progress*) = nullptr;
    Installer() : heap(1024 * 1024) {
        int module = sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, nullptr, 0, nullptr, nullptr);
        if (module < 0) throw std::runtime_error("Load BGFT: " + hexCode(module));
        auto init = symbol<int (*)(InitParams*)>(module, "sceBgftServiceInit");
        registerPkg = symbol<decltype(registerPkg)>(module, "sceBgftServiceIntDebugDownloadRegisterPkg");
        start = symbol<decltype(start)>(module, "sceBgftServiceDownloadStartTask");
        unregisterTask = symbol<decltype(unregisterTask)>(module, "sceBgftServiceIntDownloadUnregisterTask");
        progress = symbol<decltype(progress)>(module, "sceBgftServiceDownloadGetProgress");
        InitParams params{heap.data(), heap.size()};
        check(init(&params), "BGFT init");
    }
};
Installer& installer() {
    // Intentionally retain the client heap until process exit; queued tasks belong to BGFT.
    static Installer instance;
    return instance;
}
}

namespace psgi {
void prepareConsole() {
    jbc_cred cred{};
    check(jbc_get_cred(&cred), "Read homebrew credentials");
    check(jbc_jailbreak_cred(&cred), "Prepare homebrew credentials");
    check(jbc_set_cred(&cred), "Apply homebrew credentials");
}

int queueInstall(const Entry& entry, int userId) {
    if (!isHttpUrl(entry.url) || entry.name.empty() || entry.name.size() >= 600)
        throw std::runtime_error("Invalid package metadata");
    auto& bgft = installer();
    DownloadParams params{};
    params.userId = userId;
    params.entitlementType = 5;
    params.id = "";
    params.contentUrl = entry.url.c_str();
    params.contentExUrl = "";
    params.contentName = entry.name.c_str();
    params.iconPath = entry.coverUrl.c_str();
    params.skuId = "";
    params.option = 0x10000; // Do not append CDN query parameters to private URLs.
    params.playgoScenarioId = "0";
    params.releaseDate = params.packageType = params.packageSubType = "";
    params.packageSize = entry.size;
    int task = -1;
    int rc = bgft.registerPkg(&params, &task);
    if (static_cast<uint32_t>(rc) == 0x80990088 || static_cast<uint32_t>(rc) == 0x80990015)
        throw std::runtime_error("Installed title conflict (" + hexCode(rc) + "). Existing title was kept.");
    if (static_cast<uint32_t>(rc) == 0x80990086)
        throw std::runtime_error("Already queued. Check PS4 Notifications > Downloads.");
    check(rc, "Register package");
    if (task < 0) throw std::runtime_error("BGFT returned an invalid task ID");
    rc = bgft.start(task);
    if (rc) {
        int cleanup = bgft.unregisterTask(task);
        throw std::runtime_error("Start task: " + hexCode(rc) +
            (cleanup ? "; remove task in PS4 Notifications > Downloads" : "; registration removed"));
    }
    return task;
}

TaskProgress taskProgress(int id) {
    Progress p{};
    int result = installer().progress(id, &p);
    return {p.transferredTotal, p.lengthTotal, p.error, result == 0};
}

}
