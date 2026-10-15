#include "render.hpp"
#include "revision.hpp"
#include <algorithm>
#include <cmath>

namespace psgi {
namespace {
constexpr uint32_t White = 0xf2f7ff, Muted = 0x9bbdd8, Accent = 0x7ae5e0, Blue = 0x267bb8, Panel = 0x102c49;
const char* Copyright = "Created by e1z0 (c) 2026 Vintage2000.org";
std::shared_ptr<Image> softened(const Image& input) {
    auto image = std::make_shared<Image>(input);
    std::vector<uint32_t> scratch(input.pixels.size());
    constexpr int radius = 7;
    for (int pass = 0; pass < 2; ++pass) {
        const int outer = pass == 0 ? input.height : input.width;
        const int inner = pass == 0 ? input.width : input.height;
        auto index = [&](int row, int col) { return pass == 0 ? row * input.width + col : col * input.width + row; };
        for (int row = 0; row < outer; ++row) {
            int red = 0, green = 0, blue = 0;
            auto add = [&](int col, int factor) {
                uint32_t color = image->pixels[index(row, std::clamp(col, 0, inner-1))];
                red += int((color >> 16) & 255) * factor;
                green += int((color >> 8) & 255) * factor;
                blue += int(color & 255) * factor;
            };
            for (int i = -radius; i <= radius; ++i) add(i, 1);
            for (int col = 0; col < inner; ++col) {
                scratch[index(row, col)] = 0xff000000 | uint32_t(red / 15) << 16 | uint32_t(green / 15) << 8 | uint32_t(blue / 15);
                add(col - radius, -1); add(col + radius + 1, 1);
            }
        }
        image->pixels.swap(scratch);
    }
    return image;
}
std::string state(const Entry& e) { return e.queued ? "QUEUED" : e.installed ? "INSTALLED" : ""; }
std::string shortSize(uint64_t bytes) {
    auto value = formatSize(bytes);
    size_t p = value.find(".0 ");
    if (p != std::string::npos) value.erase(p, 2);
    return value;
}
}
Renderer::Renderer(const std::string& assets) : placeholder(loadImage(assets + "/icon.png")) {}
void Renderer::panel(Canvas& c, int x, int y, int w, int h, const std::string& title) {
    c.rectangle(0, 0, Canvas::Width, Canvas::Height, 0x020b17, 170);
    c.rounded(x + 10, y + 16, w, h, 20, 0x00040d, 85);
    c.rounded(x, y, w, h, 20, Panel, 250);
    c.rectangle(x + 32, y, w - 64, 3, Accent);
    c.text(title, x + 40, y + 68, 36, White, w - 80);
}
void Renderer::menu(Canvas& c, const std::vector<std::string>& rows, size_t selected,
                    int x, int y, int w, int rowHeight) {
    for (size_t i = 0; i < rows.size(); ++i) {
        int top = y + int(i) * rowHeight;
        if (i == selected) c.rounded(x, top - 32, w, rowHeight - 4, 9, Blue, 180);
        c.text(rows[i], x + 20, top, 27, i == selected ? White : Muted, w - 40);
        if (i == selected) c.rectangle(x, top - 27, 3, rowHeight - 14, Accent);
    }
}
void Renderer::draw(Canvas& c, Ui& ui, ArtworkCache& artwork, double time) {
    c.background(time, ui.screen == Screen::Settings ? ui.draft.animations : ui.settings.animations);
    const Entry* selected = ui.current();
    if (ui.screen == Screen::Splash) {
        c.image(*placeholder, 904, 330, 112, 112);
        c.centered("Loading PS4 Games Installer", 560, 48, White);
        for (int i = 0; i < 3; ++i) c.rounded(925 + i * 28, 608, 10, 10, 5, Accent,
            uint8_t(100 + 150 * (.5 + .5 * std::sin(time * 3 - i))));
        c.centered(Copyright, 1016, 23, Muted);
        return;
    }
    if (ui.settings.layout == Layout::Summary && selected) {
        std::string key = artworkKey(*selected);
        auto image = artwork.request(*selected, true);
        if (key != backgroundKey || (image && image != backgroundSource)) {
            previousBackground = currentBackground;
            backgroundSource = image;
            currentBackground = image ? softened(*image) : nullptr;
            backgroundKey = key;
            backgroundChanged = time;
        }
        unsigned alpha = ui.settings.animations ? std::min(255.0, std::max(0.0, (time - backgroundChanged) * 650)) : 255;
        if (previousBackground && alpha < 255) c.image(*previousBackground, 0, 0, Canvas::Width, Canvas::Height, 90 * (255 - alpha) / 255, true);
        if (currentBackground) c.image(*currentBackground, 0, 0, Canvas::Width, Canvas::Height, 90 * alpha / 255, true);
        c.rectangle(0, 0, Canvas::Width, Canvas::Height, 0x061a30, 130);
    }
    c.image(*placeholder, 54, 52, 58, 58);
    c.text("PS4 Games Installer", 132, 94, 39, White);
    c.text(PSGI_VERSION, 508, 127, 18, Muted);
    c.text(ui.freeBytes ? shortSize(ui.freeBytes) + " free" : "Your PS4 library", 1500, 94, 25, Muted, 350);
    c.rectangle(54, 151, 1812, 1, 0x638aad, 65);

    c.text("LIBRARY", 74, 216, 20, Muted);
    for (int i = 0; i < 2; ++i) {
        bool active = static_cast<int>(ui.library) == i;
        int y = 268 + i * 67;
        if (active) {
            c.rounded(56, y - 37, 262, 56, 10, ui.sidebar ? Blue : Panel, 210);
            c.rectangle(56, y - 26, 3, 31, Accent);
        }
        c.text(i == 0 ? "Installed" : "Available", 78, y, 29, active ? White : Muted, 220);
    }
    c.text("OPTIONS", 78, 840, 18, Muted);
    c.text("Settings & more", 78, 875, 23, White, 236);
    c.text("Vintage2000.org", 78, 935, 18, Muted);
    c.text(ui.library == Library::Installed ? "Installed games" : "Available games", 368, 206, 32, White);
    c.text(std::to_string(ui.visible.size()) + (ui.visible.size() == 1 ? " title" : " titles"), 1640, 204, 23, Muted, 220);

    if (ui.visible.empty()) {
        c.image(*placeholder, 1020, 365, 140, 140, 180);
        c.text(ui.library == Library::Installed ? "No installed games found" : "Your remote library belongs here", 700, 585, 34, White, 1100);
        c.text(ui.library == Library::Installed ? "Refresh the library in Options, or browse Available." : "Open Options > Settings and enter your Games JSON URL.", 610, 641, 26, Muted, 1220);
        if (ui.loading) c.text("Loading...", 1020, 702, 25, Accent);
    } else if (ui.settings.layout == Layout::Tiles) {
        const auto& rows = ui.tileRows();
        size_t rowIndex = ui.tileRowIndex();
        size_t begin = rowIndex / 2 * 2;
        for (size_t r = begin; r < std::min(begin + 2, rows.size()); ++r) {
            int y = 250 + (r - begin) * 348;
            if (!rows[r].group.empty()) c.text(rows[r].group, 370, y - 14, 20, Accent, 1200);
            for (size_t col = 0; col < rows[r].items.size(); ++col) {
                size_t position = rows[r].items[col];
                const auto& entry = ui.entries()[ui.visible[position].index];
                int x = 366 + col * 376;
                bool focus = position == ui.position() && !ui.sidebar;
                if (focus) c.rounded(x - 3, y - 3, 336, 325, 13, Accent, 220);
                c.rounded(x, y, 330, 319, 11, Panel, focus ? 245 : 190);
                auto image = artwork.request(entry, focus);
                c.image(*(image ? image : placeholder), x + 43, y + 12, 244, 224);
                if (!state(entry).empty()) {
                    c.rounded(x + 13, y + 16, 121, 28, 6, 0x08283c, 235);
                    c.text(state(entry), x + 22, y + 36, 15, Accent, 107);
                }
                c.text(entry.name, x + 17, y + 267, 25, White, 297);
                c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), x + 17, y + 299, 21, Muted, 288);
            }
        }
        c.text("Page " + std::to_string(begin / 2 + 1) + " / " + std::to_string((rows.size() + 1) / 2), 1670, 949, 18, Muted, 180);
    } else {
        // A compact list leaves a large selected-game preview on the right.
        size_t begin = ui.position() / 8 * 8;
        for (size_t i = begin; i < std::min(begin + 8, ui.visible.size()); ++i) {
            const auto& entry = ui.entries()[ui.visible[i].index];
            int y = 266 + (i - begin) * 82;
            bool focus = i == ui.position() && !ui.sidebar;
            if (focus) c.rounded(361, y - 24, 926, 76, 9, Blue, 190);
            auto image = artwork.request(entry, focus);
            c.image(*(image ? image : placeholder), 374, y - 17, 58, 58);
            c.text(entry.name, 450, y + 7, 27, White, 645);
            std::string meta = entry.titleId + (entry.region.empty() ? "" : "  " + entry.region) + (ui.visible[i].group.empty() ? "" : "  /  " + ui.visible[i].group);
            c.text(meta, 451, y + 35, 18, Muted, 660);
            c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), 1110, y + 13, 21, Muted, 165);
        }
        if (selected) {
            auto image = artwork.request(*selected, true);
            c.rounded(1340, 252, 514, 622, 16, Panel, 180);
            c.image(*(image ? image : placeholder), 1366, 279, 462, 400);
            c.text(selected->name, 1372, 732, 30, White, 452);
            c.text(shortSize(ui.library == Library::Installed ? selected->installedSize : selected->size), 1373, 780, 24, Accent, 450);
            c.text(state(*selected), 1373, 831, 20, Muted, 450);
        }
    }
    c.rectangle(54, 964, 1812, 1, 0x638aad, 65);
    c.text(ui.status, 70, 1000, 23, ui.loading ? Accent : White, 1778);
    c.text("X " + std::string(ui.library == Library::Installed ? "Launch" : "Install") + "     Square Details     Triangle Actions     L1/R1 Library     Options Menu     O Back / Exit", 70, 1045, 22, Muted, 1778);

    switch (ui.screen) {
        case Screen::Options:
            panel(c, 520, 168, 880, 728, "Options");
            menu(c, ui.options(), ui.menuIndex, 553, 310, 814, 58);
            c.text("X Select     O Back", 570, 862, 23, Accent); break;
        case Screen::Settings: {
            panel(c, 352, 115, 1216, 840, "Settings");
            menu(c, ui.settingRows(), ui.settingsIndex, 390, 251, 1140, 52);
            std::string url = ui.draft.sources[0].url;
            c.text(url.empty() ? "Games URL is not configured" : url, 412, 844, 21, Muted, 1090);
            c.text("X Change / Select     Left / Right Change     O Cancel", 412, 913, 23, Accent); break;
        }
        case Screen::Sources: {
            panel(c, 352, 140, 1216, 800, "Additional catalog sources");
            size_t start = ui.sourceIndex > 8 ? ui.sourceIndex - 7 : 1;
            for (size_t i = start; i <= ui.draft.sources.size() && i < start + 8; ++i) {
                int y = 280 + (i - start) * 68;
                if (i == ui.sourceIndex) c.rounded(390, y - 34, 1140, 62, 9, Blue, 190);
                c.text(i == ui.draft.sources.size() ? "+ Add catalog source" : ui.draft.sources[i].category, 414, y - 4, 27, White, 1090);
                if (i < ui.draft.sources.size()) c.text(ui.draft.sources[i].url.empty() ? "Disabled" : ui.draft.sources[i].url, 414, y + 22, 18, Muted, 1090);
            }
            c.text("X Edit / Add     Triangle Remove     O Back", 412, 898, 23, Accent); break;
        }
        case Screen::Details: {
            panel(c, 230, 120, 1460, 828, "Game details");
            auto image = artwork.request(ui.pending, true);
            c.image(*(image ? image : placeholder), 270, 240, 260, 260);
            c.text(ui.pending.name, 572, 264, 34, White, 1055);
            auto rows = ui.details();
            for (size_t i = ui.detailScroll; i < std::min(rows.size(), ui.detailScroll + 12); ++i)
                c.text(rows[i], 574, 322 + (i - ui.detailScroll) * 43, 23, i % 2 ? Muted : White, 1040);
            c.text("Up / Down Scroll     O Back", 575, 908, 23, Accent); break;
        }
        case Screen::Actions:
            panel(c, 495, 275, 930, 510, ui.pending.name);
            menu(c, ui.actionRows(), ui.menuIndex, 532, 413, 856, 64);
            c.text("X Select     O Back", 548, 747, 23, Accent); break;
        case Screen::ConfirmInstall:
            panel(c, 360, 266, 1200, 548, "Install package?");
            c.text(ui.pending.name, 410, 413, 33, White, 1100);
            c.text(shortSize(ui.pending.size) + "  " + ui.pending.titleId, 410, 467, 26, Accent, 1100);
            c.text("Download and install through the PS4 background installer.", 410, 545, 26, Muted, 1100);
            c.text(ui.pending.installed ? "An existing title will not be automatically uninstalled." : "Manage the task in PS4 Notifications > Downloads.", 410, 591, 24, Muted, 1100);
            c.text("X Confirm install     O Cancel", 410, 753, 26, Accent); break;
        case Screen::ConfirmExit:
            panel(c, 495, 334, 930, 380, "Exit PS4 Games Installer?");
            c.text("Background installation tasks will continue on the PS4.", 545, 502, 26, Muted, 830);
            c.text("O Confirm exit     X Stay", 545, 652, 28, Accent); break;
        case Screen::Discard:
            panel(c, 495, 334, 930, 380, "Discard unsaved settings?");
            c.text("Your saved configuration will be kept.", 545, 502, 26, Muted, 830);
            c.text("O Discard     X Keep editing", 545, 652, 28, Accent); break;
        case Screen::About:
            panel(c, 290, 210, 1340, 660, "About PS4 Games Installer");
            c.image(*placeholder, 345, 334, 180, 180);
            c.text("PS4 Games Installer " PSGI_VERSION, 570, 365, 37, White, 1000);
            c.text(Copyright, 570, 420, 25, Accent, 1000);
            c.text("An OpenOrbis game library, launcher and package installer.", 570, 477, 24, Muted, 1000);
            c.text("Developed in cooperation with FPKGi-node-server.", 345, 580, 27, White, 1230);
            c.text("The projects work together for catalog, cover and package hosting.", 345, 625, 24, Muted, 1230);
            c.text("https://github.com/e1z0/PS4-Games-Installer", 345, 700, 24, Accent, 1230);
            c.text("https://github.com/e1z0/FPKGi-node-server", 345, 745, 24, Accent, 1230);
            c.text("O Back", 345, 827, 23, Muted); break;
        case Screen::Keyboard:
            panel(c, 300, 330, 1320, 400, ui.editKind == EditKind::Search ? "Search library" : ui.editKind == EditKind::SourceCategory ? "Category name" : "Catalog JSON URL");
            c.rounded(340, 466, 1240, 88, 10, 0x061c32, 255);
            c.text(ui.keyboardText + "|", 367, 522, 28, White, 1185);
            c.text("Enter / X Save     Escape / O Cancel", 350, 671, 24, Accent); break;
        default: break;
    }
}
}
