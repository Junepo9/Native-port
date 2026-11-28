#pragma once
#include <cstdint>

// Button bits, in the style of the 3DS's hardware input.
enum Button : uint32_t {
    BTN_A = 1u << 0,
    BTN_B = 1u << 1,
    BTN_X = 1u << 2,
    BTN_Y = 1u << 3,
    BTN_L = 1u << 4,
    BTN_R = 1u << 5,
    BTN_ZL = 1u << 6,
    BTN_ZR = 1u << 7,
    BTN_START = 1u << 8,
    BTN_SELECT = 1u << 9,
    BTN_DUP = 1u << 10,
    BTN_DDOWN = 1u << 11,
    BTN_DLEFT = 1u << 12,
    BTN_DRIGHT = 1u << 13,
};

// What the game sees each tick.
struct HidState {
    uint32_t held = 0;   // buttons currently held
    uint32_t down = 0;   // buttons pressed this tick
    uint32_t up = 0;     // buttons released this tick
    float circleX = 0.0f;  // -1..1, right is positive
    float circleY = 0.0f;  // -1..1, up is positive
    bool touching = false;
    int touchX = 0;  // 0..319 on the bottom screen
    int touchY = 0;  // 0..239 on the bottom screen
};
