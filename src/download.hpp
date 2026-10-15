#pragma once
#include "platform.hpp"

namespace psgi {
using DownloadProgress = std::function<void(uint64_t, uint64_t)>;
// Probe the actual package, use BGFT for range-capable servers, otherwise stream
// to private staging storage and submit the complete local package to BGFT.
int installPackage(const Entry& entry, int userId, const std::string& dataDirectory,
                   const std::atomic<bool>* cancelled, const DownloadProgress& progress);
std::string stagedPackagePath(const std::string& dataDirectory, const std::string& url);
void removeStagedPackage(const std::string& dataDirectory, const std::string& url);
}
