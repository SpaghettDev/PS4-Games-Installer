#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

#ifdef PSGI_HOST
namespace psgi { using TaskThread = std::thread; }
#else
#include <orbis/libkernel.h>
namespace psgi {
// Console threads default to a small stack. HTTP, JSON and image decoding need
// explicit headroom, including calls inside system libraries.
class TaskThread {
    OrbisPthread handle{};
    bool active = false;
    static void* run(void* arg) {
        std::unique_ptr<std::function<void()>> job(static_cast<std::function<void()>*>(arg));
        (*job)(); return nullptr;
    }
public:
    TaskThread() = default;
    explicit TaskThread(std::function<void()> job) {
        auto owned = std::make_unique<std::function<void()>>(std::move(job));
        OrbisPthreadAttr attr{};
        if (scePthreadAttrInit(&attr)) throw std::runtime_error("Cannot initialize worker thread");
        int rc = scePthreadAttrSetstacksize(&attr, 512 * 1024);
        if (!rc) rc = scePthreadCreate(&handle, &attr, run, owned.get(), "psgi-worker");
        scePthreadAttrDestroy(&attr);
        if (rc) throw std::runtime_error("Cannot start worker thread");
        owned.release(); active = true;
    }
    TaskThread(TaskThread&& other) noexcept : handle(other.handle), active(other.active) { other.active = false; }
    TaskThread& operator=(TaskThread&& other) noexcept {
        if (active) std::terminate();
        handle = other.handle; active = other.active; other.active = false; return *this;
    }
    TaskThread(const TaskThread&) = delete;
    ~TaskThread() { if (active) std::terminate(); }
    bool joinable() const { return active; }
    void join() {
        if (!active || scePthreadJoin(handle, nullptr)) throw std::runtime_error("Cannot join worker thread");
        active = false;
    }
};
}
#endif
