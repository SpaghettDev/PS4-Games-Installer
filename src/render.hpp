#pragma once
#include "canvas.hpp"
#include "ui.hpp"

namespace psgi {
class Renderer {
    std::shared_ptr<Image> placeholder, previousBackground, currentBackground, backgroundSource;
    std::string backgroundKey;
    double backgroundChanged = 0;
    void panel(Canvas& canvas, int x, int y, int w, int h, const std::string& title);
    void menu(Canvas& canvas, const std::vector<std::string>& rows, size_t selected,
              int x, int y, int w, int rowHeight = 54);
public:
    explicit Renderer(const std::string& assetDirectory);
    void draw(Canvas& canvas, Ui& ui, ArtworkCache& artwork, double time);
};
}
