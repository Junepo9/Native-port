#pragma once
#include <SDL.h>

#include <cmath>

#include "controls.h"
#include "hid.h"

// A New-3DS-style C-stick, placed in the empty band between the D-pad and the
// face buttons. Change these two numbers to move it.
constexpr int CSTICK_R = 52;

static inline int cstickCx() { return (gL.dpadX + gL.faceX) / 2; }
static inline int cstickCy() { return 592; }

struct StickAxis {
    float x = 0.0f;  // -1 .. 1, right is positive
    float y = 0.0f;  // -1 .. 1, down is positive
    bool active = false;
};

// Returns the stick position, adopting a finger that lands near it and holding
// onto that finger even if it slides outside. `owner` remembers the finger.
static StickAxis pollCStick(const Input& in, SDL_FingerID& owner) {
    StickAxis out;

    if (owner != NO_FINGER) {
        for (const auto& t : in.touches) {
            if (t.id == owner) {
                float dx = (t.x - cstickCx()) / static_cast<float>(CSTICK_R);
                float dy = (t.y - cstickCy()) / static_cast<float>(CSTICK_R);
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1.0f) {  // keep it inside the circle
                    dx /= len;
                    dy /= len;
                }
                out.x = dx;
                out.y = dy;
                out.active = true;
                return out;
            }
        }
        owner = NO_FINGER;  // that finger lifted
    }

    for (const auto& t : in.touches) {
        float dx = t.x - cstickCx();
        float dy = t.y - cstickCy();
        float reach = CSTICK_R * 1.6f;
        if (dx * dx + dy * dy <= reach * reach) {
            owner = t.id;
            out.active = true;
            return out;
        }
    }
    return out;
}

static void drawCStick(SDL_Renderer* r, float ax, float ay) {
    int cx = cstickCx();
    int cy = cstickCy();

    SDL_SetRenderDrawColor(r, 30, 30, 38, 255);
    fillCircle(r, cx, cy, CSTICK_R + 12);
    SDL_SetRenderDrawColor(r, 52, 52, 62, 255);
    fillCircle(r, cx, cy, CSTICK_R + 5);
    SDL_SetRenderDrawColor(r, 22, 22, 28, 255);
    fillCircle(r, cx, cy, CSTICK_R);

    int nx = cx + static_cast<int>(ax * (CSTICK_R - 12));
    int ny = cy + static_cast<int>(ay * (CSTICK_R - 12));
    SDL_SetRenderDrawColor(r, 210, 210, 230, 255);
    fillCircle(r, nx, ny, 20);
}
