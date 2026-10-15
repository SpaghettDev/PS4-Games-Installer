#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

inline std::string fixtureSfo(std::string title = "A game", std::string category = "gd", std::string titleId = "CUSA00001") {
    std::vector<std::pair<std::string, std::string>> fields{{"TITLE", title}, {"TITLE_ID", titleId}, {"CATEGORY", category}, {"APP_VER", "1.00"}};
    std::string keys, values;
    std::string out(20 + fields.size() * 16, '\0');
    auto u32 = [&](size_t offset, uint32_t n) { for (int i = 0; i < 4; ++i) out[offset + i] = n >> (i * 8); };
    u32(0, 0x46535000); u32(4, 0x101); u32(8, out.size()); u32(16, fields.size());
    for (size_t i = 0; i < fields.size(); ++i) {
        size_t offset = 20 + i * 16;
        u32(offset, uint32_t(keys.size()) | 0x02040000);
        u32(offset + 4, fields[i].second.size() + 1);
        u32(offset + 8, fields[i].second.size() + 1);
        u32(offset + 12, values.size());
        keys += fields[i].first + '\0'; values += fields[i].second + '\0';
    }
    u32(12, out.size() + keys.size());
    return out + keys + values;
}
