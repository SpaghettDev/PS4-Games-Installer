#include "app.hpp"

namespace psgi {
std::vector<Entry> demoLibrary() {
#ifdef PSGI_HOST
    const char* names[] = {"DOOM", "Horizon Zero Dawn", "Journey", "Control", "Celeste", "No Man's Sky", "Death Stranding", "The Last Guardian", "ABZU", "Bloodborne", "God of War", "GRIS", "Hollow Knight", "Inside", "Ratchet & Clank", "Shadow of the Colossus"};
    std::vector<Entry> entries;
    for (int i = 0; i < 16; ++i) {
        Entry e;
        e.name = names[i]; e.titleId = "CUSA" + std::to_string(10000 + i);
        e.url = "http://preview.invalid/packages/" + std::to_string(i) + ".pkg";
        e.localIcon = "build/demo/" + std::to_string(i) + ".png";
        e.coverUrl = "http://preview.invalid/" + std::to_string(i) + ".png";
        e.region = i % 3 ? "EUR" : "USA";
        e.category = i % 5 ? "games" : "homebrew";
        e.size = i == 0 ? 32000000000ULL : uint64_t(2 + i * 3) * 1000000000;
        e.version = "1.00"; e.release = "2026"; e.minFirmware = "11.00";
        e.description = "Desktop preview fixture. Names, IDs and sizes are sample data; no packages are downloaded.";
        entries.push_back(e);
    }
    return entries;
#else
    return {};
#endif
}
}
