#pragma once
#include <SDL.h>

#include "controls.h"
#include "hid.h"

// Graphics settings. Everything here is applied in main.cpp.

struct Settings {
    int resIdx = 0;     // index into kResHeights
    int fpsIdx = 1;     // 0 = 30, 1 = 60, 2 = uncapped
    int filterIdx = 0;  // 0 = nearest, 1 = smooth
    bool open = false;
};

static const int kResHeights[5] = {240, 480, 720, 960, 1080};
static const char* kResNames[5] = {"240p", "480p", "720p", "960p", "1080p"};
static const char* kFpsNames[3] = {"30", "60", "Uncapped"};
static const char* kFilterNames[2] = {"Nearest", "Smooth"};

static inline bool pointInRect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

// The gear button, top-right corner of the canvas.
static inline SDL_Rect settingsButtonRect() {
    SDL_Rect r;
    r.x = gL.vw - 88;
    r.y = 10;
    r.w = 78;
    r.h = 48;
    return r;
}

static inline int settingsPanelW() {
    int w = gL.vw - 120;
    if (w > 640) w = 640;
    return w;
}
static inline int settingsPanelH() {
    int h = gL.vh - 40;
    if (h > 520) h = 520;
    return h;
}
static inline int settingsPanelX() { return (gL.vw - settingsPanelW()) / 2; }
static inline int settingsPanelY() { return (gL.vh - settingsPanelH()) / 2; }

// Four rows: resolution, fps limit, filter, back.
static inline SDL_Rect settingsRowRect(int i) {
    SDL_Rect r;
    r.x = settingsPanelX() + 24;
    r.y = settingsPanelY() + 92 + i * 76;
    r.w = settingsPanelW() - 48;
    r.h = 64;
    return r;
}

static void drawSettingsButton(SDL_Renderer* r, bool open) {
    SDL_Rect b = settingsButtonRect();
    if (open) {
        SDL_SetRenderDrawColor(r, 88, 88, 112, 255);
    } else {
        SDL_SetRenderDrawColor(r, 44, 44, 56, 255);
    }
    SDL_RenderFillRect(r, &b);
    SDL_SetRenderDrawColor(r, 215, 215, 230, 255);
    drawText(r, "SET", b.x + b.w / 2, b.y + b.h / 2, 3);
}

static void drawSettingsPanel(SDL_Renderer* r, const Settings& st) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    SDL_Rect full;
    full.x = 0;
    full.y = 0;
    full.w = gL.vw;
    full.h = gL.vh;
    SDL_RenderFillRect(r, &full);

    SDL_Rect p;
    p.x = settingsPanelX();
    p.y = settingsPanelY();
    p.w = settingsPanelW();
    p.h = settingsPanelH();
    SDL_SetRenderDrawColor(r, 26, 26, 34, 255);
    SDL_RenderFillRect(r, &p);

    SDL_SetRenderDrawColor(r, 88, 88, 112, 255);
    SDL_Rect accent;
    accent.x = p.x;
    accent.y = p.y;
    accent.w = p.w;
    accent.h = 3;
    SDL_RenderFillRect(r, &accent);

    SDL_SetRenderDrawColor(r, 235, 235, 245, 255);
    drawText(r, "GRAPHICS", p.x + p.w / 2, p.y + 46, 4);

    const char* labels[4] = {"RESOLUTION", "FPS LIMIT", "FILTER", "BACK"};
    const char* values[4] = {kResNames[st.resIdx], kFpsNames[st.fpsIdx],
                             kFilterNames[st.filterIdx], ""};

    for (int i = 0; i < 4; i++) {
        SDL_Rect row = settingsRowRect(i);

        SDL_SetRenderDrawColor(r, 38, 38, 48, 255);
        SDL_RenderFillRect(r, &row);

        SDL_SetRenderDrawColor(r, 210, 210, 228, 255);
        if (i == 3) {
            drawText(r, labels[i], row.x + row.w / 2, row.y + row.h / 2, 3);
        } else {
            drawText(r, labels[i], row.x + row.w / 4, row.y + row.h / 2, 3);
            SDL_SetRenderDrawColor(r, 120, 220, 150, 255);
            drawText(r, values[i], row.x + (row.w * 3) / 4, row.y + row.h / 2, 3);
        }
    }
}
