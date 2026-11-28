#pragma once
#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "display.h"
#include "hid.h"

// Everything is laid out on a fixed 1280x720 canvas. SDL scales it to any phone.
constexpr int VW = 1280;
constexpr int VH = 720;

static const SDL_Rect TOP_RECT = {80, 60, 600, 360};
static const SDL_Rect BOT_RECT = {720, 60, 480, 360};

constexpr int CP_X = 170;  // circle pad centre
constexpr int CP_Y = 580;
constexpr int CP_R = 85;

struct Ctrl {
    uint32_t bit;
    SDL_Rect rect;
    bool round;
    const char* label;
};

static const Ctrl kCtrls[] = {
    {BTN_L, {80, 12, 160, 38}, false, "L"},
    {BTN_ZL, {250, 12, 100, 38}, false, "ZL"},
    {BTN_ZR, {930, 12, 100, 38}, false, "ZR"},
    {BTN_R, {1040, 12, 160, 38}, false, "R"},

    {BTN_DUP, {390, 490, 60, 60}, false, nullptr},
    {BTN_DDOWN, {390, 610, 60, 60}, false, nullptr},
    {BTN_DLEFT, {330, 550, 60, 60}, false, nullptr},
    {BTN_DRIGHT, {450, 550, 60, 60}, false, nullptr},

    {BTN_X, {965, 465, 80, 80}, true, "X"},
    {BTN_B, {965, 615, 80, 80}, true, "B"},
    {BTN_Y, {890, 540, 80, 80}, true, "Y"},
    {BTN_A, {1040, 540, 80, 80}, true, "A"},

    {BTN_SELECT, {560, 630, 130, 48}, false, "SELECT"},
    {BTN_START, {710, 630, 130, 48}, false, "START"},
};

// ---- Tiny 5x7 pixel font (just the letters we need) ----
struct Glyph {
    char c;
    uint8_t rows[7];
};

static const Glyph kGlyphs[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

static const uint8_t* findGlyph(char c) {
    for (const auto& g : kGlyphs) {
        if (g.c == c) return g.rows;
    }
    return nullptr;
}

static void drawText(SDL_Renderer* r, const char* text, int cx, int cy, int scale) {
    int n = static_cast<int>(std::strlen(text));
    int width = n * 6 * scale - scale;
    int x = cx - width / 2;
    int y = cy - (7 * scale) / 2;
    for (int i = 0; i < n; i++) {
        const uint8_t* rows = findGlyph(text[i]);
        if (rows) {
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if (rows[row] & (1 << (4 - col))) {
                        SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(r, &px);
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

static void fillCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(rad * rad - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// dir: 0 up, 1 down, 2 left, 3 right
static void drawArrow(SDL_Renderer* r, int cx, int cy, int dir) {
    for (int i = 0; i < 12; i++) {
        switch (dir) {
            case 0: SDL_RenderDrawLine(r, cx - i, cy - 8 + i, cx + i, cy - 8 + i); break;
            case 1: SDL_RenderDrawLine(r, cx - i, cy + 8 - i, cx + i, cy + 8 - i); break;
            case 2: SDL_RenderDrawLine(r, cx - 8 + i, cy - i, cx - 8 + i, cy + i); break;
            case 3: SDL_RenderDrawLine(r, cx + 8 - i, cy - i, cx + 8 - i, cy + i); break;
        }
    }
}

// ---- Input collection ----
struct TouchPoint {
    SDL_FingerID id;
    float x;  // position on the 1280x720 canvas
    float y;
};

constexpr SDL_FingerID MOUSE_ID = -1000;

struct Input {
    std::vector<TouchPoint> touches;
    std::vector<SDL_GameController*> pads;

    void setTouch(SDL_FingerID id, float x, float y) {
        for (auto& t : touches) {
            if (t.id == id) {
                t.x = x;
                t.y = y;
                return;
            }
        }
        touches.push_back({id, x, y});
    }

    void removeTouch(SDL_FingerID id) {
        for (size_t i = 0; i < touches.size(); i++) {
            if (touches[i].id == id) {
                touches.erase(touches.begin() + static_cast<long>(i));
                return;
            }
        }
    }
};

static void handleEvent(Input& in, SDL_Renderer* r, SDL_Window* window, const SDL_Event& e) {
    switch (e.type) {
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION: {
            int ww = 1, wh = 1;
            SDL_GetWindowSize(window, &ww, &wh);
            float vx = 0, vy = 0;
            SDL_RenderWindowToLogical(r, static_cast<int>(e.tfinger.x * ww),
                                      static_cast<int>(e.tfinger.y * wh), &vx, &vy);
            in.setTouch(e.tfinger.fingerId, vx, vy);
            break;
        }
        case SDL_FINGERUP:
            in.removeTouch(e.tfinger.fingerId);
            break;
#ifndef __ANDROID__
        // On desktop, the mouse acts like one finger (handy for testing).
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                float vx = 0, vy = 0;
                SDL_RenderWindowToLogical(r, e.button.x, e.button.y, &vx, &vy);
                in.setTouch(MOUSE_ID, vx, vy);
            }
            break;
        case SDL_MOUSEMOTION:
            if (e.motion.state & SDL_BUTTON_LMASK) {
                float vx = 0, vy = 0;
                SDL_RenderWindowToLogical(r, e.motion.x, e.motion.y, &vx, &vy);
                in.setTouch(MOUSE_ID, vx, vy);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) in.removeTouch(MOUSE_ID);
            break;
#endif
        case SDL_CONTROLLERDEVICEADDED: {
            SDL_GameController* c = SDL_GameControllerOpen(e.cdevice.which);
            if (c) in.pads.push_back(c);
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED: {
            SDL_GameController* c = SDL_GameControllerFromInstanceID(e.cdevice.which);
            for (size_t i = 0; i < in.pads.size(); i++) {
                if (in.pads[i] == c) {
                    in.pads.erase(in.pads.begin() + static_cast<long>(i));
                    break;
                }
            }
            if (c) SDL_GameControllerClose(c);
            break;
        }
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) in.touches.clear();
            break;
        default:
            break;
    }
}

static bool hit(const Ctrl& c, float x, float y) {
    if (c.round) {
        float cx = c.rect.x + c.rect.w / 2.0f;
        float cy = c.rect.y + c.rect.h / 2.0f;
        float rad = c.rect.w / 2.0f;
        float dx = x - cx;
        float dy = y - cy;
        return dx * dx + dy * dy <= rad * rad;
    }
    return x >= c.rect.x && x < c.rect.x + c.rect.w && y >= c.rect.y &&
           y < c.rect.y + c.rect.h;
}

// Turns raw touches and gamepads into the 3DS-style state the game reads.
static void updateHid(HidState& hid, const Input& in) {
    uint32_t held = 0;
    float cx = 0.0f;
    float cy = 0.0f;
    bool touching = false;
    int tx = 0;
    int ty = 0;

    for (const auto& t : in.touches) {
        for (const auto& c : kCtrls) {
            if (hit(c, t.x, t.y)) held |= c.bit;
        }

        float dx = t.x - CP_X;
        float dy = t.y - CP_Y;
        if (std::sqrt(dx * dx + dy * dy) <= CP_R * 1.6f) {
            float nx = dx / CP_R;
            float ny = -dy / CP_R;
            float len = std::sqrt(nx * nx + ny * ny);
            if (len > 1.0f) {
                nx /= len;
                ny /= len;
            }
            cx = nx;
            cy = ny;
        }

        if (!touching && t.x >= BOT_RECT.x && t.x < BOT_RECT.x + BOT_RECT.w &&
            t.y >= BOT_RECT.y && t.y < BOT_RECT.y + BOT_RECT.h) {
            touching = true;
            tx = static_cast<int>((t.x - BOT_RECT.x) * BOT_W / BOT_RECT.w);
            ty = static_cast<int>((t.y - BOT_RECT.y) * BOT_H / BOT_RECT.h);
        }
    }

    for (SDL_GameController* pad : in.pads) {
        auto btn = [&](SDL_GameControllerButton b) {
            return SDL_GameControllerGetButton(pad, b) != 0;
        };
        // 3DS buttons are named by position: A is on the right, B at the bottom.
        if (btn(SDL_CONTROLLER_BUTTON_B)) held |= BTN_A;
        if (btn(SDL_CONTROLLER_BUTTON_A)) held |= BTN_B;
        if (btn(SDL_CONTROLLER_BUTTON_Y)) held |= BTN_X;
        if (btn(SDL_CONTROLLER_BUTTON_X)) held |= BTN_Y;
        if (btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) held |= BTN_L;
        if (btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) held |= BTN_R;
        if (btn(SDL_CONTROLLER_BUTTON_START)) held |= BTN_START;
        if (btn(SDL_CONTROLLER_BUTTON_BACK)) held |= BTN_SELECT;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_UP)) held |= BTN_DUP;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) held |= BTN_DDOWN;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) held |= BTN_DLEFT;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) held |= BTN_DRIGHT;

        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000)
            held |= BTN_ZL;
        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000)
            held |= BTN_ZR;

        float sx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
        float sy = -SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
        if (std::sqrt(sx * sx + sy * sy) > 0.2f) {
            cx = sx;
            cy = sy;
        }
    }

    hid.down = held & ~hid.held;
    hid.up = hid.held & ~held;
    hid.held = held;
    hid.circleX = cx;
    hid.circleY = cy;
    hid.touching = touching;
    hid.touchX = tx;
    hid.touchY = ty;
}

static void drawControls(SDL_Renderer* r, const HidState& hid) {
    // Circle pad
    SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
    fillCircle(r, CP_X, CP_Y, CP_R);
    SDL_SetRenderDrawColor(r, 28, 28, 34, 255);
    fillCircle(r, CP_X, CP_Y, CP_R - 6);
    int knobX = CP_X + static_cast<int>(hid.circleX * (CP_R - 30));
    int knobY = CP_Y - static_cast<int>(hid.circleY * (CP_R - 30));
    SDL_SetRenderDrawColor(r, 150, 150, 168, 255);
    fillCircle(r, knobX, knobY, 38);

    // Buttons
    for (const auto& c : kCtrls) {
        bool pressed = (hid.held & c.bit) != 0;
        int cx = c.rect.x + c.rect.w / 2;
        int cy = c.rect.y + c.rect.h / 2;

        if (c.round) {
            SDL_SetRenderDrawColor(r, 80, 80, 92, 255);
            fillCircle(r, cx, cy, c.rect.w / 2);
            if (pressed) SDL_SetRenderDrawColor(r, 235, 235, 245, 255);
            else SDL_SetRenderDrawColor(r, 36, 36, 44, 255);
            fillCircle(r, cx, cy, c.rect.w / 2 - 4);
        } else {
            SDL_SetRenderDrawColor(r, 80, 80, 92, 255);
            SDL_RenderFillRect(r, &c.rect);
            SDL_Rect inner = {c.rect.x + 4, c.rect.y + 4, c.rect.w - 8, c.rect.h - 8};
            if (pressed) SDL_SetRenderDrawColor(r, 235, 235, 245, 255);
            else SDL_SetRenderDrawColor(r, 36, 36, 44, 255);
            SDL_RenderFillRect(r, &inner);
        }

        if (pressed) SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
        else SDL_SetRenderDrawColor(r, 215, 215, 230, 255);

        if (c.label) {
            drawText(r, c.label, cx, cy, c.round ? 5 : 3);
        } else if (c.bit == BTN_DUP) {
            drawArrow(r, cx, cy, 0);
        } else if (c.bit == BTN_DDOWN) {
            drawArrow(r, cx, cy, 1);
        } else if (c.bit == BTN_DLEFT) {
            drawArrow(r, cx, cy, 2);
        } else if (c.bit == BTN_DRIGHT) {
            drawArrow(r, cx, cy, 3);
        }
    }
}
