#pragma once
#include "catalog.hpp"
#include <atomic>
#include <optional>

namespace psgi {
std::string httpGet(const std::string& url, size_t limit = MaxCatalogBytes, const std::atomic<bool>* cancelled = nullptr);
void prepareConsole();
int queueInstall(const Entry& entry, int userId);
struct TaskProgress {
    uint64_t transferred = 0, total = 0;
    int error = 0;
    bool found = true;
};
TaskProgress taskProgress(int taskId);
std::vector<Entry> scanInstalled();
void launchTitle(const std::string& titleId);
uint64_t freeSpace(const std::string& path);
}
