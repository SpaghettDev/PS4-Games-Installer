#include "platform.hpp"
#include "library.hpp"
#include "storage.hpp"
#include "package.hpp"
#include "sfo.hpp"
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#ifdef PSGI_EMULATION_MODE
#include <orbis/Bgft.h>
#endif

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
#ifndef PSGI_EMULATION_MODE
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
#else
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT), "Load Bgft module via sysmodule");

        registerPkg = [](DownloadParams* dp, int* id) {
            return sceBgftServiceIntDebugDownloadRegisterPkg(reinterpret_cast<OrbisBgftDownloadParam*>(dp), static_cast<OrbisBgftTaskId*>(id));
        };
        registerLocal = [](StorageDownloadParams* sdp, int* id) {
            return sceBgftServiceIntDownloadRegisterTaskByStorageEx(reinterpret_cast<OrbisBgftDownloadParamEx*>(sdp), static_cast<OrbisBgftTaskId*>(id));
        };
        start = sceBgftServiceDownloadStartTask;
        unregisterTask = sceBgftServiceIntDownloadUnregisterTask;
        progress = [](int id, Progress* p) {
            return sceBgftServiceDownloadGetProgress(id, reinterpret_cast<OrbisBgftTaskProgress*>(p));
        };
        findTask = [](const char* name, int type, int* id) {
            return sceBgftServiceDownloadFindTaskByContentId(name, static_cast<OrbisBgftTaskSubType>(type), static_cast<OrbisBgftTaskId*>(id));
        };

        OrbisBgftInitParams params{ heap.data(), heap.size() };
        check(sceBgftServiceIntInit(&params), "BFGT init");
#endif
    }
};
Installer& installer() {
    // Intentionally retain the client heap until process exit; queued tasks belong to BGFT.
    static Installer instance;
    return instance;
}
std::mutex serviceMutex;
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
    StorageDownloadParams local{}; local.params = params;
    if (!localPath.empty() && packageKind(entry) == 1) {
        int module = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
        if (module < 0) check(module, "Load update slot service");
        auto initialize = symbol<int (*)()>(module, "sceAppInstUtilInitialize");
        check(initialize(), "Initialize update slot service");
        auto getSlot = symbol<int (*)(const char*, unsigned*)>(module, "sceAppInstUtilGetPrimaryAppSlot");
        check(getSlot(package->titleId.c_str(), &local.slot), "Read update application slot");
    }
    int task = -1, rc = 0;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        task = -1;
        rc = localPath.empty() ? bgft.registerPkg(&params, &task) : bgft.registerLocal(&local, &task);
        if (static_cast<uint32_t>(rc) == 0x80990088)
            throw SubmissionFailure("Installed title conflict (" + hexCode(rc) + "). Existing title was kept.", true);
        if (static_cast<uint32_t>(rc) == 0x80990086 || static_cast<uint32_t>(rc) == 0x80990015) {
            std::string message = "PS4 refused installation (" + hexCode(rc) + "). ";
            const std::string content = package ? package->contentId : entry.contentId;
            const int subtype = package && package->type == 0x1b ? 7 : 6;
            int found = -1;
            if (bgft.findTask && !content.empty() && bgft.findTask(content.c_str(), subtype, &found) == 0 && found >= 0) {
                Progress state{};
                const bool failed = !bgft.progress(found, &state) && state.error;
                message += "Existing download task " + std::to_string(found);
                if (failed) message += " failed: " + hexCode(state.error);
                // A validated local PKG is our identity authority. Recover a failed
                // exact-content task left by an earlier installer, without adopting
                // it as owned or stopping an active download. Recheck immediately
                // before release, then register once more with the same local file.
                if (!attempt && package && failed) {
                    int current = -1;
                    Progress latest{};
                    if (!bgft.findTask(content.c_str(), subtype, &current) && current == found &&
                        !bgft.progress(current, &latest) && latest.error) {
                        const int cleanup = bgft.unregisterTask(current);
                        if (cleanup) throw SubmissionFailure(message + ". Could not release the failed matching task: " + hexCode(cleanup) +
                            ". Cancel that title in PS4 Downloads, then retry. Downloaded package kept.", false);
                        continue;
                    }
                }
                message += ". Finish or cancel that title in PS4 Notifications > Downloads, then retry.";
            } else message += "No matching task could be confirmed. Close the app, restart the PS4, enable GoldHEN and retry.";
            if (package) message += " Downloaded package kept.";
            throw SubmissionFailure(message, false);
        }
        if (rc) throw SubmissionFailure(std::string(localPath.empty() ? "Register remote package" : "Register local package " + package->titleId + " at " + localPath) + ": " + hexCode(rc), true);
        break;
    }
    if (task < 0) throw std::runtime_error("BGFT returned an invalid task ID");
    rc = bgft.start(task);
    if (rc) {
        int cleanup = bgft.unregisterTask(task);
        throw SubmissionFailure("Start task: " + hexCode(rc) +
            (cleanup ? "; remove task in PS4 Notifications > Downloads" : "; registration removed"), !cleanup, cleanup ? task : -1);
    }
    return task;
}
}
int queueInstall(const Entry& entry, int userId) { std::lock_guard<std::mutex> lock(serviceMutex); return submit(entry, userId, ""); }
int queueLocalInstall(const Entry& entry, const std::string& path, int userId) {
    std::lock_guard<std::mutex> lock(serviceMutex);
    const auto systemPath = storagePackagePath(path);
    const auto package = inspectPackage(path);
    if (!entry.titleId.empty() && entry.titleId != package.titleId)
        throw std::runtime_error("Downloaded package Title ID does not match the selected game");
    return submit(entry, userId, systemPath, &package);
}
void cancelInstallTask(int taskId) {
    std::lock_guard<std::mutex> lock(serviceMutex);
    if (taskId < 0) throw std::runtime_error("Invalid owned installation task");
    auto& bgft = installer();
    Progress state{};
    const int status = bgft.progress(taskId, &state);
    if (static_cast<uint32_t>(status) == 0x80990019u) return;
    check(status, "Check task before cancellation");
    const uint64_t total = state.lengthTotal ? state.lengthTotal : state.length;
    const uint64_t done = state.lengthTotal ? state.transferredTotal : state.transferred;
    if (state.error || (total && done >= total && state.preparingPercent >= 100 && state.localCopyPercent >= 100)) {
        check(bgft.unregisterTask(taskId), "Release inactive owned task; staging file was kept"); return;
    }
    const int module = sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, nullptr, 0, nullptr, nullptr);
    if (module < 0) check(module, "Load cancellation service");
    auto stop = symbol<int (*)(int)>(module, "sceBgftServiceDownloadStopTask");
    check(stop(taskId), "Stop owned installation task; check PS4 Downloads");
    check(bgft.unregisterTask(taskId), "Remove stopped task; staging file was kept");
}
bool retireInactiveTask(int taskId) {
    std::lock_guard<std::mutex> lock(serviceMutex);
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
    std::lock_guard<std::mutex> lock(serviceMutex);
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
