#pragma once
#include <SDL.h>
#include <algorithm>
#include <cmath>
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

struct Screen {
    int w = 0;
    int h = 0;
    SDL_Texture* tex = nullptr;
    std::vector<uint32_t> pixels;  // 0xAARRGGBB
    std::vector<float> zbuf;       // 1.0 = far, 0.0 = at the eye

    bool create(SDL_Renderer* renderer, int width, int height) {
        w = width;
        h = height;
        pixels.assign(static_cast<size_t>(w) * h, 0xFF000000u);
        zbuf.assign(static_cast<size_t>(w) * h, 1.0f);
        tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
        if (tex) SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
        return tex != nullptr;
    }

    void destroy() {
        if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
    }

    void clear(uint32_t color) { std::fill(pixels.begin(), pixels.end(), color); }
    void clearDepth() { std::fill(zbuf.begin(), zbuf.end(), 1.0f); }

    void fillRect(int x, int y, int rw, int rh, uint32_t color) {
        for (int yy = y; yy < y + rh; yy++) {
            if (yy < 0 || yy >= h) continue;
            for (int xx = x; xx < x + rw; xx++) {
                if (xx < 0 || xx >= w) continue;
                pixels[static_cast<size_t>(yy) * w + xx] = color;
            }
        }
    }

    // Twice the signed area of triangle (a,b,p). Positive/negative = which side.
    static inline float edge(int ax, int ay, int bx, int by, float px, float py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }

    // Rasterize one triangle with a depth test. z: 0 = near, 1 = far.
    void fillTriangle(int x0, int y0, float z0,
                      int x1, int y1, float z1,
                      int x2, int y2, float z2,
                      uint32_t color) {
        float area = edge(x0, y0, x1, y1, x2, y2);
        if (area == 0.0f) return;
        if (area < 0.0f) {  // flip winding so "inside" is always positive
            std::swap(x1, x2); std::swap(y1, y2); std::swap(z1, z2);
            area = -area;
        }

        int minX = std::max(0, std::min(x0, std::min(x1, x2)));
        int maxX = std::min(w - 1, std::max(x0, std::max(x1, x2)));
        int minY = std::max(0, std::min(y0, std::min(y1, y2)));
        int maxY = std::min(h - 1, std::max(y0, std::max(y1, y2)));
        if (minX > maxX || minY > maxY) return;

        const float inv = 1.0f / area;

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float px = x + 0.5f, py = y + 0.5f;

                // Barycentric weights of this pixel inside the triangle
                float w0 = edge(x1, y1, x2, y2, px, py) * inv;
                float w1 = edge(x2, y2, x0, y0, px, py) * inv;
                float w2 = edge(x0, y0, x1, y1, px, py) * inv;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                float z = w0 * z0 + w1 * z1 + w2 * z2;
                size_t i = static_cast<size_t>(y) * w + x;
                if (z < zbuf[i]) {  // nearer than whatever is already here
                    zbuf[i] = z;
                    pixels[i] = color;
                }
            }
        }
    }

    void upload() {
        if (tex) SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);
    }
};
