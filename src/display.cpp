#include "display.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

namespace psgi {
Display::Display() {
    video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, nullptr);
    if (video < 0) throw std::runtime_error("Video output could not be opened");
    constexpr size_t Alignment = 2 * 1024 * 1024;
    constexpr size_t BufferBytes = Width * Height * sizeof(uint32_t);
    allocation = (BufferBytes * 2 + Alignment - 1) & ~(Alignment - 1);
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), allocation, Alignment, 3, &offset) < 0)
        throw std::runtime_error("Display allocation failed");
    if (sceKernelMapDirectMemory(&memory, allocation, 0x33, 0, offset, Alignment) < 0)
        throw std::runtime_error("Display mapping failed");
    std::memset(memory, 0, allocation);
    buffers[0] = memory;
    buffers[1] = static_cast<char*>(memory) + BufferBytes;
    OrbisVideoOutBufferAttribute attr{};
    // B8G8R8A8, linear layout. Pixel words are 0xFFRRGGBB.
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, Width, Height, Width);
    if (sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr) < 0)
        throw std::runtime_error("Display buffer registration failed");
    sceVideoOutSetFlipRate(video, 0);
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0 || FT_Init_FreeType(&library))
        throw std::runtime_error("Font library init failed");
    if (FT_New_Face(library, "/app0/assets/font.ttf", 0, &face))
        throw std::runtime_error("Bundled font is missing");
}

Display::~Display() {
    if (face) FT_Done_Face(face);
    if (library) FT_Done_FreeType(library);
    if (video >= 0) sceVideoOutClose(video);
    if (memory) sceKernelMunmap(memory, allocation);
    if (allocation) sceKernelReleaseDirectMemory(offset, allocation);
}

void Display::rectangle(int x, int y, int w, int h, uint32_t color) {
    auto* pixels = static_cast<uint32_t*>(buffers[active]);
    int right = std::min(Width, x + w), bottom = std::min(Height, y + h);
    for (int row = std::max(0, y); row < bottom; ++row)
        for (int col = std::max(0, x); col < right; ++col) pixels[row * Width + col] = 0xff000000 | color;
}

void Display::text(const std::string& value, int x, int baseline, int size,
                   uint32_t color, int maxWidth) {
    FT_Set_Pixel_Sizes(face, 0, size);
    int pen = x, right = std::min(Width, x + maxWidth);
    auto* pixels = static_cast<uint32_t*>(buffers[active]);
    // Decode UTF-8 before looking up glyphs. Invalid bytes become replacement glyphs.
    for (size_t i = 0; i < value.size() && pen < right;) {
        uint32_t ch = static_cast<unsigned char>(value[i++]);
        if (ch >= 0x80) {
            unsigned count = ch >= 0xf0 && ch <= 0xf4 ? 3 : ch >= 0xe0 && ch < 0xf0 ? 2 : ch >= 0xc2 && ch < 0xe0 ? 1 : 0;
            uint32_t cp = count ? ch & ((1u << (6 - count)) - 1) : 0xfffd;
            bool valid = count && i + count <= value.size();
            for (unsigned j = 0; valid && j < count; ++j) {
                unsigned char next = value[i + j];
                if ((next & 0xc0) != 0x80) valid = false;
                else cp = (cp << 6) | (next & 0x3f);
            }
            if (valid) {
                i += count;
                valid = cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff) &&
                    cp >= (count == 1 ? 0x80u : count == 2 ? 0x800u : 0x10000u);
            }
            ch = valid ? cp : 0xfffd;
        }
        if (ch < 32 || ch == 127) ch = ' ';
        if (FT_Load_Char(face, ch, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot g = face->glyph;
        if (pen + (g->advance.x >> 6) > right) break;
        const FT_Bitmap& b = g->bitmap;
        for (int row = 0; row < b.rows; ++row) {
            int dy = baseline - g->bitmap_top + row;
            if (dy < 0 || dy >= Height) continue;
            const unsigned char* line = b.pitch >= 0 ? b.buffer + row * b.pitch : b.buffer + (b.rows - row - 1) * (-b.pitch);
            for (int col = 0; col < b.width; ++col) {
                int dx = pen + g->bitmap_left + col;
                if (dx < x || dx >= right) continue;
                unsigned alpha = line[col];
                uint32_t& pixel = pixels[dy * Width + dx];
                uint32_t blended = 0xff000000;
                for (unsigned shift : {0u, 8u, 16u}) {
                    unsigned channel = (((color >> shift) & 255) * alpha + ((pixel >> shift) & 255) * (255 - alpha)) / 255;
                    blended |= channel << shift;
                }
                pixel = blended;
            }
        }
        pen += g->advance.x >> 6;
    }
}

void Display::present() {
    if (sceVideoOutSubmitFlip(video, active, ORBIS_VIDEO_OUT_FLIP_VSYNC, frame) < 0)
        throw std::runtime_error("Display flip failed");
    OrbisVideoOutFlipStatus status{};
    const uint64_t deadline = sceKernelGetProcessTime() + 3000000;
    do {
        if (sceVideoOutGetFlipStatus(video, &status) < 0) throw std::runtime_error("Display status failed");
        if (status.flipArg == frame) break;
        if (sceKernelGetProcessTime() > deadline) throw std::runtime_error("Display flip timed out");
        sceKernelUsleep(1000);
    } while (true);
    active ^= 1;
    ++frame;
}
}
