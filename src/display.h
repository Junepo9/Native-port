#pragma once
#include <SDL.h>
#include <cstdint>
#include <vector>

constexpr int TOP_W = 400;
constexpr int TOP_H = 240;
constexpr int BOT_W = 320;
constexpr int BOT_H = 240;

static inline uint32_t rgb(int r, int g, int b) {
    return 0xFF000000u | (uint32_t(r & 255) << 16) | (uint32_t(g & 255) << 8) |
           uint32_t(b & 255);
}

// One 3DS screen: a CPU-side pixel buffer uploaded to a texture each frame.
// A game renderer will eventually draw into these buffers.
struct Screen {
    int w = 0;
    int h = 0;
    SDL_Texture* tex = nullptr;
    std::vector<uint32_t> pixels;  // 0xAARRGGBB

    bool create(SDL_Renderer* renderer, int width, int height) {
        w = width;
        h = height;
        pixels.assign(static_cast<size_t>(w) * h, 0xFF000000u);
        tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
        if (tex) SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
        return tex != nullptr;
    }

    void destroy() {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
    }

    void clear(uint32_t color) {
        for (auto& p : pixels) p = color;
    }

    void fillRect(int x, int y, int rw, int rh, uint32_t color) {
        for (int yy = y; yy < y + rh; yy++) {
            if (yy < 0 || yy >= h) continue;
            for (int xx = x; xx < x + rw; xx++) {
                if (xx < 0 || xx >= w) continue;
                pixels[static_cast<size_t>(yy) * w + xx] = color;
            }
        }
    }

    void upload() {
        if (tex) SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);
    }
};
