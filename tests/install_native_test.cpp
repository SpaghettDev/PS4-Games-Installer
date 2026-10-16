#include "platform.hpp"
#include "storage.hpp"
#include "package.hpp"
#include <orbis/libkernel.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include "package_fixture.hpp"

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
int stops = 0;
std::string recovery;
int retryCalls = 0, lookupCalls = 0, progressCalls = 0, recoveryRemovals = 0;
int expectedSubtype = 6;
int recoveredTaskStarted = -1;

bool local = false;
bool update = false;
std::string expectedContent, expectedLookupContent;
int init(Init* p) { assert(p->heap && p->size == 1024 * 1024); return 0; }
int registerPkg(Params* p, int* task) {
    ++registrations;
    assert(p->user == 123 && p->entitlement == 5 && p->option == 0x10000);
    assert(p->size == (local ? 16384u : 0u) && std::strcmp(p->url, expectedUrl.c_str()) == 0);
    assert(std::strcmp(p->id, expectedContent.c_str()) == 0);
    assert(std::strcmp(p->icon, expectedIcon.c_str()) == 0 && std::strcmp(p->name, "Test game") == 0);
    *task = 45;
    if (!recovery.empty()) {
        ++retryCalls;
        if (retryCalls == 1 || recovery == "repeat-conflict" || recovery == "cleanup-error" || recovery == "active" || recovery == "lookup-error" || recovery == "progress-error" || recovery == "changed-task" || recovery == "resumed" || recovery == "recheck-error")
            return static_cast<int32_t>(recovery == "alternate-code" ? 0x80990015u : 0x80990086u);
        if (recovery == "retry-error") return -9;
        *task = 46; return 0;
    }
    if (mode == "conflict") return static_cast<int32_t>(0x80990088u);
    if (mode == "duplicate" || mode == "duplicate-found") return static_cast<int32_t>(0x80990086u);
    return 0;
}
int registerLocal(Local* p, int* task) { assert(local && p->slot == (update ? 3u : 0u)); return registerPkg(&p->params, task); }
int initApp() { return 0; }
int getSlot(const char* id, unsigned* slot) { assert(std::strcmp(id, "CUSA00001") == 0); *slot = 3; return 0; }
int start(int task) { if (!recovery.empty()) { assert(task == 46); recoveredTaskStarted = task; return recovery == "retry-start-error" || recovery == "retry-start-retained" ? -3 : 0; } assert(task == 45); ++starts; return mode == "start-error" || mode == "start-retained" ? -3 : 0; }
int unregisterTask(int task) { if (!recovery.empty()) { assert(task == 45 || (task == 46 && (recovery == "retry-start-error" || recovery == "retry-start-retained"))); ++recoveryRemovals; return recovery == "cleanup-error" || (task == 46 && recovery == "retry-start-retained") ? -4 : 0; } assert(task == 45); ++removals; return mode == "remove-error" || mode == "start-retained" ? -4 : 0; }
int stop(int task) { assert(task == 45); ++stops; return mode == "stop-error" ? -6 : 0; }
int progress(int task, Progress* p) {
    assert(task == 45); *p = {};
    if (!recovery.empty()) {
        ++progressCalls;
        if (recovery == "progress-error" || (recovery == "recheck-error" && progressCalls > 1)) return -5;
        if (recovery != "active" && !(recovery == "resumed" && progressCalls > 1)) p->error = static_cast<int32_t>(0x80991404u);
        return 0;
    }
    if (mode == "missing") return static_cast<int32_t>(0x80990019u);
    if (mode == "progress-error") return -5;
    if (mode == "failed" || mode == "remove-error") p->error = static_cast<int32_t>(0x80991404u);
    p->total = 100; p->transferred = mode == "active" ? 50 : 100;
    p->preparing = 100; p->copy = mode == "complete" ? 100 : 0;
    if (mode == "local-complete") { p->total = p->transferred = 0; p->length = p->done = 100; p->preparing = p->copy = 100; }
    return 0;
}
int findTask(const char* content, int subtype, int* task) {
    assert(std::strcmp(content, (expectedLookupContent.empty() ? expectedContent : expectedLookupContent).c_str()) == 0 && subtype == expectedSubtype);
    if (!recovery.empty()) {
        ++lookupCalls;
        if (recovery == "lookup-error") { *task = -1; return -5; }
        *task = recovery == "changed-task" && lookupCalls > 1 ? 47 : 45;
        return 0;
    }
    *task = mode == "duplicate-found" ? 45 : -1; return *task < 0 ? static_cast<int32_t>(0x80990019u) : 0;
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
    BIND("sceBgftServiceDownloadStopTask", stop)
    BIND("sceBgftServiceIntDownloadUnregisterTask", unregisterTask)
    BIND("sceBgftServiceDownloadGetProgress", progress)
    BIND("sceBgftServiceDownloadFindTaskByContentId", findTask)
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
    mode = "duplicate"; rejects([&] { queueInstall(entry, 123); }, "0x80990086"); assert(removals == 0);
    mode = "start-error"; rejects([&] { queueInstall(entry, 123); }, "registration removed"); assert(removals == 1);
    mode.clear(); local = true; char temporary[] = "/tmp/psgi-package-XXXXXX"; assert(mkdtemp(temporary));
    expectedUrl = std::string(temporary) + "/test.pkg"; expectedIcon.clear();
    expectedContent = "IV0000-CUSA00001_00-1234567890123456"; writePackageFixture(expectedUrl);
    const auto info = inspectPackage(expectedUrl); assert(info.contentId == expectedContent && info.titleId == "CUSA00001" && info.size == 16384);
    assert(storagePackagePath("/data/PS4-Game-Installer/downloads/test.pkg") == "/user/data/PS4-Game-Installer/downloads/test.pkg");
    assert(storagePackagePath("/user/data/test.pkg") == "/user/data/test.pkg");
    entry.titleId = "CUSA00001";
    assert(queueLocalInstall(entry, expectedUrl, 123) == 45);
    update = true; entry.category = "updates"; entry.titleId = "CUSA00001";
    assert(queueLocalInstall(entry, expectedUrl, 123) == 45); update = false; entry.category = "games";
    rejects([&] { queueLocalInstall(entry, "../test.pkg", 123); }, "Invalid local");
    mode = "duplicate"; rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "No matching task could be confirmed");
    mode = "duplicate-found"; rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "Existing download task 45");
    assert(removals == 1); // Diagnosing a foreign duplicate must never cancel it.
    entry.titleId = "CUSA00002"; rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "does not match"); entry.titleId = "CUSA00001";
    FILE* broken = std::fopen(expectedUrl.c_str(), "r+b"); assert(broken); std::fputc(0, broken); std::fclose(broken);
    rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "package header");
    writePackageFixture(expectedUrl); assert(!truncate(expectedUrl.c_str(), 2000));
    rejects([&] { inspectPackage(expectedUrl); }, "header size");
    // File length remains 64-bit and does not depend on the SDK stat ABI.
    writePackageFixture(expectedUrl, 5ULL * 1024 * 1024 * 1024); assert(inspectPackage(expectedUrl).size == 5ULL * 1024 * 1024 * 1024);
    unlink(expectedUrl.c_str()); rmdir(temporary);
    mode = "active"; assert(!taskProgress(45).completed && !retireInactiveTask(45) && removals == 1);
    mode.clear(); assert(!retireInactiveTask(45)); // 100% transferred does not prove installation finished.
    mode = "failed"; assert(taskProgress(45).error && retireInactiveTask(45) && removals == 2);
    mode = "remove-error"; assert(!retireInactiveTask(45) && removals == 3);
    mode = "complete"; assert(taskProgress(45).completed && retireInactiveTask(45) && removals == 4);
    mode = "local-complete"; assert(taskProgress(45).completed && taskProgress(45).total == 100 && retireInactiveTask(45) && removals == 5);
    mode = "missing"; assert(!taskProgress(45).found && !retireInactiveTask(45));
    mode = "progress-error"; rejects([] { taskProgress(45); }, "Read installation progress"); assert(!retireInactiveTask(45));
    rejects([] { cancelInstallTask(-1); }, "Invalid owned");
    rejects([] { cancelInstallTask(45); }, "Check task before cancellation");
    const int before = removals;
    mode = "active"; cancelInstallTask(45); assert(stops == 1 && removals == before + 1);
    mode = "stop-error"; rejects([] { cancelInstallTask(45); }, "Stop owned"); assert(stops == 2 && removals == before + 1);
    mode = "remove-error"; rejects([] { cancelInstallTask(45); }, "Release inactive"); assert(removals == before + 2);
    mode = "missing"; cancelInstallTask(45); assert(removals == before + 2);
    local = false; expectedUrl = entry.url; expectedIcon = entry.coverUrl; expectedContent.clear();
    mode = "start-retained";
    bool retained = false;
    try { queueInstall(entry, 123); } catch (const SubmissionFailure& error) { retained = !error.storageReleased && error.retainedTask == 45; }
    assert(retained);
    mode = "start-error";
    bool released = false;
    try { queueInstall(entry, 123); } catch (const SubmissionFailure& error) { released = error.storageReleased && error.retainedTask < 0; }
    assert(released);
    // Reproduce an orphaned failed remote task blocking a complete local PKG.
    local = true; mode.clear(); expectedIcon.clear();
    char recoveryDir[] = "/tmp/psgi-recovery-XXXXXX"; assert(mkdtemp(recoveryDir));
    expectedUrl = std::string(recoveryDir) + "/test.pkg";
    expectedContent = "IV0000-CUSA00001_00-1234567890123456";
    writePackageFixture(expectedUrl);
    auto scenario = [&](const char* name) {
        recovery = name; retryCalls = lookupCalls = progressCalls = recoveryRemovals = 0; recoveredTaskStarted = -1;
    };
    for (const char* code : {"failed", "alternate-code"}) {
        scenario(code);
        assert(queueLocalInstall(entry, expectedUrl, 123) == 46);
        assert(retryCalls == 2 && recoveryRemovals == 1 && recoveredTaskStarted == 46 && stops == 2);
    }
    for (const char* unsafe : {"active", "lookup-error", "progress-error", "changed-task", "resumed", "recheck-error"}) {
        scenario(unsafe);
        rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "PS4 refused installation");
        assert(recoveryRemovals == 0 && retryCalls == 1 && recoveredTaskStarted == -1);
    }
    scenario("cleanup-error");
    rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "Could not release the failed matching task: 0xFFFFFFFC");
    assert(recoveryRemovals == 1 && retryCalls == 1 && recoveredTaskStarted == -1);
    scenario("repeat-conflict");
    rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "Downloaded package kept");
    assert(recoveryRemovals == 1 && retryCalls == 2 && recoveredTaskStarted == -1);
    scenario("retry-error");
    released = false;
    try { queueLocalInstall(entry, expectedUrl, 123); } catch (const SubmissionFailure& error) { released = error.storageReleased && error.retainedTask < 0; }
    assert(released && recoveryRemovals == 1 && retryCalls == 2);
    scenario("retry-start-error");
    rejects([&] { queueLocalInstall(entry, expectedUrl, 123); }, "registration removed");
    assert(recoveryRemovals == 2 && recoveredTaskStarted == 46);
    scenario("retry-start-retained"); retained = false;
    try { queueLocalInstall(entry, expectedUrl, 123); } catch (const SubmissionFailure& error) { retained = !error.storageReleased && error.retainedTask == 46; }
    assert(retained && recoveryRemovals == 2 && recoveredTaskStarted == 46);
    // Update packages use their native subtype and primary app slot on both registrations.
    writePackageFixture(expectedUrl);
    FILE* patch = std::fopen(expectedUrl.c_str(), "r+b"); assert(patch);
    assert(!fseeko(patch, 0x77, SEEK_SET) && std::fputc(0x1b, patch) != EOF && !std::fclose(patch));
    update = true; entry.category = "updates"; expectedSubtype = 7;
    scenario("failed"); assert(queueLocalInstall(entry, expectedUrl, 123) == 46 && recoveryRemovals == 1 && retryCalls == 2);
    update = false; entry.category = "games"; expectedSubtype = 6;
    // Remote metadata alone is insufficient authority to release someone else's task.
    local = false; expectedUrl = entry.url; expectedIcon = entry.coverUrl;
    scenario("failed"); expectedContent.clear(); expectedLookupContent = entry.contentId = "IV0000-CUSA00001_00-1234567890123456";
    rejects([&] { queueInstall(entry, 123); }, "Existing download task 45 failed: 0x80991404");
    assert(recoveryRemovals == 0 && retryCalls == 1);
    unlink((std::string(recoveryDir) + "/test.pkg").c_str()); rmdir(recoveryDir);
    std::puts("Failed exact-content local task recovery, active-task preservation, bounded retries and update slots passed");
    std::puts("BGFT remote/local registration, URL preservation, conflicts, retry cleanup and completion checks passed");
}
