#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>

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

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("OoT3D Port", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 800, 480, SDL_WINDOW_SHOWN);
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

    std::string headerPath = "header.bin";
#ifdef __ANDROID__
    const char* base = SDL_AndroidGetInternalStoragePath();
    if (base) headerPath = std::string(base) + "/header.bin";
#endif

    unsigned char header[HEADER_SIZE];
    std::memset(header, 0, sizeof(header));
    size_t headerLen = 0;
    State state = State::Waiting;
    int frame = 0;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }

        // Check about twice a second until a file has been chosen.
        if (state == State::Waiting && frame % 30 == 0) {
            if (loadHeader(headerPath, header, headerLen)) {
                state = looksLike3dsGame(header, headerLen) ? State::Valid : State::Invalid;
            }
        }
        frame++;

        switch (state) {
            case State::Waiting: SDL_SetRenderDrawColor(renderer, 15, 20, 60, 255); break;
            case State::Valid:   SDL_SetRenderDrawColor(renderer, 10, 90, 40, 255); break;
            case State::Invalid: SDL_SetRenderDrawColor(renderer, 100, 25, 25, 255); break;
        }
        SDL_RenderClear(renderer);

        // One square per header byte (offsets 0x100 to 0x11F), brightness = byte value.
        if (headerLen >= 0x120) {
            int w = 0, h = 0;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            int cell = w / 36;
            int startX = (w - cell * 32) / 2;
            int y = h / 2 - cell / 2;
            for (int i = 0; i < 32; i++) {
                Uint8 v = header[0x100 + i];
                SDL_SetRenderDrawColor(renderer, v, v, v, 255);
                SDL_Rect r = {startX + i * cell, y, cell - 2, cell - 2};
                SDL_RenderFillRect(renderer, &r);
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
