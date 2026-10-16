#include "render.hpp"
#include "revision.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace psgi {
namespace {
constexpr uint32_t White = 0xf2f7ff, Muted = 0x9bbdd8, Accent = 0x7ae5e0, Blue = 0x267bb8, Panel = 0x102c49;
const char* Copyright = "Created by e1z0 (c) 2026 Vintage2000.org";
void buttonIcon(Canvas& c, Button button, int x, int baseline, int width) {
    const int top = baseline - 27;
    c.rounded(x, top, width, 36, 8, 0x46647e);
    c.rounded(x + 1, top + 1, width - 2, 34, 7, 0x16344f);
    auto line = [&](int ax, int ay, int bx, int by, uint32_t color) {
        const int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
        for (int i = 0; i <= steps; ++i)
            c.rectangle(x + ax + (bx - ax) * i / steps, top + ay + (by - ay) * i / steps, 2, 2, color);
    };
    switch (button) {
        case Button::Cross:
            line(10, 10, 24, 24, 0x8cbfff); line(10, 24, 24, 10, 0x8cbfff); break;
        case Button::Square:
            line(9, 9, 25, 9, 0xf4a5d7); line(25, 9, 25, 25, 0xf4a5d7);
            line(25, 25, 9, 25, 0xf4a5d7); line(9, 25, 9, 9, 0xf4a5d7); break;
        case Button::Triangle:
            line(17, 8, 27, 25, 0x7ae5b5); line(27, 25, 7, 25, 0x7ae5b5);
            line(7, 25, 17, 8, 0x7ae5b5); break;
        case Button::Circle:
            for (int i = 0; i < 64; ++i) {
                double angle = i * 6.283185307 / 64;
                c.rectangle(x + 17 + int(std::round(9 * std::cos(angle))),
                            top + 17 + int(std::round(9 * std::sin(angle))), 2, 2, 0xff9b9f);
            }
            break;
        default: {
            const std::string label = button == Button::Options ? "OPTIONS" : "L1 / R1";
            c.text(label, x + (width - c.textWidth(label, 13)) / 2, top + 23, 13, White);
            break;
        }
    }
}
void hints(Canvas& c, int x, int baseline,
           std::initializer_list<std::pair<Button, std::string>> items, int size = 23) {
    for (const auto& item : items) {
        const int width = item.first == Button::Options || item.first == Button::L1 ? 80 : 36;
        buttonIcon(c, item.first, x, baseline, width);
        c.text(item.second, x + width + 10, baseline, size, White);
        x += width + 10 + c.textWidth(item.second, size) + 32;
    }
}
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
std::string state(const Entry& e) { return e.installed ? "Installed" : e.queued ? "Queued" : e.wasInstalled ? "Was installed" : ""; }
void stars(Canvas& c, unsigned rating, int x, int y, int size) {
    if (!rating) return;
    for (unsigned star = 0; star < 5; ++star) {
        std::array<std::pair<double, double>, 10> points;
        for (size_t i = 0; i < points.size(); ++i) {
            const double angle = -1.57079632679 + i * .628318530718;
            const double radius = size * (i % 2 ? .22 : .49);
            points[i] = {x + star * (size + 4) + size * .5 + std::cos(angle) * radius,
                         y + size * .5 + std::sin(angle) * radius};
        }
        for (int row = y; row < y + size; ++row) {
            std::array<double, 10> crossings{}; size_t count = 0;
            for (size_t i = 0; i < points.size(); ++i) {
                const auto a = points[i], b = points[(i + 1) % points.size()]; const double scan = row + .5;
                if ((a.second <= scan && b.second > scan) || (b.second <= scan && a.second > scan))
                    crossings[count++] = a.first + (scan - a.second) * (b.first - a.first) / (b.second - a.second);
            }
            std::sort(crossings.begin(), crossings.begin() + count);
            for (size_t i = 0; i + 1 < count; i += 2) {
                const int left = int(std::ceil(crossings[i])), right = int(std::floor(crossings[i + 1]));
                c.rectangle(left, row, right - left + 1, 1, star < rating ? 0xffd478 : 0x46647e);
            }
        }
    }
}
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
                    int x, int y, int w, int rowHeight, size_t count) {
    const size_t start = count == SIZE_MAX ? 0 : selected / count * count;
    for (size_t i = start; i < rows.size() && i - start < count; ++i) {
        int top = y + int(i - start) * rowHeight;
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
    c.text(ui.filterDescription(), 750, 205, 20, Accent, 850);
    c.text(std::to_string(ui.shownGames()) + (ui.shownGames() == 1 ? " title" : " titles"), 1640, 204, 23, Muted, 220);

    if (ui.visible.empty()) {
        c.image(*placeholder, 1020, 365, 140, 140, 180);
        c.text(!ui.filterDescription().empty() || !ui.query.empty() ? "No games match this view" : ui.library == Library::Installed ? "No installed games found" : "Your remote library belongs here", 700, 585, 34, White, 1100);
        c.text(!ui.filterDescription().empty() ? "Options > Browse local categories or tags > All games clears local filters." : !ui.query.empty() ? "Options > Search lets you change or clear your search." : ui.library == Library::Installed ? "Refresh the library in Options, or browse Available." : "Open Options > Settings and enter your Games JSON URL.", 610, 641, 26, Muted, 1220);
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
                c.image(*(image ? image : placeholder), x + 55, y + 12, 220, 204);
                if (!state(entry).empty()) {
                    c.rounded(x + 13, y + 16, 148, 28, 6, 0x08283c, 235);
                    c.text(state(entry), x + 22, y + 36, 15, Accent, 132);
                }
                c.text(entry.name, x + 17, y + 245, 24, White, 297);
                c.text(entry.info.category.empty() ? "Uncategorized" : entry.info.category, x + 17, y + 270, 17, Accent, 146);
                c.text(entry.info.label.empty() ? "Unlabeled" : entry.info.label, x + 174, y + 270, 17, Muted, 140);
                c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), x + 17, y + 299, 21, Muted, 288);
                stars(c, entry.rating, x + 190, y + 282, 18);
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
            c.text(entry.name, 450, y + 3, 25, White, 645);
            std::string meta = entry.titleId + (entry.region.empty() ? "" : "  " + entry.region) + (ui.visible[i].group.empty() ? "" : "  /  " + ui.visible[i].group);
            if (!state(entry).empty()) meta += "  /  " + state(entry);
            c.text(entry.info.category.empty() ? "Uncategorized" : entry.info.category, 451, y + 26, 17, Accent, 300);
            c.text(entry.info.label.empty() ? "Unlabeled" : entry.info.label, 775, y + 26, 17, Muted, 315);
            c.text(meta, 451, y + 46, 15, Muted, 640);
            c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), 1110, y + 13, 21, Muted, 165);
            stars(c, entry.rating, 1110, y + 25, 17);
        }
        if (selected) {
            auto image = artwork.request(*selected, true);
            c.rounded(1340, 252, 514, 622, 16, Panel, 180);
            c.image(*(image ? image : placeholder), 1366, 279, 462, 400);
            c.text(selected->name, 1372, 732, 30, White, 452);
            c.text(selected->info.category.empty() ? "Uncategorized" : selected->info.category, 1373, 763, 18, Accent, 215);
            c.text(selected->info.label.empty() ? "Unlabeled" : selected->info.label, 1605, 763, 18, Muted, 218);
            c.text(shortSize(ui.library == Library::Installed ? selected->installedSize : selected->size), 1373, 800, 24, Accent, 450);
            c.text(state(*selected), 1373, 831, 20, Muted, 450);
            stars(c, selected->rating, 1373, 845, 20);
        }
    }
    if (selected) {
        auto description = selected->info.description.empty() ? selected->description : selected->info.description;
        for (auto& c : description) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (!description.empty()) c.text(description, 368, 946, 21, Muted, 1250);
    }
    c.rectangle(54, 964, 1812, 1, 0x638aad, 65);
    c.text(ui.librarySummary(), 70, 1000, 22, White, 620);
    c.text(ui.status, 720, 1000, 21, ui.loading ? Accent : Muted, 1130);
    hints(c, 70, 1045, {{Button::Cross, ui.library == Library::Installed ? "Launch" : "Install"},
        {Button::Square, "Details"}, {Button::Triangle, "Actions"}, {Button::Options, "Menu"},
        {Button::Circle, "Back / Exit"}, {Button::L1, "Library"}}, 22);

    switch (ui.screen) {
        case Screen::Options:
            panel(c, 520, 168, 880, 728, "Options");
            menu(c, ui.options(), ui.menuIndex, 553, 310, 814, 58, 8);
            c.text("Up / Down for more options", 570, 812, 20, Muted, 760);
            hints(c, 570, 862, {{Button::Cross, "Select"}, {Button::Circle, "Back"}}); break;
        case Screen::Settings: {
            panel(c, 352, 115, 1216, 840, "Settings");
            menu(c, ui.settingRows(), ui.settingsIndex, 390, 251, 1140, 48);
            c.text(ui.status, 412, 812, 20, Muted, 1090);
            std::string url = ui.draft.sources[0].url;
            c.text(url.empty() ? "Games URL is not configured" : url, 412, 844, 21, Muted, 1090);
            c.text(ui.settingsNotice.empty() ? "Select Save settings to apply edits. O cancels unsaved changes." : ui.settingsNotice,
                   412, 879, 20, Accent, 1090);
            hints(c, 412, 913, {{Button::Cross, "Change / Select"}, {Button::Circle, "Cancel"}});
            c.text("Left / Right Change", 1100, 913, 23, Muted); break;
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
            hints(c, 412, 898, {{Button::Cross, "Edit / Add"}, {Button::Triangle, "Remove"}, {Button::Circle, "Back"}}); break;
        }
        case Screen::Storage: {
            panel(c, 320, 170, 1280, 740, "Installation storage");
            c.text("Choose where new games will be installed", 370, 320, 30, White, 1180);
            auto space = [&](const char* name, const StorageSpace& drive, int y, size_t index) {
                if (ui.storageIndex == index) c.rounded(352, y - 36, 1210, 67, 9, Blue, 190);
                const bool active = ui.storage.destination == (index ? InstallDestination::External : InstallDestination::Internal);
                c.text(std::string(active ? "[Selected]  " : "") + name, 370, y, 27, White, 650);
                c.text(!ui.storageChecked ? "Checking..." : drive.available && !drive.sizeKnown ? "Space unavailable" : drive.available ?
                       (drive.freeBytes ? shortSize(drive.freeBytes) : "0 B") + " free" : "Unavailable",
                       1030, y, 26, drive.available ? Accent : Muted, 510);
            };
            space("Internal storage", ui.storage.internal, 425, 0);
            space("External USB (Extended Storage)", ui.storage.external, 510, 1);
            c.text("Press X to apply. This also changes the PS4's default install location.", 370, 610, 25, Accent, 1180);
            c.text("A USB drive must already be configured as PS4 Extended Storage.", 370, 671, 24, Muted, 1180);
            c.text(ui.storage.error.empty() ? "Existing games stay on their current drive." : ui.storage.error, 370, 729, 24, Muted, 1180);
            hints(c, 370, 863, {{Button::Cross, "Select destination"}, {Button::Triangle, "Check storage"}, {Button::Circle, "Back"}}); break;
        }
        case Screen::Details: {
            panel(c, 230, 120, 1460, 828, "Game details");
            auto image = artwork.request(ui.pending, true);
            c.image(*(image ? image : placeholder), 270, 240, 260, 260);
            c.text(ui.pending.name, 572, 264, 34, White, 1055);
            auto rows = ui.details();
            for (size_t i = ui.detailScroll; i < std::min(rows.size(), ui.detailScroll + 12); ++i)
                c.text(rows[i], 574, 322 + (i - ui.detailScroll) * 43, 23, i % 2 ? Muted : White, 1040);
            c.text("Up / Down Scroll", 575, 908, 23, Muted);
            hints(c, 840, 908, {{Button::Circle, "Back"}}); break;
        }
        case Screen::Actions:
            panel(c, 495, 210, 930, 650, ui.pending.name);
            menu(c, ui.actionRows(), ui.menuIndex, 532, 356, 856, 58);
            hints(c, 548, 815, {{Button::Cross, "Select"}, {Button::Circle, "Back"}}); break;
        case Screen::Rating: {
            panel(c, 560, 190, 800, 680, "Rate game");
            c.text(ui.pending.name, 600, 328, 28, White, 720);
            std::vector<std::string> rows;
            for (unsigned i = 1; i <= 5; ++i) rows.push_back(std::to_string(i) + (i == 1 ? " star" : " stars") + (ui.pending.rating == i ? " (current)" : ""));
            rows.push_back("Remove rating");
            menu(c, rows, ui.ratingIndex, 598, 400, 724, 56);
            for (unsigned i = 1; i <= 5; ++i) stars(c, i, 1110, 382 + (i - 1) * 56, 20);
            c.text("Saved locally by Title ID. Catalog refreshes keep your rating.", 600, 770, 20, Muted, 720);
            hints(c, 600, 829, {{Button::Cross, "Save rating"}, {Button::Circle, "Cancel"}}); break;
        }
        case Screen::LocalInfo:
            panel(c, 350, 170, 1220, 740, "Edit local game info");
            c.text(ui.pending.name + "  /  " + ui.pending.titleId, 395, 307, 28, White, 1130);
            menu(c, ui.localRows(), ui.localIndex, 390, 385, 1140, 72);
            c.text(ui.localNotice.empty() ? "New category/tag names are created when you save. Empty text clears a field." : ui.localNotice, 395, 785, 22, Accent, 1130);
            hints(c, 395, 861, {{Button::Cross, "Edit / Save"}, {Button::Circle, "Back"}}); break;
        case Screen::PickCategory:
        case Screen::PickTags: {
            const bool tags = ui.screen == Screen::PickTags;
            panel(c, 470, 160, 980, 760, tags ? "Choose local tags" : "Choose local category");
            std::vector<std::string> rows{tags ? "Clear all game tags" : "Uncategorized"};
            for (const auto& term : ui.termChoices(tags, true)) {
                const bool active = tags ? std::any_of(ui.localDraft.tags.begin(), ui.localDraft.tags.end(), [&](const std::string& tag) { return lower(tag) == lower(term); }) : lower(term) == lower(ui.localDraft.category);
                rows.push_back(std::string(active ? "[Selected]  " : "") + term);
            }
            menu(c, rows, ui.pickerIndex, 510, 320, 900, 56, 8);
            c.text(tags ? "X toggles a tag. O returns to the draft." : "Choose a category, or press Triangle to create one.", 510, 788, 21, Muted, 900);
            hints(c, 510, 863, {{Button::Cross, "Select"}, {Button::Triangle, "New"}, {Button::Circle, "Back"}}); break;
        }
        case Screen::Collections: {
            panel(c, 380, 140, 1160, 790, ui.collectionTags ? "Browse local tags" : "Browse local categories");
            c.text(ui.library == Library::Installed ? "Installed library" : "Available library", 425, 276, 23, Accent, 1070);
            menu(c, ui.collectionRows(), ui.collectionIndex, 420, 340, 1080, 56, 7);
            c.text(ui.collectionNotice.empty() ? "L1 / R1 switches categories and tags. Up / Down scrolls names." : ui.collectionNotice, 425, 792, 21, Muted, 1070);
            hints(c, 425, 857, {{Button::Cross, "Browse"}, {Button::Triangle, "New"}, {Button::Square, "Rename"}, {Button::Options, "Delete"}}, 21);
            hints(c, 425, 900, {{Button::Circle, "Back"}}, 21); break;
        }
        case Screen::ConfirmDeleteCollection:
            panel(c, 390, 280, 1140, 530, ui.collectionTags ? "Delete local tag?" : "Delete local category?");
            c.text(ui.collectionOriginal, 435, 429, 32, White, 1050);
            c.text("This removes the name and its assignments from all local records.", 435, 515, 24, Muted, 1050);
            c.text("Games, ratings, descriptions and installation history are kept.", 435, 566, 24, Muted, 1050);
            hints(c, 435, 749, {{Button::Cross, "Delete local name"}, {Button::Circle, "Cancel"}}); break;
        case Screen::DiscardLocal:
            panel(c, 495, 334, 930, 380, "Discard local info edits?");
            c.text("Your saved local info will be kept.", 545, 502, 26, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Discard"}, {Button::Cross, "Keep editing"}}, 26); break;
        case Screen::ConfirmUninstall:
            panel(c, 360, 266, 1200, 548, "Uninstall game?");
            c.text(ui.pending.name, 410, 413, 33, White, 1100);
            c.text(ui.pending.titleId, 410, 467, 26, Accent, 1100);
            c.text("This removes the game's installed files.", 410, 545, 26, Muted, 1100);
            c.text("Your local rating and installation history will be kept.", 410, 591, 24, Muted, 1100);
            hints(c, 410, 753, {{Button::Cross, "Uninstall"}, {Button::Circle, "Cancel"}}, 26); break;
        case Screen::ConfirmInstall:
            panel(c, 360, 266, 1200, 548, "Install package?");
            c.text(ui.pending.name, 410, 413, 33, White, 1100);
            c.text(shortSize(ui.pending.size) + "  " + ui.pending.titleId, 410, 467, 26, Accent, 1100);
            c.text("Uses background installation or a direct-download fallback.", 410, 545, 26, Muted, 1100);
            c.text("Keep this app open while a direct-download percentage is shown.", 410, 591, 24, Muted, 1100);
            c.text(ui.pending.installed ? "An existing title will not be automatically uninstalled." : "Submitted tasks appear in PS4 Notifications > Downloads.", 410, 637, 24, Muted, 1100);
            hints(c, 410, 753, {{Button::Cross, "Confirm install"}, {Button::Circle, "Cancel"}}, 26); break;
        case Screen::InstallError: {
            panel(c, 330, 200, 1260, 680, "Installation failed");
            c.text(ui.failedInstall.name, 380, 347, 33, White, 1160);
            c.text("Downloading a package does not install the game.", 380, 410, 26, Accent, 1160);
            std::string remaining = ui.installError;
            int baseline = 473;
            while (!remaining.empty() && baseline <= 737) {
                size_t length = std::min(size_t(78), remaining.size());
                if (length < remaining.size()) {
                    size_t space = remaining.rfind(' ', length);
                    if (space != std::string::npos && space > 15) length = space;
                    while (length && (static_cast<unsigned char>(remaining[length]) & 0xc0) == 0x80) --length;
                    if (!length) length = 1;
                }
                c.text(remaining.substr(0, length), 380, baseline, 25, Muted, 1160);
                remaining.erase(0, length);
                if (!remaining.empty() && remaining.front() == ' ') remaining.erase(0, 1);
                baseline += 44;
            }
            hints(c, 380, 827, {{Button::Cross, "Retry"}, {Button::Circle, "Close"}}, 26); break;
        }
        case Screen::ConfirmExit:
            panel(c, 495, 334, 930, 380, "Exit PS4 Games Installer?");
            c.text("Submitted PS4 installation tasks will continue.", 545, 502, 25, Muted, 830);
            c.text("An unfinished direct download will be cancelled.", 545, 549, 25, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Confirm exit"}, {Button::Cross, "Stay"}}, 28); break;
        case Screen::Discard:
            panel(c, 495, 334, 930, 380, "Discard unsaved settings?");
            c.text("Your saved configuration will be kept.", 545, 502, 26, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Discard"}, {Button::Cross, "Keep editing"}}, 28); break;
        case Screen::About:
            panel(c, 290, 210, 1340, 660, "About PS4 Games Installer");
            c.image(*placeholder, 345, 334, 180, 180);
            c.text("PS4 Games Installer " PSGI_VERSION, 570, 365, 37, White, 1000);
            c.text(Copyright, 570, 420, 25, Accent, 1000);
            c.text("An OpenOrbis game library, launcher and package installer.", 570, 477, 24, Muted, 1000);
            c.text("Works with a companion catalog and package server.", 345, 580, 27, White, 1230);
            c.text("Browse its library, view game artwork and install packages.", 345, 625, 24, Muted, 1230);
            c.text("https://github.com/e1z0/PS4-Games-Installer", 345, 700, 24, Accent, 1230);
            c.text("Native PS4 dashboard built with the OpenOrbis toolchain.", 345, 745, 24, Muted, 1230);
            hints(c, 345, 827, {{Button::Circle, "Back"}}); break;
        case Screen::Keyboard:
            panel(c, 300, 330, 1320, 400, keyboardTitle(ui.editKind));
            c.rounded(340, 466, 1240, 88, 10, 0x061c32, 255);
            c.text(ui.keyboardText + "|", 367, 522, 28, White, 1185);
#ifdef PSGI_HOST
            c.text("Type with your keyboard. Enter saves; Escape cancels.", 350, 671, 24, Accent);
#else
            c.text("X selects a character. Choose Done on the keyboard to accept.", 350, 610, 24, Muted);
            hints(c, 350, 671, {{Button::Cross, "Select key"}, {Button::Circle, "Cancel"}});
#endif
            break;
        default: break;
    }
}
}
