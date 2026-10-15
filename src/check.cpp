#include "catalog.hpp"
#include "platform.hpp"
#include "settings.hpp"
#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: catalog-check config.json\n");
        return 2;
    }
    try {
        auto sources = psgi::parseSettings(psgi::readFile(argv[1])).sources;
        size_t loaded = 0;
        std::vector<psgi::Entry> entries;
        for (const auto& source : sources) {
            if (source.url.empty()) continue;
            auto catalog = psgi::parseCatalog(psgi::httpGet(source.url), source.category);
            std::printf("%s: %zu packages\n", source.category.c_str(), catalog.size());
            psgi::mergeCatalog(entries, catalog);
            ++loaded;
        }
        if (!loaded) { std::fprintf(stderr, "No catalog sources configured\n"); return 1; }
        std::printf("%zu unique packages\n", entries.size());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
