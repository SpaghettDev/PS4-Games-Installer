#pragma once
#include "controller.hpp"
#include <orbis/Pad.h>

namespace psgi {
inline ControllerState readNativeController(const OrbisPadData& data, bool valid) {
    ControllerState state;
    if (!valid || !data.connected) return state;
    state.connected = true;
    state.leftX = (int(data.leftStick.x) - 128) * 256;
    state.leftY = (int(data.leftStick.y) - 128) * 256;
    const std::pair<uint32_t, Button> bindings[] = {
        {ORBIS_PAD_BUTTON_CIRCLE, Button::Circle}, {ORBIS_PAD_BUTTON_OPTIONS, Button::Options},
        {ORBIS_PAD_BUTTON_CROSS, Button::Cross}, {ORBIS_PAD_BUTTON_SQUARE, Button::Square},
        {ORBIS_PAD_BUTTON_TRIANGLE, Button::Triangle}, {ORBIS_PAD_BUTTON_UP, Button::Up},
        {ORBIS_PAD_BUTTON_DOWN, Button::Down}, {ORBIS_PAD_BUTTON_LEFT, Button::Left},
        {ORBIS_PAD_BUTTON_RIGHT, Button::Right}, {ORBIS_PAD_BUTTON_L1, Button::L1}, {ORBIS_PAD_BUTTON_R1, Button::R1},
        {ORBIS_PAD_BUTTON_L2, Button::L2}, {ORBIS_PAD_BUTTON_R2, Button::R2}
    };
    for (const auto& binding : bindings) if (data.buttons & binding.first) state.buttons |= buttonMask(binding.second);
    return state;
}
}
