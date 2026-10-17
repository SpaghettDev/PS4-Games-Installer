#pragma once
#include "catalog.hpp"
#include "task_thread.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace psgi {
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
std::shared_ptr<Image> decodeImage(const std::string& bytes, int maximum = 384);
std::shared_ptr<Image> loadImage(const std::string& path, int maximum = 384);
std::string artworkKey(const Entry& entry, bool fullSize = false);
std::string artworkCacheName(const std::string& key);
class ArtworkCache {
    struct Cached { std::shared_ptr<Image> image; uint64_t age = 0; };
    struct Job { Entry entry; uint64_t generation; bool fullSize = false; };
    std::string directory;
    std::mutex mutex;
    std::condition_variable wake;
    std::map<std::string, Cached> images;
    std::map<std::string, uint64_t> failed;
    std::deque<Job> jobs;
    std::set<std::string> queued;
    uint64_t age = 0, generation = 0;
    std::atomic<bool> stopped{false};
    TaskThread worker;
    void run();
    void pruneDisk();
public:
    explicit ArtworkCache(std::string cacheDirectory);
    ~ArtworkCache();
    std::shared_ptr<Image> request(const Entry& entry, bool priority = false, bool fullSize = false);
    void refresh(const Entry& entry, bool fullSize = false);
    void clear();
};
}
