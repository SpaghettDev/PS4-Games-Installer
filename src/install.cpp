#include "platform.hpp"
#include "storage.hpp"
#include "package.hpp"
#include "sfo.hpp"
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <orbis/libkernel.h>

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
struct StorageDownloadParams { DownloadParams params; unsigned slot; };
static_assert(sizeof(StorageDownloadParams) == 112, "BGFT local storage ABI");
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
    int (*registerLocal)(StorageDownloadParams*, int*) = nullptr;
    int (*start)(int) = nullptr;
    int (*unregisterTask)(int) = nullptr;
    int (*progress)(int, Progress*) = nullptr;
    int (*findTask)(const char*, int, int*) = nullptr;
    Installer() : heap(1024 * 1024) {
        int module = sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, nullptr, 0, nullptr, nullptr);
        if (module < 0) throw std::runtime_error("Load BGFT: " + hexCode(module));
        auto init = symbol<int (*)(InitParams*)>(module, "sceBgftServiceInit");
        registerPkg = symbol<decltype(registerPkg)>(module, "sceBgftServiceIntDebugDownloadRegisterPkg");
        registerLocal = symbol<decltype(registerLocal)>(module, "sceBgftServiceIntDownloadRegisterTaskByStorageEx");
        start = symbol<decltype(start)>(module, "sceBgftServiceDownloadStartTask");
        unregisterTask = symbol<decltype(unregisterTask)>(module, "sceBgftServiceIntDownloadUnregisterTask");
        progress = symbol<decltype(progress)>(module, "sceBgftServiceDownloadGetProgress");
        // Diagnostics must not make normal installation depend on this export.
        if (sceKernelDlsym(module, "sceBgftServiceDownloadFindTaskByContentId", reinterpret_cast<void**>(&findTask))) findTask = nullptr;
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
namespace {
int submit(const Entry& entry, int userId, const std::string& localPath, const PackageInfo* package = nullptr) {
    if (!isHttpUrl(entry.url) || entry.name.empty() || entry.name.size() >= 600)
        throw std::runtime_error("Invalid package metadata");
    const auto storage = scanInstallStorage();
    if (storage.destination == InstallDestination::External && !storage.external.available)
        throw std::runtime_error("Selected Extended Storage is unavailable; reconnect it or select Internal storage");
    auto& bgft = installer();
    DownloadParams params{};
    params.userId = userId;
    params.entitlementType = 5;
    params.id = package ? package->contentId.c_str() : "";
    params.contentUrl = localPath.empty() ? entry.url.c_str() : localPath.c_str();
    params.contentExUrl = "";
    params.contentName = entry.name.c_str();
    params.iconPath = localPath.empty() ? entry.coverUrl.c_str() : "";
    params.skuId = "";
    params.option = 0x10000; // Do not append CDN query parameters to private URLs.
    params.playgoScenarioId = "0";
    params.releaseDate = params.packageType = params.packageSubType = "";
    // Ask the installer to read package metadata rather than treating catalog
    // size estimates as authoritative package headers.
    params.packageSize = package ? package->size : 0;
    int task = -1;
    int rc;
    if (localPath.empty()) rc = bgft.registerPkg(&params, &task);
    else {
        StorageDownloadParams local{}; local.params = params;
        if (entry.category == "updates") {
            int module = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
            if (module < 0) check(module, "Load update slot service");
            auto initialize = symbol<int (*)()>(module, "sceAppInstUtilInitialize");
            check(initialize(), "Initialize update slot service");
            auto getSlot = symbol<int (*)(const char*, unsigned*)>(module, "sceAppInstUtilGetPrimaryAppSlot");
            check(getSlot((package ? package->titleId : entry.titleId).c_str(), &local.slot), "Read update application slot");
        }
        rc = bgft.registerLocal(&local, &task);
    }
    if (static_cast<uint32_t>(rc) == 0x80990088)
        throw std::runtime_error("Installed title conflict (" + hexCode(rc) + "). Existing title was kept.");
    if (static_cast<uint32_t>(rc) == 0x80990086 || static_cast<uint32_t>(rc) == 0x80990015) {
        std::string message = "PS4 refused installation (" + hexCode(rc) + "). ";
        const std::string content = package ? package->contentId : entry.contentId;
        int found = -1;
        if (bgft.findTask && !content.empty() && bgft.findTask(content.c_str(), package && package->type == 0x1b ? 7 : 6, &found) == 0 && found >= 0) {
            Progress state{};
            message += "Existing download task " + std::to_string(found);
            if (!bgft.progress(found, &state) && state.error) message += " failed: " + hexCode(state.error);
            message += ". Finish or cancel that title in PS4 Notifications > Downloads, then retry.";
        } else message += "No matching task could be confirmed. Close the app, restart the PS4, enable GoldHEN and retry.";
        if (package) message += " Downloaded package kept.";
        throw std::runtime_error(message);
    }
    if (rc) throw std::runtime_error(std::string(localPath.empty() ? "Register remote package" : "Register local package " + package->titleId + " at " + localPath) + ": " + hexCode(rc));
    if (task < 0) throw std::runtime_error("BGFT returned an invalid task ID");
    rc = bgft.start(task);
    if (rc) {
        int cleanup = bgft.unregisterTask(task);
        throw std::runtime_error("Start task: " + hexCode(rc) +
            (cleanup ? "; remove task in PS4 Notifications > Downloads" : "; registration removed"));
    }
    return task;
}
}
int queueInstall(const Entry& entry, int userId) { return submit(entry, userId, ""); }
int queueLocalInstall(const Entry& entry, const std::string& path, int userId) {
    const auto systemPath = storagePackagePath(path);
    const auto package = inspectPackage(path);
    if (!entry.titleId.empty() && entry.titleId != package.titleId)
        throw std::runtime_error("Downloaded package Title ID does not match the selected game");
    return submit(entry, userId, systemPath, &package);
}
bool retireInactiveTask(int taskId) {
    Progress progress{};
    auto& bgft = installer();
    if (bgft.progress(taskId, &progress)) return false;
    const uint64_t total = progress.lengthTotal ? progress.lengthTotal : progress.length;
    const uint64_t done = progress.lengthTotal ? progress.transferredTotal : progress.transferred;
    const bool completed = total && done >= total &&
        progress.preparingPercent >= 100 && progress.localCopyPercent >= 100;
    if (!progress.error && !completed) return false;
    return bgft.unregisterTask(taskId) == 0;
}

TaskProgress taskProgress(int id) {
    Progress p{};
    int result = installer().progress(id, &p);
    // A service failure is not proof that the installer released a staging file.
    if (static_cast<uint32_t>(result) == 0x80990019u) return {0, 0, 0, false};
    check(result, "Read installation progress");
    TaskProgress out{p.lengthTotal ? p.transferredTotal : p.transferred, p.lengthTotal ? p.lengthTotal : p.length, p.error, true};
    out.completed = !p.error && out.total && out.transferred >= out.total &&
        p.preparingPercent >= 100 && p.localCopyPercent >= 100;
    return out;
}

}
