#pragma once
#include "ui.hpp"
#include <cstdint>
#include <optional>

namespace psgi {
constexpr uint32_t buttonMask(Button button) { return 1u << static_cast<unsigned>(button); }
struct ControllerState {
    bool connected = false;
    uint32_t buttons = 0;
    // Centered axes: -32768 is up/left, +32767 is down/right.
    int leftX = 0, leftY = 0;
};
class ControllerInput {
    uint32_t previous = 0, repeating = 0, suppressed = 0, stick = 0;
    uint64_t repeatAt = 0;
    bool suppressStick = false;
public:
    std::optional<Button> poll(const ControllerState& state, uint64_t microseconds);
    // Ignore held controls until released, including after a native dialog.
    void suppress(const ControllerState& state);
};
}
