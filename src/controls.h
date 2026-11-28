#pragma once
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "display.h"
#include "hid.h"

// ---------------------------------------------------------------------------
// Layout
//
// The canvas is always 720 units tall. Its width follows the phone's shape, so
// a tall/wide phone gets a wider canvas instead of black bars. That lets us
// push the controls out to the edges, where your thumbs actually are.
// ---------------------------------------------------------------------------
struct Ctrl {
    uint32_t bit;
    SDL_Rect rect;
    bool round;
    const char* label;
};

struct Layout {
    int vw = 1280;
    int vh = 720;
    SDL_Rect top = {0, 0, 0, 0};  // top 3DS screen
    SDL_Rect bot = {0, 0, 0, 0};  // bottom 3DS screen
    int cpX = 0, cpY = 0, cpR = 100;  // circle pad
    int dpadX = 0, dpadY = 0;         // centre of the D-pad cross
    int faceX = 0, faceY = 0;         // centre of the A/B/X/Y diamond
    std::vector<Ctrl> ctrls;          // rectangular + round buttons
};

constexpr int FACE_R = 52;     // face button radius
constexpr int FACE_OFF = 76;   // distance from diamond centre to each face button
constexpr int DPAD_ARM = 76;   // size of one D-pad arm
constexpr int TOUCH_PAD_X = 4;   // extra touch margin around rectangular buttons
constexpr int TOUCH_PAD_Y = 14;  // (bigger vertically, so shoulder buttons are easy)

static Layout gL;

static void computeLayout(int winW, int winH) {
    Layout& L = gL;
    float aspect = (winW > 0 && winH > 0) ? float(winW) / float(winH) : 16.0f / 9.0f;
    L.vh = 720;
    L.vw = std::min(2600, std::max(1280, int(720.0f * aspect + 0.5f)));
    const int vw = L.vw;
    const int M = 24;  // margin from the screen edge

    // Screens, centred at the top.
    const int topW = 520, topH = 312, botW = 416, botH = 312, gap = 24, scrY = 36;
    const int sx = (vw - (topW + gap + botW)) / 2;
    L.top = {sx, scrY, topW, topH};
    L.bot = {sx + topW + gap, scrY, botW, botH};

    // Shoulder + SELECT/START row, just under the screens.
    const int rowY = 372, rowH = 56;

    // Left thumb: circle pad and D-pad.
    L.cpR = 100;
    L.cpX = M + 130;
    L.cpY = 582;
    L.dpadX = L.cpX + 270;
    L.dpadY = 590;

    // Right thumb: face buttons.
    L.faceX = vw - M - 150;
    L.faceY = 580;

    auto faceRect = [&](int dx, int dy) {
        return SDL_Rect{L.faceX + dx - FACE_R, L.faceY + dy - FACE_R, FACE_R * 2, FACE_R * 2};
    };

    L.ctrls = {
        {BTN_L, {M, rowY, 170, rowH}, false, "L"},
        {BTN_ZL, {M + 180, rowY, 110, rowH}, false, "ZL"},
        {BTN_R, {vw - M - 170, rowY, 170, rowH}, false, "R"},
        {BTN_ZR, {vw - M - 170 - 10 - 110, rowY, 110, rowH}, false, "ZR"},

        {BTN_SELECT, {vw / 2 - 150, rowY, 140, rowH}, false, "SELECT"},
        {BTN_START, {vw / 2 + 10, rowY, 140, rowH}, false, "START"},

        {BTN_X, faceRect(0, -FACE_OFF), true, "X"},
        {BTN_B, faceRect(0, FACE_OFF), true, "B"},
        {BTN_Y, faceRect(-FACE_OFF, 0), true, "Y"},
        {BTN_A, faceRect(FACE_OFF, 0), true, "A"},
    };
}

// Call at start-up and whenever the window changes size.
static void applyLayout(SDL_Renderer* r) {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(r, &w, &h);
    computeLayout(w, h);
    SDL_RenderSetLogicalSize(r, gL.vw, gL.vh);
}

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

static void fillRoundRect(SDL_Renderer* r, const SDL_Rect& rc, int rad) {
    rad = std::min(rad, std::min(rc.w, rc.h) / 2);
    SDL_Rect mid = {rc.x, rc.y + rad, rc.w, rc.h - 2 * rad};
    SDL_Rect up = {rc.x + rad, rc.y, rc.w - 2 * rad, rad};
    SDL_Rect dn = {rc.x + rad, rc.y + rc.h - rad, rc.w - 2 * rad, rad};
    SDL_RenderFillRect(r, &mid);
    SDL_RenderFillRect(r, &up);
    SDL_RenderFillRect(r, &dn);
    fillCircle(r, rc.x + rad, rc.y + rad, rad);
    fillCircle(r, rc.x + rc.w - rad - 1, rc.y + rad, rad);
    fillCircle(r, rc.x + rad, rc.y + rc.h - rad - 1, rad);
    fillCircle(r, rc.x + rc.w - rad - 1, rc.y + rc.h - rad - 1, rad);
}

// dir: 0 up, 1 down, 2 left, 3 right
static void drawArrow(SDL_Renderer* r, int cx, int cy, int dir) {
    for (int i = 0; i < 14; i++) {
        switch (dir) {
            case 0: SDL_RenderDrawLine(r, cx - i, cy - 10 + i, cx + i, cy - 10 + i); break;
            case 1: SDL_RenderDrawLine(r, cx - i, cy + 10 - i, cx + i, cy + 10 - i); break;
            case 2: SDL_RenderDrawLine(r, cx - 10 + i, cy - i, cx - 10 + i, cy + i); break;
            case 3: SDL_RenderDrawLine(r, cx + 10 - i, cy - i, cx + 10 - i, cy + i); break;
        }
    }
}

// ---- Input collection ----
struct TouchPoint {
    SDL_FingerID id;
    float x;  // position on the canvas
    float y;
};

constexpr SDL_FingerID MOUSE_ID = -1000;
constexpr SDL_FingerID NO_FINGER = -2000;

struct Input {
    std::vector<TouchPoint> touches;
    std::vector<SDL_GameController*> pads;
    SDL_FingerID padId = NO_FINGER;  // the finger currently steering the circle pad

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
        if (id == padId) padId = NO_FINGER;
        for (size_t i = 0; i < touches.size(); i++) {
            if (touches[i].id == id) {
                touches.erase(touches.begin() + static_cast<long>(i));
                return;
            }
        }
    }

    // A finger that lands near the circle pad owns it until it lifts, even if it
    // slides far outside the pad. That keeps steering smooth.
    void tryCapturePad(SDL_FingerID id, float x, float y) {
        if (padId != NO_FINGER) return;
        float dx = x - gL.cpX;
        float dy = y - gL.cpY;
        float reach = gL.cpR * 1.7f;
        if (dx * dx + dy * dy <= reach * reach) padId = id;
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
            if (e.type == SDL_FINGERDOWN) in.tryCapturePad(e.tfinger.fingerId, vx, vy);
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
                in.tryCapturePad(MOUSE_ID, vx, vy);
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
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                in.touches.clear();
                in.padId = NO_FINGER;
            }
            break;
        default:
            break;
    }
}

// ---- Hit testing ----
static bool hitRect(const SDL_Rect& rc, float x, float y) {
    return x >= rc.x - TOUCH_PAD_X && x < rc.x + rc.w + TOUCH_PAD_X &&
           y >= rc.y - TOUCH_PAD_Y && y < rc.y + rc.h + TOUCH_PAD_Y;
}

// Face buttons: the nearest button wins, with a generous reach, so there are no
// dead gaps between A/B/X/Y and sliding from one to the next just works.
static uint32_t faceAt(float x, float y) {
    uint32_t best = 0;
    float bestD = 1e9f;
    for (const auto& c : gL.ctrls) {
        if (!c.round) continue;
        float dx = x - (c.rect.x + c.rect.w * 0.5f);
        float dy = y - (c.rect.y + c.rect.h * 0.5f);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < bestD) {
            bestD = d;
            best = c.bit;
        }
    }
    return bestD <= FACE_R + 22.0f ? best : 0;
}

// D-pad: one touch area for the whole cross. Diagonals press two directions.
static uint32_t dpadAt(float x, float y) {
    float dx = x - gL.dpadX;
    float dy = y - gL.dpadY;
    const float reach = DPAD_ARM * 1.5f + 16.0f;
    if (std::fabs(dx) > reach || std::fabs(dy) > reach) return 0;
    if (std::sqrt(dx * dx + dy * dy) < 22.0f) return 0;  // dead centre
    float ax = std::fabs(dx);
    float ay = std::fabs(dy);
    uint32_t bits = 0;
    if (ay > ax * 0.414f) bits |= (dy < 0) ? BTN_DUP : BTN_DDOWN;
    if (ax > ay * 0.414f) bits |= (dx < 0) ? BTN_DLEFT : BTN_DRIGHT;
    return bits;
}

// Turns raw touches and gamepads into the 3DS-style state the game reads.
static void updateHid(HidState& hid, const Input& in) {
    const Layout& L = gL;
    uint32_t held = 0;
    float cx = 0.0f;
    float cy = 0.0f;
    bool touching = false;
    int tx = 0;
    int ty = 0;

    for (const auto& t : in.touches) {
        // The finger that owns the circle pad only steers; it presses nothing else.
        if (t.id == in.padId) {
            float nx = (t.x - L.cpX) / L.cpR;
            float ny = -(t.y - L.cpY) / L.cpR;
            float len = std::sqrt(nx * nx + ny * ny);
            if (len > 1.0f) {
                nx /= len;
                ny /= len;
                len = 1.0f;
            }
            if (len > 0.10f) {  // small dead zone
                cx = nx;
                cy = ny;
            }
            continue;
        }

        for (const auto& c : L.ctrls) {
            if (!c.round && hitRect(c.rect, t.x, t.y)) held |= c.bit;
        }
        held |= faceAt(t.x, t.y);
        held |= dpadAt(t.x, t.y);

        if (!touching && t.x >= L.bot.x && t.x < L.bot.x + L.bot.w && t.y >= L.bot.y &&
            t.y < L.bot.y + L.bot.h) {
            touching = true;
            tx = static_cast<int>((t.x - L.bot.x) * BOT_W / L.bot.w);
            ty = static_cast<int>((t.y - L.bot.y) * BOT_H / L.bot.h);
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

// ---- Drawing ----
static void drawControls(SDL_Renderer* r, const HidState& hid) {
    const Layout& L = gL;

    // Circle pad
    SDL_SetRenderDrawColor(r, 62, 62, 74, 255);
    fillCircle(r, L.cpX, L.cpY, L.cpR);
    SDL_SetRenderDrawColor(r, 26, 26, 32, 255);
    fillCircle(r, L.cpX, L.cpY, L.cpR - 6);
    bool steering = hid.circleX != 0.0f || hid.circleY != 0.0f;
    int knobX = L.cpX + static_cast<int>(hid.circleX * (L.cpR - 38));
    int knobY = L.cpY - static_cast<int>(hid.circleY * (L.cpR - 38));
    if (steering) SDL_SetRenderDrawColor(r, 196, 196, 214, 255);
    else SDL_SetRenderDrawColor(r, 150, 150, 168, 255);
    fillCircle(r, knobX, knobY, 44);

    // D-pad
    struct Arm {
        uint32_t bit;
        int dx, dy, dir;
    };
    const Arm arms[] = {{BTN_DUP, 0, -1, 0}, {BTN_DDOWN, 0, 1, 1}, {BTN_DLEFT, -1, 0, 2},
                        {BTN_DRIGHT, 1, 0, 3}};
    const int half = DPAD_ARM / 2;
    SDL_Rect centre = {L.dpadX - half, L.dpadY - half, DPAD_ARM, DPAD_ARM};
    SDL_SetRenderDrawColor(r, 80, 80, 92, 255);
    SDL_RenderFillRect(r, &centre);
    SDL_SetRenderDrawColor(r, 36, 36, 44, 255);
    SDL_Rect centreIn = {centre.x + 4, centre.y + 4, centre.w - 8, centre.h - 8};
    SDL_RenderFillRect(r, &centreIn);
    for (const auto& a : arms) {
        bool pressed = (hid.held & a.bit) != 0;
        SDL_Rect rc = {L.dpadX - half + a.dx * DPAD_ARM, L.dpadY - half + a.dy * DPAD_ARM,
                       DPAD_ARM, DPAD_ARM};
        SDL_SetRenderDrawColor(r, 80, 80, 92, 255);
        fillRoundRect(r, rc, 12);
        SDL_Rect in = {rc.x + 4, rc.y + 4, rc.w - 8, rc.h - 8};
        if (pressed) SDL_SetRenderDrawColor(r, 235, 235, 245, 255);
        else SDL_SetRenderDrawColor(r, 36, 36, 44, 255);
        fillRoundRect(r, in, 9);
        if (pressed) SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
        else SDL_SetRenderDrawColor(r, 215, 215, 230, 255);
        drawArrow(r, rc.x + rc.w / 2, rc.y + rc.h / 2, a.dir);
    }

    // Other buttons
    for (const auto& c : L.ctrls) {
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
            fillRoundRect(r, c.rect, 14);
            SDL_Rect inner = {c.rect.x + 4, c.rect.y + 4, c.rect.w - 8, c.rect.h - 8};
            if (pressed) SDL_SetRenderDrawColor(r, 235, 235, 245, 255);
            else SDL_SetRenderDrawColor(r, 36, 36, 44, 255);
            fillRoundRect(r, inner, 11);
        }

        if (pressed) SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
        else SDL_SetRenderDrawColor(r, 215, 215, 230, 255);

        if (c.label) {
            int scale = c.round ? 5 : (std::strlen(c.label) > 2 ? 3 : 4);
            drawText(r, c.label, cx, cy, scale);
        }
    }
}
