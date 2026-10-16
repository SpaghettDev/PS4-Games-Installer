#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

inline void writePackageFixture(const std::string& path, uint64_t size = 16384,
                                const std::string& content = "IV0000-CUSA00001_00-1234567890123456") {
    std::vector<unsigned char> header(0x440);
    header[0] = 0x7f; header[1] = 'C'; header[2] = 'N'; header[3] = 'T';
    for (size_t i = 0; i < content.size(); ++i) header[0x40 + i] = content[i];
    header[0x77] = 0x1a;
    for (unsigned i = 0; i < 8; ++i) header[0x430 + i] = size >> (56 - i * 8);
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file && std::fwrite(header.data(), 1, header.size(), file) == header.size());
    assert(!fseeko(file, size - 1, SEEK_SET) && std::fputc(0, file) != EOF && !std::fclose(file));
}
