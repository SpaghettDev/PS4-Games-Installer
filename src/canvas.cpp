#include "canvas.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#ifndef PSGI_HOST
#include <orbis/Sysmodule.h>
#endif

namespace psgi {
namespace {
uint32_t blend(uint32_t previous, uint32_t color, unsigned alpha) {
    if (alpha == 255) return 0xff000000 | color;
    uint32_t out = 0xff000000;
    for (unsigned shift : {0u, 8u, 16u})
        out |= ((((color >> shift) & 255) * alpha + ((previous >> shift) & 255) * (255 - alpha)) / 255) << shift;
    return out;
}
uint32_t nextCodepoint(const std::string& text, size_t& i) {
    uint32_t first = static_cast<uint8_t>(text[i++]);
    if (first < 128) return first < 32 ? ' ' : first;
    unsigned count = first >= 0xf0 && first <= 0xf4 ? 3 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xc2 && first <= 0xdf ? 1 : 0;
    uint32_t value = count ? first & ((1u << (6 - count)) - 1) : 0xfffd;
    if (!count || i + count > text.size()) return 0xfffd;
    for (unsigned j = 0; j < count; ++j) {
        uint8_t ch = text[i + j];
        if ((ch & 0xc0) != 0x80) return 0xfffd;
        value = value << 6 | (ch & 63);
    }
    i += count;
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) || value < (count == 1 ? 0x80u : count == 2 ? 0x800u : 0x10000u)) return 0xfffd;
    return value;
}
}
Canvas::Canvas(const std::string& font) : fontBytes(readFile(font)), pixels(Width * Height) {
#ifndef PSGI_HOST
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) throw std::runtime_error("Cannot load font module");
#endif
    if (FT_Init_FreeType(&library)) throw std::runtime_error("Cannot initialize font renderer");
    // Keep the bytes alive for the face: FreeType reads glyph data lazily.
    if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(fontBytes.data()),
                           static_cast<FT_Long>(fontBytes.size()), 0, &face)) {
        FT_Done_FreeType(library); library = nullptr;
        throw std::runtime_error("Cannot load bundled font");
    }
}
Canvas::~Canvas() {
    if (face) FT_Done_Face(face);
    if (library) FT_Done_FreeType(library);
}
const Canvas::Glyph& Canvas::glyph(uint32_t codepoint, int size) {
    uint64_t key = uint64_t(size) << 32 | codepoint;
    auto found = glyphs.find(key);
    if (found != glyphs.end()) return found->second;
    if (glyphs.size() > 2048) glyphs.clear();
    FT_Set_Pixel_Sizes(face, 0, size);
    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) FT_Load_Char(face, '?', FT_LOAD_RENDER);
    auto g = face->glyph;
    Glyph out{static_cast<int>(g->bitmap.width), static_cast<int>(g->bitmap.rows), g->bitmap_left, g->bitmap_top, static_cast<int>(g->advance.x >> 6), {}};
    out.alpha.resize(out.width * out.height);
    for (int row = 0; row < out.height; ++row) {
        const uint8_t* line = g->bitmap.pitch >= 0 ? g->bitmap.buffer + row * g->bitmap.pitch : g->bitmap.buffer + (out.height - row - 1) * (-g->bitmap.pitch);
        for (int col = 0; col < out.width; ++col) out.alpha[row * out.width + col] = line[col];
    }
    return glyphs.emplace(key, std::move(out)).first->second;
}
void Canvas::rectangle(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || x >= Width || y >= Height || x + w <= 0 || y + h <= 0) return;
    int right = std::min(Width, x + w), bottom = std::min(Height, y + h);
    for (int row = std::max(0, y); row < bottom; ++row) {
        auto* target = pixels.data() + row * Width;
        if (alpha == 255) std::fill(target + std::max(0, x), target + std::max(0, right), 0xff000000 | color);
        else for (int col = std::max(0, x); col < right; ++col) target[col] = blend(target[col], color, alpha);
    }
}
void Canvas::rounded(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha) {
    radius = std::min(radius, std::min(w, h) / 2);
    rectangle(x + radius, y, w - 2 * radius, h, color, alpha);
    rectangle(x, y + radius, radius, h - radius * 2, color, alpha);
    rectangle(x + w - radius, y + radius, radius, h - radius * 2, color, alpha);
    for (int dy = 0; dy < radius; ++dy) {
        int offset = radius - static_cast<int>(std::sqrt(radius * radius - (radius - dy - 1) * (radius - dy - 1)));
        rectangle(x + offset, y + dy, radius - offset, 1, color, alpha);
        rectangle(x + w - radius, y + dy, radius - offset, 1, color, alpha);
        rectangle(x + offset, y + h - dy - 1, radius - offset, 1, color, alpha);
        rectangle(x + w - radius, y + h - dy - 1, radius - offset, 1, color, alpha);
    }
}
void Canvas::text(const std::string& value, int x, int baseline, int size, uint32_t color, int maxWidth, uint8_t opacity) {
    int pen = x, right = std::min(Width, x + maxWidth);
    for (size_t i = 0; i < value.size();) {
        auto& g = glyph(nextCodepoint(value, i), size);
        if (pen + g.advance > right) {
            auto& dot = glyph(0x2026, size);
            if (pen + dot.advance <= right) text("…", pen, baseline, size, color, right - pen, opacity);
            break;
        }
        for (int row = 0; row < g.height; ++row) {
            int dy = baseline - g.top + row;
            if (dy < 0 || dy >= Height) continue;
            for (int col = 0; col < g.width; ++col) {
                int dx = pen + g.left + col;
                if (dx < std::max(0, x) || dx >= right) continue;
                unsigned alpha = unsigned(g.alpha[row * g.width + col]) * opacity / 255;
                if (alpha) pixels[dy * Width + dx] = blend(pixels[dy * Width + dx], color, alpha);
            }
        }
        pen += g.advance;
    }
}
int Canvas::textWidth(const std::string& text, int size) {
    int result = 0;
    for (size_t i = 0; i < text.size();) result += glyph(nextCodepoint(text, i), size).advance;
    return result;
}
void Canvas::centered(const std::string& value, int baseline, int size, uint32_t color) {
    text(value, std::max(0, (Width - textWidth(value, size)) / 2), baseline, size, color);
}
void Canvas::image(const Image& source, int x, int y, int w, int h, uint8_t opacity, bool cover) {
    if (source.width <= 0 || source.height <= 0 || w <= 0 || h <= 0) return;
    double scale = cover ? std::max(double(w) / source.width, double(h) / source.height) : std::min(double(w) / source.width, double(h) / source.height);
    int dw = std::max(1, int(source.width * scale)), dh = std::max(1, int(source.height * scale));
    int left = x + (w - dw) / 2, top = y + (h - dh) / 2;
    int beginX = std::max({0, x, left}), endX = std::min({Width, x + w, left + dw});
    int beginY = std::max({0, y, top}), endY = std::min({Height, y + h, top + dh});
    std::vector<int> indices(std::max(0, endX - beginX));
    for (int xx = beginX; xx < endX; ++xx) indices[xx - beginX] = (xx - left) * source.width / dw;
    for (int yy = beginY; yy < endY; ++yy) {
        const auto* input = source.pixels.data() + ((yy - top) * source.height / dh) * source.width;
        auto* output = pixels.data() + yy * Width;
        for (int xx = beginX; xx < endX; ++xx) {
            uint32_t color = input[indices[xx - beginX]];
            unsigned alpha = (color >> 24) * opacity / 255;
            output[xx] = blend(output[xx], color & 0xffffff, alpha);
        }
    }
}
void Canvas::background(double time, bool animate) {
    backgroundBase();
    bubbles(time, animate);
}
void Canvas::backgroundBase() {
    for (int y = 0; y < Height; ++y) {
        double f = double(y) / Height;
        uint32_t color = (uint32_t(9 + 5 * f) << 16) | (uint32_t(51 - 25 * f) << 8) | uint32_t(94 - 35 * f);
        std::fill(pixels.begin() + y * Width, pixels.begin() + (y+1) * Width, color | 0xff000000);
    }
}
void Canvas::bubbles(double time, bool animate) {
    // Cache the soft rims and highlights once; the CPU renderer only composites
    // small sprites per frame, with no per-pixel square roots during animation.
    static const auto sprites = [] {
        std::array<Image, 6> result;
        const int radii[] = {18, 28, 40, 54, 70, 88};
        for (size_t i = 0; i < result.size(); ++i) {
            auto& sprite = result[i];
            const int radius = radii[i], center = radius + 7;
            sprite.width = sprite.height = center * 2 + 1;
            sprite.pixels.resize(sprite.width * sprite.height);
            for (int y = 0; y < sprite.height; ++y) for (int x = 0; x < sprite.width; ++x) {
                const double dx = x - center, dy = y - center;
                const double distance = std::sqrt(dx * dx + dy * dy);
                const double rim = std::max(0.0, 1.0 - std::abs(distance - radius) / 1.8);
                const double halo = std::max(0.0, 1.0 - std::abs(distance - radius) / 6.0);
                const double highlight = std::clamp(.5 - (dx + dy) / (2 * radius), 0.0, 1.0);
                const unsigned alpha = unsigned((distance < radius ? 10 : 0) + halo * 10 + rim * (58 + 48 * highlight));
                sprite.pixels[y * sprite.width + x] = (alpha << 24) | 0xb2e7ff;
            }
        }
        return result;
    }();
    if (!animate) time = 0;
    for (int i = 0; i < 28; ++i) {
        const auto& sprite = sprites[i % sprites.size()];
        const int extent = sprite.width / 2;
        const int cx = 64 + i * 317 % (Width - 128) + int(std::sin(time * .19 + i * 1.17) * 42);
        const int cy = Height + extent - int(std::fmod(i * 173.0 + time * (18 + i % 7 * 4), Height + extent * 2));
        image(sprite, cx - extent, cy - extent, sprite.width, sprite.height);
    }
}
void Canvas::writePpm(const std::string& path) const {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) throw std::runtime_error("Cannot write preview");
    std::fprintf(file, "P6\n%d %d\n255\n", Width, Height);
    for (uint32_t color : pixels) {
        unsigned char rgb[] = {static_cast<unsigned char>(color >> 16), static_cast<unsigned char>(color >> 8), static_cast<unsigned char>(color)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
}
}
