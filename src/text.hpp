#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
namespace psgi {
inline std::u16string toUtf16(const std::string& text) {
    std::u16string out;
    for (size_t i = 0; i < text.size();) {
        uint32_t ch = static_cast<uint8_t>(text[i++]);
        if (ch >= 128) {
            unsigned count = ch >= 0xf0 && ch <= 0xf4 ? 3 : ch >= 0xe0 && ch <= 0xef ? 2 : ch >= 0xc2 && ch <= 0xdf ? 1 : 0;
            if (!count || i + count > text.size()) throw std::runtime_error("Invalid UTF-8 text");
            ch &= (1u << (6-count)) - 1;
            for (unsigned j = 0; j < count; ++j) {
                uint8_t next = text[i++];
                if ((next & 0xc0) != 0x80) throw std::runtime_error("Invalid UTF-8 text");
                ch = ch << 6 | (next & 63);
            }
            if (ch > 0x10ffff || (ch >= 0xd800 && ch <= 0xdfff) || ch < (count == 1 ? 0x80u : count == 2 ? 0x800u : 0x10000u)) throw std::runtime_error("Invalid UTF-8 text");
        }
        if (ch < 0x10000) out += static_cast<char16_t>(ch);
        else { ch -= 0x10000; out += static_cast<char16_t>(0xd800 | (ch >> 10)); out += static_cast<char16_t>(0xdc00 | (ch & 1023)); }
    }
    return out;
}
inline std::string fromUtf16(const char16_t* text, size_t size) {
    std::string out;
    for (size_t i = 0; i < size; ++i) {
        uint32_t ch = text[i];
        if (ch >= 0xd800 && ch <= 0xdbff) {
            if (++i >= size || text[i] < 0xdc00 || text[i] > 0xdfff) throw std::runtime_error("Invalid keyboard text");
            ch = 0x10000 + ((ch - 0xd800) << 10) + (text[i] - 0xdc00);
        } else if (ch >= 0xdc00 && ch <= 0xdfff) throw std::runtime_error("Invalid keyboard text");
        if (ch < 0x80) out += static_cast<char>(ch);
        else if (ch < 0x800) { out += static_cast<char>(0xc0 | (ch >> 6)); out += static_cast<char>(0x80 | (ch & 63)); }
        else {
            if (ch >= 0x10000) { out += static_cast<char>(0xf0 | (ch >> 18)); out += static_cast<char>(0x80 | ((ch >> 12) & 63)); }
            else out += static_cast<char>(0xe0 | (ch >> 12));
            out += static_cast<char>(0x80 | ((ch >> 6) & 63)); out += static_cast<char>(0x80 | (ch & 63));
        }
    }
    return out;
}
}
