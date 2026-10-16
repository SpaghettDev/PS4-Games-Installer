#pragma once
#include "catalog.hpp"
#include <atomic>
#include <optional>
#include <functional>

namespace psgi {
std::string httpGet(const std::string& url, size_t limit = MaxCatalogBytes, const std::atomic<bool>* cancelled = nullptr);
struct HttpTransferInfo { int status = 0; uint64_t length = 0; };
// Bounded-memory package transfer. Returning false from the sink stops a probe
// without downloading the remaining body. total is the response Content-Length.
using HttpSink = std::function<bool(const void*, size_t, uint64_t)>;
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                             const std::atomic<bool>* cancelled = nullptr);
void prepareConsole();
// Called after application resources and worker threads have been destroyed.
// Native builds hand termination to the PS4 shell; desktop builds return status.
int finishApplication(int status);
int queueInstall(const Entry& entry, int userId);
int queueLocalInstall(const Entry& entry, const std::string& path, int userId);
// Only call for failed/completed tasks owned/tracked by this application.
bool retireInactiveTask(int taskId);
struct TaskProgress {
    uint64_t transferred = 0, total = 0;
    int error = 0;
    bool found = true;
    bool retired = false;
    bool completed = false;
};
TaskProgress taskProgress(int taskId);
std::vector<Entry> scanInstalled();
void launchTitle(const std::string& titleId);
void uninstallTitle(const std::string& titleId);
uint64_t freeSpace(const std::string& path);
}
