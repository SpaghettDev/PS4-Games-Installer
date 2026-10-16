#include "package.hpp"
#include "sfo.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace psgi {
PackageInfo inspectPackage(const std::string& path) {
    auto close = [](FILE* file) { std::fclose(file); };
    std::unique_ptr<FILE, decltype(close)> file(std::fopen(path.c_str(), "rb"), close);
    if (!file) throw std::runtime_error("Cannot open staged package");
    // The pinned SDK stat structure disagrees with the kernel's st_size offset.
    // Seeking the descriptor reads the file length without that ABI dependency.
    if (fseeko(file.get(), 0, SEEK_END)) throw std::runtime_error("Cannot seek staged package");
    const auto length = ftello(file.get());
    if (length < 0 || fseeko(file.get(), 0, SEEK_SET)) throw std::runtime_error("Cannot read staged package size");
    std::array<unsigned char, 0x440> header{};
    if (std::fread(header.data(), 1, header.size(), file.get()) != header.size() ||
        header[0] != 0x7f || header[1] != 'C' || header[2] != 'N' || header[3] != 'T')
        throw std::runtime_error("Invalid staged PS4 package header");
    auto number = [&](size_t offset, size_t bytes) {
        uint64_t value = 0;
        for (size_t i = 0; i < bytes; ++i) value = value * 256 + header[offset + i];
        return value;
    };
    PackageInfo info;
    const char* id = reinterpret_cast<const char*>(header.data() + 0x40);
    size_t count = 0; while (count < 36 && id[count]) ++count;
    info.contentId.assign(id, count);
    if (count != 36 || header[0x64] != 0 || info.contentId[6] != '-' || info.contentId[16] != '_' || info.contentId[19] != '-' ||
        info.contentId.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::runtime_error("Invalid package Content ID");
    info.titleId = info.contentId.substr(7, 9);
    if (!validTitleId(info.titleId)) throw std::runtime_error("Invalid package Title ID");
    info.size = number(0x430, 8); info.type = number(0x74, 4);
    if (!info.size || info.size != uint64_t(length)) throw std::runtime_error("Package header size does not match downloaded file");
    return info;
}
std::string storagePackagePath(const std::string& path) {
    if (path.empty() || path.front() != '/' || path.find_first_of("\\\r\n") != std::string::npos ||
        path.find("/../") != std::string::npos || path.find("/./") != std::string::npos ||
        path.size() < 5 || path.substr(path.size() - 4) != ".pkg")
        throw std::runtime_error("Invalid local package path");
    // BGFT runs in the system namespace, outside this application's /data mount.
    return path.compare(0, 6, "/data/") == 0 ? "/user" + path : path;
}
}
