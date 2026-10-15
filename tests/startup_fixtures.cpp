// Test-only console boundary: hide /app0-equivalent files at privilege setup.
// The executable otherwise uses the real main, application, UI and renderer.
#include "video.hpp"
#include "platform.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace psgi {
namespace {
std::chrono::steady_clock::time_point firstFrame;
bool presented = false, dashboardSeen = false, videoClosed = false;
}
Video::Video(bool) {}
Video::~Video() { videoClosed = true; std::puts("VIDEO_CLOSED"); }
void Video::present(const Canvas&) {
    if (!presented) { firstFrame = std::chrono::steady_clock::now(); presented = true; }
}
void Video::startKeyboard(Ui&) {}
void Video::pollKeyboard(Ui&) {}
void Video::flushInput() {}
std::vector<Button> Video::input(Ui& ui) {
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - firstFrame).count();
    if (ui.screen == Screen::Dashboard && !dashboardSeen) {
        dashboardSeen = true;
        std::printf("DASHBOARD_AFTER_SECONDS=%.6f\n", elapsed);
    }
    // Startup input must not open an exit dialog before the dashboard is ready.
    return {Button::Circle};
}
void prepareConsole() {
    if (!presented) throw std::runtime_error("Privilege setup ran before the first frame");
    std::filesystem::rename("assets", "sandbox-assets-hidden");
    std::puts("SANDBOX_ASSETS_HIDDEN");
    if (std::getenv("PSGI_TEST_STARTUP_FAILURE")) throw std::runtime_error("Simulated privilege setup failure");
}
int finishApplication(int status) {
    if (!videoClosed) throw std::runtime_error("Exit requested before video cleanup");
    std::printf("APPLICATION_FINISHED=%d\n", status);
    return status;
}
int queueInstall(const Entry&, int) { throw std::runtime_error("PS4 required"); }
int queueLocalInstall(const Entry&, const std::string&, int) { throw std::runtime_error("PS4 required"); }
bool retireInactiveTask(int) { return false; }
TaskProgress taskProgress(int) { return {0, 0, 0, false}; }
std::vector<Entry> scanInstalled() { return {}; }
void launchTitle(const std::string&) { throw std::runtime_error("PS4 required"); }
uint64_t freeSpace(const std::string&) { return 0; }
}
