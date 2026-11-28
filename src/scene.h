#pragma once
#include <cmath>

#include "display.h"

// A small test scene: a ground grid, a cube and an orbiting cube.
// All of this is your own math -- no libraries.

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

static inline Vec3 v3(float x, float y, float z) {
    Vec3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}
static inline Vec3 subV(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline float dotV(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 crossV(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline Vec3 normalizeV(Vec3 a) {
    float len = std::sqrt(dotV(a, a));
    if (len < 1e-6f) return v3(0.0f, 0.0f, 0.0f);
    return v3(a.x / len, a.y / len, a.z / len);
}

static const Vec3 kCubeVerts[8] = {
    v3(-1, -1, -1), v3(1, -1, -1), v3(1, 1, -1), v3(-1, 1, -1),
    v3(-1, -1,  1), v3(1, -1,  1), v3(1, 1,  1), v3(-1, 1,  1),
};

static const int kCubeTris[12][3] = {
    {0, 2, 1}, {0, 3, 2},   // back
    {4, 5, 6}, {4, 6, 7},   // front
    {0, 1, 5}, {0, 5, 4},   // bottom
    {3, 7, 6}, {3, 6, 2},   // top
    {0, 4, 7}, {0, 7, 3},   // left
    {1, 2, 6}, {1, 6, 5},   // right
};

static const Vec3 kLight = v3(-0.40f, 0.75f, -0.53f);

// Where the eye is and which way it looks.
struct Camera {
    float yaw = 0.6f;
    float pitch = 0.35f;
    float dist = 6.5f;
    float focal = 3.2f;
};

// World point -> view space (rotate the world, then push it away).
static inline Vec3 viewV(Vec3 p, const Camera& cam) {
    float cy = std::cos(cam.yaw), sy = std::sin(cam.yaw);
    float cp = std::cos(cam.pitch), sp = std::sin(cam.pitch);

    Vec3 a = v3(p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy);
    Vec3 b = v3(a.x, a.y * cp - a.z * sp, a.y * sp + a.z * cp);
    return v3(b.x, b.y, b.z + cam.dist);
}

// View space -> screen pixels. Now takes the buffer size so any
// resolution works. depth: 0 = near the eye, 1 = far away.
static inline void projectV(Vec3 p, float focal, int w, int h, int& sx, int& sy, float& depth) {
    float z = p.z;
    if (z < 0.1f) z = 0.1f;
    float f = focal * static_cast<float>(h) * 0.5f;
    sx = w / 2 + static_cast<int>(p.x * f / z);
    sy = h / 2 - static_cast<int>(p.y * f / z);
    const float nearZ = 0.5f, farZ = 20.0f;
    depth = (z - nearZ) / (farZ - nearZ);
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
}

// One pixel, tested against the depth buffer.
static inline void plotPx(Screen& s, int x, int y, float z, uint32_t color) {
    if (x < 0 || y < 0 || x >= s.w || y >= s.h) return;
    size_t i = static_cast<size_t>(y) * s.w + x;
    if (z < s.zbuf[i]) {
        s.zbuf[i] = z;
        s.pixels[i] = color;
    }
}

// A straight line between two projected points, depth-tested along the way.
static void drawLine3(Screen& s, int x0, int y0, float z0, int x1, int y1, float z1,
                      uint32_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int steps = (dx > dy) ? dx : dy;
    if (steps == 0) {
        plotPx(s, x0, y0, z0, color);
        return;
    }
    for (int i = 0; i <= steps; i++) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        int x = x0 + static_cast<int>((x1 - x0) * t + 0.5f);
        int y = y0 + static_cast<int>((y1 - y0) * t + 0.5f);
        plotPx(s, x, y, z0 + (z1 - z0) * t, color);
    }
}

// A flat-shaded cube at `center`, spun by its own angles.
static void drawCubeAt(Screen& s, Vec3 center, float scale, float objYaw, float objPitch,
                       const Camera& cam, uint32_t baseColor) {
    Vec3 v[8];
    int sx[8], sy[8];
    float sz[8];

    float ocy = std::cos(objYaw), osy = std::sin(objYaw);
    float ocp = std::cos(objPitch), osp = std::sin(objPitch);

    for (int i = 0; i < 8; i++) {
        Vec3 p = kCubeVerts[i];
        p.x *= scale;
        p.y *= scale;
        p.z *= scale;

        Vec3 a = v3(p.x * ocy + p.z * osy, p.y, -p.x * osy + p.z * ocy);
        Vec3 b = v3(a.x, a.y * ocp - a.z * osp, a.y * osp + a.z * ocp);
        b.x += center.x;
        b.y += center.y;
        b.z += center.z;

        v[i] = viewV(b, cam);
        projectV(v[i], cam.focal, s.w, s.h, sx[i], sy[i], sz[i]);
    }

    for (int t = 0; t < 12; t++) {
        int i0 = kCubeTris[t][0], i1 = kCubeTris[t][1], i2 = kCubeTris[t][2];

        Vec3 n = normalizeV(crossV(subV(v[i1], v[i0]), subV(v[i2], v[i0])));
        float lit = dotV(n, kLight);
        if (lit < 0.0f) lit = 0.0f;
        float shade = 0.18f + 0.82f * lit;

        int r = static_cast<int>(((baseColor >> 16) & 0xFF) * shade);
        int g = static_cast<int>(((baseColor >> 8) & 0xFF) * shade);
        int b = static_cast<int>((baseColor & 0xFF) * shade);

        s.fillTriangle(sx[i0], sy[i0], sz[i0], sx[i1], sy[i1], sz[i1], sx[i2], sy[i2], sz[i2],
                       rgb(r, g, b));
    }
}

// Kept so nothing that calls it breaks.
static void drawCube(Screen& screen, float yaw, float pitch, uint32_t baseColor) {
    Camera cam;
    drawCubeAt(screen, v3(0, 0, 0), 1.0f, yaw, pitch, cam, baseColor);
}

// A ground plane made of lines, so you can see the camera move.
static void drawGrid(Screen& s, const Camera& cam, float y) {
    const int N = 5;

    for (int i = -N; i <= N; i++) {
        uint32_t col = (i == 0) ? rgb(70, 110, 70) : rgb(40, 62, 40);

        int x0, y0, x1, y1;
        float z0, z1;

        Vec3 a = viewV(v3(-N, y, static_cast<float>(i)), cam);
        Vec3 b = viewV(v3(N, y, static_cast<float>(i)), cam);
        projectV(a, cam.focal, s.w, s.h, x0, y0, z0);
        projectV(b, cam.focal, s.w, s.h, x1, y1, z1);
        drawLine3(s, x0, y0, z0, x1, y1, z1, col);

        Vec3 c = viewV(v3(static_cast<float>(i), y, -N), cam);
        Vec3 d = viewV(v3(static_cast<float>(i), y, N), cam);
        projectV(c, cam.focal, s.w, s.h, x0, y0, z0);
        projectV(d, cam.focal, s.w, s.h, x1, y1, z1);
        drawLine3(s, x0, y0, z0, x1, y1, z1, col);
    }
}

// The whole test scene. `time` drives the independent object spins.
static void drawTestScene(Screen& s, const Camera& cam, float time) {
    drawGrid(s, cam, -1.25f);

    drawCubeAt(s, v3(0.0f, 0.0f, 0.0f), 0.9f, time * 0.7f, time * 0.35f, cam,
               rgb(90, 220, 130));

    float orb = time * 0.9f;
    drawCubeAt(s, v3(std::cos(orb) * 2.1f, 0.35f, std::sin(orb) * 2.1f), 0.35f, orb * 2.0f,
               orb * 1.2f, cam, rgb(230, 160, 60));
}
