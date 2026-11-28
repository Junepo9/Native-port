#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "controls.h"
#include "display.h"
#include "hid.h"

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

static uint32_t tintFor(State s, int y) {
    int lift = y * 40 / TOP_H;
    switch (s) {
        case State::Valid: return rgb(lift, 70 + lift, 30 + lift);
        case State::Invalid: return rgb(80 + lift, 10 + lift, 10 + lift);
        default: return rgb(lift, lift, 60 + lift);
    }
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

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    applyLayout(renderer);

    Screen top;
    Screen bottom;
    if (!top.create(renderer, TOP_W, TOP_H) || !bottom.create(renderer, BOT_W, BOT_H)) {
        std::fprintf(stderr, "Screen create failed: %s\n", SDL_GetError());
        top.destroy();
        bottom.destroy();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    // Test scene state
    float boxX = 188.0f;
    float boxY = 108.0f;
    int colorIdx = 0;
    const uint32_t palette[4] = {rgb(255, 200, 0), rgb(0, 200, 255), rgb(255, 80, 120),
                                 rgb(120, 255, 120)};

    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();
    double acc = 0.0;
    const double dt = 1.0 / 60.0;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                                              e.window.event == SDL_WINDOWEVENT_RESIZED)) {
                applyLayout(renderer);
            }
            handleEvent(input, renderer, window, e);
        }

        // Check about twice a second until a game file has been chosen.
        if (state == State::Waiting && frame % 30 == 0) {
            if (loadHeader(headerPath, header, headerLen)) {
                state = looksLike3dsGame(header, headerLen) ? State::Valid : State::Invalid;
            }
        }
        frame++;

        // Fixed 60 Hz game updates.
        Uint64 now = SDL_GetPerformanceCounter();
        acc += static_cast<double>(now - last) / static_cast<double>(freq);
        last = now;
        if (acc > 0.25) acc = 0.25;
        while (acc >= dt) {
            updateHid(hid, input);

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
            if (boxY < 12) boxY = 12;
            if (boxX > TOP_W - 24) boxX = TOP_W - 24;
            if (boxY > TOP_H - 24) boxY = TOP_H - 24;

            if (hid.down & BTN_A) colorIdx = (colorIdx + 1) % 4;

            acc -= dt;
        }

        // Draw the top screen into its buffer.
        for (int y = 0; y < TOP_H; y++) top.fillRect(0, y, TOP_W, 1, tintFor(state, y));
        if (headerLen >= 0x120) {
            for (int i = 0; i < 32; i++) {
                int v = header[0x100 + i];
                top.fillRect(i * 12, 0, 11, 10, rgb(v, v, v));
            }
        }
        top.fillRect(static_cast<int>(boxX), static_cast<int>(boxY), 24, 24, palette[colorIdx]);

        // Draw the bottom screen into its buffer.
        bottom.clear(rgb(20, 20, 26));
        for (int x = 0; x < BOT_W; x += 20) bottom.fillRect(x, 0, 1, BOT_H, rgb(40, 40, 50));
        for (int y = 0; y < BOT_H; y += 20) bottom.fillRect(0, y, BOT_W, 1, rgb(40, 40, 50));
        if (hid.touching) {
            bottom.fillRect(hid.touchX - 6, hid.touchY - 6, 12, 12, rgb(0, 220, 255));
        }

        top.upload();
        bottom.upload();

        // Put everything on the window.
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
        SDL_RenderPresent(renderer);
    }

    for (SDL_GameController* pad : input.pads) SDL_GameControllerClose(pad);
    top.destroy();
    bottom.destroy();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
