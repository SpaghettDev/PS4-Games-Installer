#pragma once
#include "canvas.hpp"
#include "ui.hpp"
#include "controller.hpp"
#ifndef PSGI_HOST
#include <orbis/VideoOut.h>
#endif

namespace psgi {
class Video {
#ifdef PSGI_HOST
    void *window = nullptr, *renderer = nullptr, *texture = nullptr, *controller = nullptr;
#else
    int video = -1, active = 0, pad = -1;
    int64_t frame = 1;
    off_t offset = 0;
    void* memory = nullptr;
    size_t allocation = 0;
    void* buffers[2] = {};
#endif
    ControllerInput controllerInput;
    bool keyboard = false;
public:
    int userId = -1;
    explicit Video(bool headless = false);
    ~Video();
    void present(const Canvas& canvas);
    std::vector<Button> input(Ui& ui);
    void startKeyboard(Ui& ui);
    void pollKeyboard(Ui& ui);
    void flushInput();
};
}
