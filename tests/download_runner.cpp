#include "download.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
std::string mode, directory, url;
int remoteCalls = 0, localCalls = 0, updates = 0;
std::atomic<bool> cancelled{false};
}
namespace psgi {
uint64_t freeSpace(const std::string&) { return mode == "space" ? 1024 : 1000000000; }
int queueInstall(const Entry& e, int user) { assert(user == 123 && e.url == url); ++remoteCalls; return 91; }
int queueLocalInstall(const Entry& e, const std::string& path, int user) {
    assert(user == 123 && e.url == url && path == stagedPackagePath(directory, url));
    const auto size = std::filesystem::file_size(path); assert(size == 1024 * 1024 + 4);
    std::ifstream file(path, std::ios::binary); std::string magic(4, '\0'); file.read(&magic[0], 4);
    assert(magic == std::string("\x7f" "CNT", 4));
    char chunk[8192]; size_t read = 4;
    while (file) { file.read(chunk, sizeof(chunk)); const auto n = file.gcount();
        for (int i = 0; i < n; ++i) assert(chunk[i] == char((read + i) % 251));
        read += n;
    }
    assert(read == size); ++localCalls; return 92;
}
}
int main(int argc, char** argv) {
    assert(argc == 4); url = argv[1]; directory = argv[2]; mode = argv[3];
    psgi::Entry entry; entry.name = "Test package"; entry.url = url;
    const auto path = psgi::stagedPackagePath(directory, url);
    try {
        const int task = psgi::installPackage(entry, 123, directory, &cancelled, [&](uint64_t bytes, uint64_t total) {
            ++updates; assert(bytes <= total);
            if (mode == "cancel" && bytes) cancelled = true;
        });
        assert(mode == "range" || mode == "local");
        if (mode == "range") assert(task == 91 && remoteCalls == 1 && localCalls == 0 && updates == 0);
        else {
            assert(task == 92 && localCalls == 1 && remoteCalls == 0 && updates > 1);
            assert(psgi::installPackage(entry, 123, directory, &cancelled, [](uint64_t, uint64_t) {}) == 92);
            assert(localCalls == 2); // A valid staged file survives a registration retry.
            psgi::removeStagedPackage(directory, url); assert(!std::filesystem::exists(path));
        }
    } catch (const std::exception& e) {
        const std::string message = e.what(); std::cout << message << '\n';
        if (mode == "missing") assert(message.find("HTTP 404") != std::string::npos);
        else if (mode == "html") assert(message.find("PS4 PKG") != std::string::npos);
        else if (mode == "cancel") assert(message.find("cancelled") != std::string::npos);
        else if (mode == "space") assert(message.find("Not enough internal") != std::string::npos);
        else if (mode == "unknown-length") assert(message.find("Content-Length") != std::string::npos);
        else if (mode == "truncated") assert(message.find("transfer failed") != std::string::npos);
        else throw;
        assert(remoteCalls == 0 && localCalls == 0);
        assert(!std::filesystem::exists(path) && !std::filesystem::exists(path + ".part"));
    }
}
