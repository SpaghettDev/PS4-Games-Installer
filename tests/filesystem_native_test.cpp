#include "storage.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace { std::string mode; int lastFd = -1; }
extern "C" int _fstatfs(int fd, void* raw) {
    assert(fcntl(fd, F_GETFD) >= 0); lastFd = fd;
    if (mode == "error") return -1;
    if (mode == "empty") return 0;
    auto* bytes = static_cast<unsigned char*>(raw);
    std::memset(raw, 0xab, 4096); // Check the backing buffer also accommodates a long kernel result.
    const uint64_t flags = mode == "readonly" ? 1 : 0;
    const uint64_t block = mode == "overflow" ? UINT64_MAX : 4096;
    const int64_t available = mode == "negative" ? -1 : mode == "full" ? 0 : 250000000;
    std::memcpy(bytes + 8, &flags, 8); std::memcpy(bytes + 16, &block, 8); std::memcpy(bytes + 48, &available, 8);
    return 0;
}
int main() {
    auto space = psgi::filesystemSpace("/tmp");
    assert(space.available && space.sizeKnown && space.freeBytes == 1024000000000ULL);
    assert(fcntl(lastFd, F_GETFD) == -1 && errno == EBADF);
    mode = "full"; space = psgi::filesystemSpace("/tmp"); assert(space.available && space.sizeKnown && space.freeBytes == 0);
    for (const auto* error : {"empty", "error", "negative", "overflow"}) {
        mode = error; space = psgi::filesystemSpace("/tmp"); assert(space.available && !space.sizeKnown);
    }
    mode = "readonly"; assert(!psgi::filesystemSpace("/tmp").available);
    assert(!psgi::filesystemSpace("/does-not-exist/psgi").available);
    std::puts("Native filesystem ABI, 64-bit capacity, full/unknown/read-only states and descriptor cleanup passed");
}
