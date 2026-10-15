#pragma once
#include "artwork.hpp"
#ifdef PSGI_HOST
#include <ft2build.h>
#include FT_FREETYPE_H
#else
#include <proto-include.h>
#endif

namespace psgi {
class Canvas {
    struct Glyph { int width, height, left, top, advance; std::vector<uint8_t> alpha; };
    std::string fontBytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    std::map<uint64_t, Glyph> glyphs;
    const Glyph& glyph(uint32_t codepoint, int size);
public:
    static constexpr int Width = 1920, Height = 1080;
    std::vector<uint32_t> pixels;
    explicit Canvas(const std::string& font);
    ~Canvas();
    Canvas(const Canvas&) = delete;
    void rectangle(int x, int y, int w, int h, uint32_t color, uint8_t alpha = 255);
    void rounded(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha = 255);
    void text(const std::string& value, int x, int baseline, int size, uint32_t color,
              int maxWidth = Width, uint8_t alpha = 255);
    int textWidth(const std::string& value, int size);
    void centered(const std::string& value, int baseline, int size, uint32_t color);
    void image(const Image& image, int x, int y, int w, int h, uint8_t alpha = 255, bool cover = false);
    void background(double time, bool animate);
    void backgroundBase();
    void bubbles(double time, bool animate);
    void writePpm(const std::string& path) const;
};
}
