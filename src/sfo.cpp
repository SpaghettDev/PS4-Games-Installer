#include "sfo.hpp"
#include <cstdint>
#include <stdexcept>

namespace psgi {
bool validTitleId(const std::string& id) {
    if (id.size() != 9) return false;
    for (char c : id) if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) return false;
    return id.compare(0, 4, "NPXS") != 0;
}
std::map<std::string, std::string> parseSfo(const std::string& bytes) {
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    auto u32 = [&](size_t p) -> uint32_t {
        if (p > bytes.size() || bytes.size() - p < 4) throw std::runtime_error("Truncated SFO");
        return data[p] | uint32_t(data[p+1]) << 8 | uint32_t(data[p+2]) << 16 | uint32_t(data[p+3]) << 24;
    };
    auto u16 = [&](size_t p) -> uint16_t {
        if (p > bytes.size() || bytes.size() - p < 2) throw std::runtime_error("Truncated SFO");
        return data[p] | uint16_t(data[p+1]) << 8;
    };
    if (bytes.size() < 20 || bytes.size() > 1024 * 1024 || u32(0) != 0x46535000)
        throw std::runtime_error("Invalid SFO");
    uint32_t keys = u32(8), values = u32(12), count = u32(16);
    if (count > 4096 || keys < 20 + count * 16 || values < keys || values > bytes.size()) throw std::runtime_error("Invalid SFO tables");
    std::map<std::string, std::string> result;
    for (uint32_t i = 0; i < count; ++i) {
        size_t p = 20 + i * 16, key = size_t(keys) + u16(p), value = size_t(values) + u32(p + 12);
        uint32_t length = u32(p + 4);
        if (key >= values || value > bytes.size() || length > bytes.size() - value) throw std::runtime_error("Invalid SFO entry");
        size_t end = bytes.find('\0', key);
        if (end == std::string::npos || end >= values) throw std::runtime_error("Invalid SFO key");
        std::string name = bytes.substr(key, end - key);
        if (u16(p + 2) == 0x0204) {
            size_t n = bytes.find('\0', value);
            if (n == std::string::npos || n >= value + length) throw std::runtime_error("Invalid SFO string");
            result[name] = bytes.substr(value, n - value);
        } else if (u16(p + 2) == 0x0404) {
            if (length < 4) throw std::runtime_error("Invalid SFO integer");
            result[name] = std::to_string(u32(value));
        }
    }
    return result;
}
}
