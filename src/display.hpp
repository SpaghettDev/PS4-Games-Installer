#pragma once
#include <cstdint>
#include <string>
#include <orbis/VideoOut.h>
#include <proto-include.h>

namespace psgi {
class Display {
    int video = -1, active = 0;
    int64_t frame = 1;
    off_t offset = 0;
    void* memory = nullptr;
    size_t allocation = 0;
    void* buffers[2] = {};
    FT_Library library = nullptr;
    FT_Face face = nullptr;
public:
    static constexpr int Width = 1920, Height = 1080;
    Display();
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    void rectangle(int x, int y, int w, int h, uint32_t color);
    void text(const std::string& value, int x, int baseline, int size,
              uint32_t color, int maxWidth = Width);
    void present();
};
}
