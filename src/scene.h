#pragma once
#include <cmath>

#include "display.h"

// A tiny 3D scene: one cube, flat-shaded, drawn with the depth buffer.
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

// The 8 corners of a cube, 1 unit out from the centre.
static const Vec3 kCubeVerts[8] = {
    v3(-1, -1, -1), v3(1, -1, -1), v3(1, 1, -1), v3(-1, 1, -1),
    v3(-1, -1,  1), v3(1, -1,  1), v3(1, 1,  1), v3(-1, 1,  1),
};

// 12 triangles: 2 per face. Wound so each normal points outward.
static const int kCubeTris[12][3] = {
    {0, 2, 1}, {0, 3, 2},   // back
    {4, 5, 6}, {4, 6, 7},   // front
    {0, 1, 5}, {0, 5, 4},   // bottom
    {3, 7, 6}, {3, 6, 2},   // top
    {0, 4, 7}, {0, 7, 3},   // left
    {1, 2, 6}, {1, 6, 5},   // right
};

// Light coming from the upper left, slightly in front of the cube.
static const Vec3 kLight = v3(-0.40f, 0.75f, -0.53f);

// Spin a point, then push it away from the eye.
static inline Vec3 transformV(Vec3 p, float yaw, float pitch, float dist) {
    float cy = std::cos(yaw), sy = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);

    Vec3 a = v3(p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy);  // yaw about Y
    Vec3 b = v3(a.x, a.y * cp - a.z * sp, a.y * sp + a.z * cp);   // pitch about X
    return v3(b.x, b.y, b.z + dist);                             // move away
}

// 3D -> screen pixels. depth: 0 = near the eye, 1 = far away.
static inline void projectV(Vec3 p, float focal, int& sx, int& sy, float& depth) {
    float z = p.z;
    if (z < 0.1f) z = 0.1f;
    float f = focal * static_cast<float>(TOP_H) * 0.5f;
    sx = TOP_W / 2 + static_cast<int>(p.x * f / z);
    sy = TOP_H / 2 - static_cast<int>(p.y * f / z);  // screen y grows downward
    const float nearZ = 0.5f, farZ = 20.0f;
    depth = (z - nearZ) / (farZ - nearZ);
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
}

// Draw the cube into a Screen.
static void drawCube(Screen& screen, float yaw, float pitch, uint32_t baseColor) {
    const float dist = 5.0f;
    const float focal = 3.2f;

    Vec3 eye[8];
    int sx[8], sy[8];
    float sz[8];

    for (int i = 0; i < 8; i++) {
        eye[i] = transformV(kCubeVerts[i], yaw, pitch, dist);
        projectV(eye[i], focal, sx[i], sy[i], sz[i]);
    }

    for (int t = 0; t < 12; t++) {
        int i0 = kCubeTris[t][0], i1 = kCubeTris[t][1], i2 = kCubeTris[t][2];

        // Face normal from two edges. Uses the rotated points so the
        // brightness changes as the cube turns toward the light.
        Vec3 n = normalizeV(crossV(subV(eye[i1], eye[i0]), subV(eye[i2], eye[i0])));

        float lit = dotV(n, kLight);
        if (lit < 0.0f) lit = 0.0f;
        float shade = 0.18f + 0.82f * lit;  // never fully black

        int r = static_cast<int>(((baseColor >> 16) & 0xFF) * shade);
        int g = static_cast<int>(((baseColor >> 8) & 0xFF) * shade);
        int b = static_cast<int>((baseColor & 0xFF) * shade);

        screen.fillTriangle(sx[i0], sy[i0], sz[i0],
                            sx[i1], sy[i1], sz[i1],
                            sx[i2], sy[i2], sz[i2],
                            rgb(r, g, b));
    }
}
