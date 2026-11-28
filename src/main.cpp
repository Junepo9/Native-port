#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "controls.h"
#include "cstick.h"
#include "display.h"
#include "hid.h"
#include "scene.h"
#include "settings.h"

enum class State { Waiting, Valid, Invalid };

static const size_t HEADER_SIZE = 512;

static bool loadHeader(const std::string& path, unsigned char* buf, size_t& len) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    len = std::fread(buf, 1, HEADER_SIZE, f);
    std::fclose(f);
    return len > 0;
}

static bool looksLike3dsGame(const unsigned char* buf, size_t len) {
    if (len < 0x104) return false;
    return std::memcmp(buf + 0x100, "NCSD", 4) == 0 ||
           std::memcmp(buf + 0x100, "NCCH", 4) == 0;
}

static uint32_t tintFor(State s, int y, int h) {
    int lift = y * 40 / h;
    switch (s) {
        case State::Valid: return rgb(lift, 70 + lift, 30 + lift);
        case State::Invalid: return rgb(80 + lift, 10 + lift, 10 + lift);
        default: return rgb(lift, lift, 60 + lift);
    }
}

// Rebuild both screen buffers at the chosen resolution.
static bool makeScreens(SDL_Renderer* r, Screen& top, Screen& bottom, const Settings& st) {
    float scale = static_cast<float>(kResHeights[st.resIdx]) / 240.0f;

    int tw = static_cast<int>(400.0f * scale + 0.5f);
    int th = static_cast<int>(240.0f * scale + 0.5f);
    int bw = static_cast<int>(320.0f * scale + 0.5f);
    int bh = static_cast<int>(240.0f * scale + 0.5f);

    top.destroy();
    bottom.destroy();

    if (!top.create(r, tw, th)) return false;
    if (!bottom.create(r, bw, bh)) {
        top.destroy();
        return false;
    }

    SDL_ScaleMode mode = (st.filterIdx == 1) ? SDL_ScaleModeLinear : SDL_ScaleModeNearest;
    if (top.tex) SDL_SetTextureScaleMode(top.tex, mode);
    if (bottom.tex) SDL_SetTextureScaleMode(bottom.tex, mode);
    return true;
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("OoT3D Port", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024,
                         576, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // No PRESENTVSYNC: we pace frames ourselves so 30/60/uncapped all work.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    applyLayout(renderer);

    Settings st;

    Screen top;
    Screen bottom;
    if (!makeScreens(renderer, top, bottom, st)) {
        std::fprintf(stderr, "Screen create failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    int appliedRes = st.resIdx;
    int appliedFilter = st.filterIdx;

    std::string headerPath = "header.bin";
#ifdef __ANDROID__
    const char* base = SDL_AndroidGetInternalStoragePath();
    if (base) headerPath = std::string(base) + "/header.bin";
#endif

    unsigned char header[HEADER_SIZE];
    std::memset(header, 0, sizeof(header));
    size_t headerLen = 0;
    State state = State::Waiting;
    long frame = 0;

    Input input;
    HidState hid;
    SDL_FingerID stickFinger = NO_FINGER;
    SDL_FingerID uiFinger = NO_FINGER;
    bool btnHeld = false;

    Camera cam;

    float boxX = 0.0f;
    float boxY = 0.0f;
    int colorIdx = 0;
    const uint32_t palette[4] = {rgb(255, 200, 0), rgb(0, 200, 255), rgb(255, 80, 120),
                                 rgb(120, 255, 120)};
    boxX = top.w * 0.75f;
    boxY = top.h * 0.82f;

    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();
    double acc = 0.0;
    const double dt = 1.0 / 60.0;

    bool running = true;
    while (running) {
        const Uint64 frameStart = SDL_GetPerformanceCounter();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                                              e.window.event == SDL_WINDOWEVENT_RESIZED)) {
                applyLayout(renderer);
            }
            handleEvent(input, renderer, window, e);
        }

        if (state == State::Waiting && frame % 30 == 0) {
            if (loadHeader(headerPath, header, headerLen)) {
                state = looksLike3dsGame(header, headerLen) ? State::Valid : State::Invalid;
            }
        }
        frame++;

        // --- Settings: the SET button ------------------------------------
        bool onBtn = false;
        for (const auto& t : input.touches) {
            if (pointInRect(t.x, t.y, settingsButtonRect())) onBtn = true;
        }
        if (onBtn && !btnHeld) st.open = !st.open;
        btnHeld = onBtn;

        // --- Settings: taps on the rows ----------------------------------
        if (st.open) {
            int hitRow = -1;
            SDL_FingerID hitId = NO_FINGER;
            for (const auto& t : input.touches) {
                for (int i = 0; i < 4; i++) {
                    if (pointInRect(t.x, t.y, settingsRowRect(i))) {
                        hitRow = i;
                        hitId = t.id;
                        break;
                    }
                }
                if (hitRow >= 0) break;
            }

            if (hitRow >= 0 && hitId != uiFinger) {
                uiFinger = hitId;
                if (hitRow == 0) {
                    st.resIdx = (st.resIdx + 1) % 5;
                } else if (hitRow == 1) {
                    st.fpsIdx = (st.fpsIdx + 1) % 3;
                } else if (hitRow == 2) {
                    st.filterIdx = (st.filterIdx + 1) % 2;
                } else {
                    st.open = false;
                }
            }
            if (hitRow < 0) uiFinger = NO_FINGER;
        }

        // Apply a resolution or filter change now that input is settled.
        if (st.resIdx != appliedRes || st.filterIdx != appliedFilter) {
            if (makeScreens(renderer, top, bottom, st)) {
                appliedRes = st.resIdx;
                appliedFilter = st.filterIdx;
            }
        }

        // The C-stick steers the camera, unless the menu is open.
        StickAxis stick;
        if (!st.open) stick = pollCStick(input, stickFinger);

        // --- Fixed 60 Hz updates -----------------------------------------
        Uint64 now = SDL_GetPerformanceCounter();
        acc += static_cast<double>(now - last) / static_cast<double>(freq);
        last = now;
        if (acc > 0.25) acc = 0.25;
        while (acc >= dt) {
            if (!st.open) {
                updateHid(hid, input);

                if (stick.active) {
                    cam.yaw -= stick.x * 0.045f;
                    cam.pitch += stick.y * 0.035f;
                }
                if (cam.pitch < -0.15f) cam.pitch = -0.15f;
                if (cam.pitch > 1.25f) cam.pitch = 1.25f;

                float mx = hid.circleX;
                float my = -hid.circleY;
                if (hid.held & BTN_DLEFT) mx -= 1.0f;
                if (hid.held & BTN_DRIGHT) mx += 1.0f;
                if (hid.held & BTN_DUP) my -= 1.0f;
                if (hid.held & BTN_DDOWN) my += 1.0f;
                if (mx > 1.0f) mx = 1.0f;
                if (mx < -1.0f) mx = -1.0f;
                if (my > 1.0f) my = 1.0f;
                if (my < -1.0f) my = -1.0f;

                boxX += mx * 2.5f;
                boxY += my * 2.5f;
                if (boxX < 0) boxX = 0;
                if (boxY < 0) boxY = 0;
                if (boxX > top.w - 24) boxX = top.w - 24;
                if (boxY > top.h - 24) boxY = top.h - 24;

                if (hid.down & BTN_A) colorIdx = (colorIdx + 1) % 4;
            }
            acc -= dt;
        }

        float time = static_cast<float>(SDL_GetTicks()) * 0.001f;

        // --- Draw the top screen ------------------------------------------
        top.clearDepth();
        for (int y = 0; y < top.h; y++) top.fillRect(0, y, top.w, 1, tintFor(state, y, top.h));

        if (headerLen >= 0x120) {
            int cellW = top.w / 32;
            int cellH = (top.h * 10) / 240;
            if (cellW < 1) cellW = 1;
            if (cellH < 1) cellH = 1;
            for (int i = 0; i < 32; i++) {
                int v = header[0x100 + i];
                top.fillRect(i * cellW, 0, cellW - 1, cellH, rgb(v, v, v));
            }
        }

        drawTestScene(top, cam, time);

        top.fillRect(static_cast<int>(boxX), static_cast<int>(boxY), 24, 24, palette[colorIdx]);

        // --- Draw the bottom screen ---------------------------------------
        bottom.clear(rgb(20, 20, 26));
        for (int x = 0; x < bottom.w; x += 20) bottom.fillRect(x, 0, 1, bottom.h, rgb(40, 40, 50));
        for (int y = 0; y < bottom.h; y += 20) bottom.fillRect(0, y, bottom.w, 1, rgb(40, 40, 50));
        if (hid.touching) {
            int dotX = hid.touchX * bottom.w / BOT_W;
            int dotY = hid.touchY * bottom.h / BOT_H;
            bottom.fillRect(dotX - 6, dotY - 6, 12, 12, rgb(0, 220, 255));
        }

        top.upload();
        bottom.upload();

        // --- Present ------------------------------------------------------
        SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 10, 10, 12, 255);
        SDL_Rect bezelTop = {gL.top.x - 8, gL.top.y - 8, gL.top.w + 16, gL.top.h + 16};
        SDL_Rect bezelBot = {gL.bot.x - 8, gL.bot.y - 8, gL.bot.w + 16, gL.bot.h + 16};
        SDL_RenderFillRect(renderer, &bezelTop);
        SDL_RenderFillRect(renderer, &bezelBot);

        SDL_RenderCopy(renderer, top.tex, nullptr, &gL.top);
        SDL_RenderCopy(renderer, bottom.tex, nullptr, &gL.bot);

        drawControls(renderer, hid);
        drawCStick(renderer, stick.active ? stick.x : 0.0f, stick.active ? stick.y : 0.0f);
        drawSettingsButton(renderer, st.open);
        if (st.open) drawSettingsPanel(renderer, st);

        SDL_RenderPresent(renderer);

        // --- Frame pacing -------------------------------------------------
        if (st.fpsIdx < 2) {
            double budget = (st.fpsIdx == 0) ? (1.0 / 30.0) : (1.0 / 60.0);
            double spent = static_cast<double>(SDL_GetPerformanceCounter() - frameStart) /
                           static_cast<double>(freq);
            double wait = budget - spent;
            if (wait > 0.0) SDL_Delay(static_cast<Uint32>(wait * 1000.0));
        }
    }

    for (SDL_GameController* pad : input.pads) SDL_GameControllerClose(pad);
    top.destroy();
    bottom.destroy();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
