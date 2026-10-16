#include "controller.hpp"
#include <cstdlib>

namespace psgi {
namespace {
constexpr uint32_t Up = buttonMask(Button::Up), Down = buttonMask(Button::Down);
constexpr uint32_t Left = buttonMask(Button::Left), Right = buttonMask(Button::Right);
constexpr uint32_t Directions = Up | Down | Left | Right;
constexpr int Enter = 12000, Release = 8000, AxisMargin = 3000;
uint32_t direction(int x, int y, uint32_t previous) {
    const int horizontal = std::abs(x), vertical = std::abs(y);
    const int projection = previous == Up ? -y : previous == Down ? y : previous == Left ? -x : x;
    const int perpendicular = (previous == Up || previous == Down) ? horizontal : vertical;
    // Retain the current axis near diagonals, and use a smaller release deadzone.
    if (previous && projection >= Release && perpendicular <= projection + AxisMargin) return previous;
    if (horizontal < Enter && vertical < Enter) return 0;
    return horizontal > vertical ? (x < 0 ? Left : Right) : (y < 0 ? Up : Down);
}
}
void ControllerInput::suppress(const ControllerState& state) {
    *this = {};
    if (!state.connected) return;
    suppressed = state.buttons;
    suppressStick = std::abs(state.leftX) > Release || std::abs(state.leftY) > Release;
}
std::optional<Button> ControllerInput::poll(const ControllerState& state, uint64_t now) {
    if (!state.connected) { *this = {}; return {}; }
    if (std::abs(state.leftX) <= Release && std::abs(state.leftY) <= Release) suppressStick = false;
    stick = suppressStick ? 0 : direction(state.leftX, state.leftY, stick);
    uint32_t dpad = state.buttons & Directions;
    uint32_t navigation = dpad ? dpad : stick; // The D-pad takes precedence over the stick.
    if ((navigation & (Up | Down)) == (Up | Down)) navigation &= ~(Up | Down);
    if ((navigation & (Left | Right)) == (Left | Right)) navigation &= ~(Left | Right);
    suppressed &= state.buttons;
    uint32_t held = ((state.buttons & ~Directions) | navigation) & ~suppressed;
    uint32_t pressed = held & ~previous;
    uint32_t directions = held & Directions;
    if (directions != repeating) { repeating = directions; repeatAt = now + 350000; }
    else if (directions && now >= repeatAt) { pressed |= directions; repeatAt = now + 100000; }
    previous = held;
    // One action per frame prevents a back/confirm combination acting twice.
    for (Button button : {Button::Circle, Button::Options, Button::Cross, Button::Square, Button::Triangle,
                          Button::Up, Button::Down, Button::Left, Button::Right, Button::L1, Button::R1, Button::L2, Button::R2})
        if (pressed & buttonMask(button)) return button;
    return {};
}
}
