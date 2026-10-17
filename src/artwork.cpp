#include "artwork.hpp"
#include "platform.hpp"
#include "settings.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 2048
#include "stb_image.h"

namespace psgi {
namespace {
uint64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
std::string artworkCacheName(const std::string& key) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ULL; }
    char name[48]; std::snprintf(name, sizeof(name), "art-%016llx.img", static_cast<unsigned long long>(hash));
    return name;
}
std::string artworkKey(const Entry& e, bool fullSize) {
    const auto key=e.localIcon.empty() ? e.coverUrl : e.localIcon;
    return key.empty() ? "" : key+(fullSize ? "#screenshot" : "");
}
std::shared_ptr<Image> decodeImage(const std::string& bytes, int maximum) {
    if (maximum < 1 || maximum > 1920) throw std::runtime_error("Invalid artwork display size");
    if (bytes.empty() || bytes.size() > MaxCatalogBytes) throw std::runtime_error("Artwork exceeds download limit");
    int w = 0, h = 0, channels = 0;
    auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    if (!stbi_info_from_memory(data, bytes.size(), &w, &h, &channels) || w < 1 || h < 1 || w > 2048 || h > 2048)
        throw std::runtime_error("Invalid artwork dimensions");
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> decoded(stbi_load_from_memory(data, bytes.size(), &w, &h, &channels, 4), stbi_image_free);
    if (!decoded) throw std::runtime_error("Invalid PNG/JPEG artwork");
    auto out = std::make_shared<Image>();
    double scale = std::min(1.0, double(maximum) / std::max(w, h));
    out->width = std::max(1, static_cast<int>(w * scale));
    out->height = std::max(1, static_cast<int>(h * scale));
    out->pixels.resize(out->width * out->height);
    for (int y = 0; y < out->height; ++y) for (int x = 0; x < out->width; ++x) {
        const auto* p = decoded.get() + ((y * h / out->height) * w + (x * w / out->width)) * 4;
        out->pixels[y * out->width + x] = uint32_t(p[3]) << 24 | uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
    }
    return out;
}
std::shared_ptr<Image> loadImage(const std::string& path, int maximum) { return decodeImage(readFile(path),maximum); }
ArtworkCache::ArtworkCache(std::string path) : directory(std::move(path)) {
    if (mkdir(directory.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create artwork cache");
    worker = TaskThread([this] { run(); });
}
ArtworkCache::~ArtworkCache() {
    stopped = true; wake.notify_all(); if (worker.joinable()) worker.join();
}
std::shared_ptr<Image> ArtworkCache::request(const Entry& e, bool priority, bool fullSize) {
    std::string key = artworkKey(e,fullSize);
    if (key.empty()) return {};
    std::lock_guard<std::mutex> lock(mutex);
    auto hit = images.find(key);
    if (hit != images.end()) { hit->second.age = ++age; return hit->second.image; }
    auto failure = failed.find(key);
    if (failure != failed.end() && failure->second.retryAt > now()) return {};
    if (!queued.count(key)) {
        failed.erase(key);
        if (jobs.size() >= 32) { queued.erase(artworkKey(jobs.back().entry,jobs.back().fullSize)); jobs.pop_back(); }
        queued.insert(key);
        if (priority) jobs.push_front({e, generation, fullSize}); else jobs.push_back({e, generation, fullSize});
        wake.notify_one();
    }
    return {};
}
std::string ArtworkCache::error(const Entry& e, bool fullSize) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto failure=failed.find(artworkKey(e,fullSize));
    return failure==failed.end() ? "" : failure->second.message;
}
std::vector<std::string> ArtworkCache::takeMessages() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> result;result.swap(messages);return result;
}
void ArtworkCache::refresh(const Entry& e, bool fullSize) {
    std::lock_guard<std::mutex> lock(mutex);
    std::string key = artworkKey(e,fullSize);
    ++generation; jobs.clear(); queued.clear();
    images.erase(key); failed.erase(key);
    unlink((directory + "/" + artworkCacheName(key)).c_str());
}
void ArtworkCache::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    ++generation; images.clear(); failed.clear(); jobs.clear(); queued.clear();
    DIR* dir = opendir(directory.c_str());
    if (!dir) return;
    while (dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name.compare(0, 4, "art-") == 0 && name.size() == 24) unlink((directory + "/" + name).c_str());
    }
    closedir(dir);
}
void ArtworkCache::pruneDisk() {
    struct File { std::string path; uint64_t size; time_t modified; };
    std::vector<File> files;
    uint64_t total = 0;
    DIR* dir = opendir(directory.c_str());
    if (!dir) return;
    while (dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name.compare(0, 4, "art-") != 0 || name.size() != 24) continue;
        std::string path = directory + "/" + name;
        struct stat st{};
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            files.push_back({path, static_cast<uint64_t>(st.st_size), st.st_mtime}); total += st.st_size;
        }
    }
    closedir(dir);
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.modified < b.modified; });
    for (const auto& file : files) {
        if (total <= 128 * 1024 * 1024) break;
        if (!unlink(file.path.c_str())) total -= file.size;
    }
}
void ArtworkCache::run() {
    while (!stopped) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [&] { return stopped || !jobs.empty(); });
            if (stopped) break;
            job = jobs.front(); jobs.pop_front();
        }
        std::string key = artworkKey(job.entry,job.fullSize), path = directory + "/" + artworkCacheName(key);
        try {
            std::shared_ptr<Image> image;
            std::string downloaded;
            if (!job.entry.localIcon.empty()) {
                try { image = loadImage(job.entry.localIcon,job.fullSize ? 1920 : 384); } catch (const std::exception&) {}
            }
            if (!image) {
                try { image = loadImage(path,job.fullSize ? 1920 : 384); } catch (const std::exception&) { unlink(path.c_str()); }
                if (!image) { downloaded = httpGetArtwork(job.entry.coverUrl, MaxCatalogBytes, &stopped); image = decodeImage(downloaded,job.fullSize ? 1920 : 384); }
            }
            std::lock_guard<std::mutex> lock(mutex);
            if (job.generation == generation) {
                images[key] = {std::move(image), ++age};
                queued.erase(key);
                failed.erase(key);
                if(job.fullSize) {
                    if(messages.size()>=32)messages.erase(messages.begin());
                    messages.push_back("Screenshot "+artworkCacheName(key)+" ready ("+std::to_string(images[key].image->width)+"x"+std::to_string(images[key].image->height)+")");
                }
                if (!downloaded.empty()) {
                    try { atomicWrite(path, downloaded); pruneDisk(); } catch (const std::exception&) { /* Disk cache is optional. */ }
                }
                size_t decodedBytes=0;
                for(const auto& cached:images)decodedBytes+=cached.second.image->pixels.size()*sizeof(uint32_t);
                while (images.size() > 48 || decodedBytes > 64*1024*1024) {
                    auto oldest = std::min_element(images.begin(), images.end(), [](const auto& a, const auto& b) { return a.second.age < b.second.age; });
                    decodedBytes-=oldest->second.image->pixels.size()*sizeof(uint32_t);
                    images.erase(oldest);
                }
            }
        } catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(mutex);
            if (job.generation == generation) {
                const std::string message=std::string(error.what()).substr(0,256);
                failed[key] = {now() + 30,message}; queued.erase(key);
                if(messages.size()>=32)messages.erase(messages.begin());
                messages.push_back(std::string(job.fullSize ? "Screenshot " : "Artwork ")+artworkCacheName(key)+" failed: "+message);
            }
            while (failed.size() > 512) failed.erase(failed.begin());
        }
    }
}
}
