#pragma once
#include "queue.hpp"
#include "storage.hpp"
#include <set>

namespace psgi {
struct StorageUsage {
    uint64_t artwork = 0, staging = 0, reclaimable = 0, protectedBytes = 0;
    size_t removableFiles = 0, protectedFiles = 0;
    bool checked = false;
};
struct CleanupResult { uint64_t bytes = 0; size_t files = 0, failed = 0; };
// Unmarked legacy packages and in-use leases remain protected after a crash.
StorageUsage scanStorageUsage(const std::string& directory, const std::set<std::string>& protectedPaths, bool trackingTrusted);
CleanupResult cleanStaging(const std::string& directory, const std::set<std::string>& protectedPaths, bool trackingTrusted);
struct DiagnosticSnapshot {
    unsigned revision = 0;
    size_t installed = 0, available = 0;
    InstallStorage storage;
    StorageUsage usage;
    std::vector<QueueItem> queue;
    std::string logTail;
};
std::string diagnosticEvent(const std::string& message);
std::string diagnosticReport(const DiagnosticSnapshot& snapshot, uint64_t now);
std::string exportDiagnostics(const std::string& device, const DiagnosticSnapshot& snapshot, uint64_t now);
}
