#pragma once
#include "artwork.hpp"
#include "ui.hpp"
#include <functional>
#include <memory>

namespace psgi {
class Video;
class App {
    struct Worker;
    std::unique_ptr<Worker> worker;
    std::string directory;
    Settings saved;
    bool demo = false, initialScan = true, bootReady = false, startRemote = false;
    bool quitting = false;
    double lastLocalScan = 0, lastPoll = 0;
    std::map<std::string, int> tasks;
    void refresh(bool remote);
    void log(const std::string& message);
    void saveTasks();
public:
    Ui ui;
    ArtworkCache artwork;
    App(const std::string& dataDirectory, bool preview = false);
    ~App();
    void tick(double elapsed, Video& video);
    bool running() const { return !quitting; }
};
std::vector<Entry> demoLibrary();
}
