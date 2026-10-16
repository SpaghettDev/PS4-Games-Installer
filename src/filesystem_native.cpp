#include "storage.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

// The SDK statvfs implementation does not populate its output. Read the kernel
// statfs prefix through its exported descriptor API instead. Oversized backing
// storage accommodates both short and long mount-name ABI variants.
extern "C" int psgiFstatfs(int fd, void* result) __asm__("_fstatfs");
namespace psgi {
StorageSpace filesystemSpace(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return {};
    std::array<uint64_t, 512> raw{};
    const int rc = psgiFstatfs(fd, raw.data());
    close(fd);
    if (rc) return {true, 0, false};
    struct Prefix {
        uint32_t version, type;
        uint64_t flags, blockSize, ioSize, blocks, freeBlocks;
        int64_t availableBlocks;
    } info{};
    static_assert(offsetof(Prefix, blockSize) == 16 && offsetof(Prefix, availableBlocks) == 48, "Kernel statfs prefix ABI");
    std::memcpy(&info, raw.data(), sizeof(info));
    if (info.flags & 1) return {}; // Read-only mount.
    if (!info.blockSize || info.availableBlocks < 0 || uint64_t(info.availableBlocks) > UINT64_MAX / info.blockSize)
        return {true, 0, false};
    return {true, uint64_t(info.availableBlocks) * info.blockSize, true};
}
}
