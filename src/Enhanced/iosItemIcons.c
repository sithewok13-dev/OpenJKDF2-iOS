// Added: entire file. See iosItemIcons.h.
//
// The upscale is a port of Hyllian's xBR-lv2 shader (libretro glsl-shaders,
// xbr/shaders/xbr-lv2.glsl: corner mode C, smooth tips), under its own MIT
// licence:
//
//   Hyllian's xBR-lv2 Shader
//
//   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//
//   The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE SOFTWARE.
//
//   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
//
// Here it runs once over a whole 24 x 24 picture on the CPU, s output pixels
// for each input pixel; transparent pixels count as a colour of their own for
// its edge decisions, and colours are blended premultiplied, so the item's
// edges against the transparency stay clean. The rest -- the clean-up (an
// edge-keeping bilateral filter, a light unsharp mask), the masks of the parts
// that light up, the lighting, the dark edge -- is this port's own, made to
// match the mock-ups the owner picked (round 3, "C").

#include "Enhanced/iosItemIcons.h"

#ifdef JK_ENHANCED

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef IOSITEMICONS_STANDALONE
#include "stdPlatform.h"
#include "jk.h"
#endif

#define II_N 24

// ------------------------------------------------------------ the originals

// A 24 x 24 16-bit .bm (as the game reads it: "BM  ", the colour key at 0x1C,
// the format at 0x20, the first picture's size at 0x80, its pixels after) as
// RGBA, the colour key transparent; 0 if it isn't one
static int ii_DecodeBm(const unsigned char* d, int len, unsigned char* pOut)
{
#define II_U32(o) ((unsigned int)d[(o)] | ((unsigned int)d[(o) + 1] << 8) | ((unsigned int)d[(o) + 2] << 16) | ((unsigned int)d[(o) + 3] << 24))
    if (!d || len < 0x88 + II_N * II_N * 2 || memcmp(d, "BM  ", 4)) return 0;
    const unsigned int key = II_U32(0x1C), is16 = II_U32(0x20), bpp = II_U32(0x24);
    const unsigned int rb = II_U32(0x28), gb = II_U32(0x2C), bb = II_U32(0x30), rs = II_U32(0x34), gs = II_U32(0x38), bs = II_U32(0x3C);
    if (is16 != 1 || bpp != 16 || II_U32(0x80) != II_N || II_U32(0x84) != II_N || !rb || !gb || !bb || rb > 8 || gb > 8 || bb > 8)
        return 0;
    // (each channel's bits inside the 16)
    if (rs > 15 || gs > 15 || bs > 15 || rs + rb > 16 || gs + gb > 16 || bs + bb > 16)
        return 0;
    for (int i = 0; i < II_N * II_N; i++) {
        const unsigned int v = d[0x88 + i * 2] | (d[0x89 + i * 2] << 8);
        const unsigned int mr = (1u << rb) - 1, mg = (1u << gb) - 1, mb = (1u << bb) - 1;
        pOut[i * 4 + 0] = (unsigned char)(((v >> rs) & mr) * 255 / mr);
        pOut[i * 4 + 1] = (unsigned char)(((v >> gs) & mg) * 255 / mg);
        pOut[i * 4 + 2] = (unsigned char)(((v >> bs) & mb) * 255 / mb);
        pOut[i * 4 + 3] = v == (key & 0xFFFF) ? 0 : 255;
    }
    return 1;
#undef II_U32
}

static int ii_Is(const unsigned char* p, int r, int g, int b)
{
    return p[0] == r && p[1] == g && p[2] == b;
}

// The originals' darkest purples (the tile, the goggles' lens openings)
static int ii_Dark(const unsigned char* p)
{
    return ii_Is(p, 0x31, 0x00, 0x5a) || ii_Is(p, 0x4a, 0x04, 0x6a) || ii_Is(p, 0x6a, 0x14, 0x83);
}

// A flood fill (4 ways) from a seed, inside a box, over the pixels ok takes
typedef int (*ii_OkFn)(const unsigned char* px);
static void ii_Flood(const unsigned char* rgba, int sx, int sy, int x0, int y0, int x1, int y1, ii_OkFn ok, unsigned char* m)
{
    int aQ[II_N * II_N * 4], n = 0;
    aQ[n++] = sy * II_N + sx;
    while (n > 0) {
        const int k = aQ[--n], x = k % II_N, y = k / II_N;
        if (x < x0 || x > x1 || y < y0 || y > y1 || m[k] || !ok(rgba + k * 4)) continue;
        m[k] = 1;
        if (n + 4 <= (int)(sizeof(aQ) / sizeof(aQ[0]))) {
            if (x + 1 < II_N) aQ[n++] = k + 1;
            if (x > 0) aQ[n++] = k - 1;
            if (y + 1 < II_N) aQ[n++] = k + II_N;
            if (y > 0) aQ[n++] = k - II_N;
        }
    }
}

// The holes in a mask filled (what the outside can't reach, 4 ways)
static void ii_FillHoles(unsigned char* m)
{
    unsigned char out[(II_N + 2) * (II_N + 2)];
    int aQ[(II_N + 2) * (II_N + 2) * 4], n = 0;
    const int W = II_N + 2;
    memset(out, 0, sizeof(out));
    aQ[n++] = 0;
    while (n > 0) {
        const int k = aQ[--n], x = k % W, y = k / W;
        if (out[k]) continue;
        if (x >= 1 && x <= II_N && y >= 1 && y <= II_N && m[(y - 1) * II_N + x - 1]) continue;
        out[k] = 1;
        if (x + 1 < W) aQ[n++] = k + 1;
        if (x > 0) aQ[n++] = k - 1;
        if (y + 1 < W) aQ[n++] = k + W;
        if (y > 0) aQ[n++] = k - W;
    }
    for (int y = 0; y < II_N; y++)
        for (int x = 0; x < II_N; x++)
            if (!out[(y + 1) * W + x + 1]) m[y * II_N + x] = 1;
}

static int ii_DarkTile(const unsigned char* p) { return ii_Is(p, 0x31, 0x00, 0x5a); }

// The item alone: its tile made transparent (0 LIGHT, 1 IR, 2 BACTA)
static void ii_ItemAlone(int item, unsigned char* a)
{
    if (item == IOSITEMICONS_IR) {
        // the tile, from the edge and three seeds between the straps, over its own purple
        unsigned char bg[II_N * II_N];
        int aQ[II_N * II_N * 4], n = 0;
        memset(bg, 0, sizeof(bg));
        for (int i = 0; i < II_N; i++) {
            aQ[n++] = i;
            aQ[n++] = (II_N - 1) * II_N + i;
            aQ[n++] = i * II_N;
            aQ[n++] = i * II_N + II_N - 1;
        }
        aQ[n++] = 6 * II_N + 10;
        aQ[n++] = 7 * II_N + 8;
        aQ[n++] = 5 * II_N + 14;
        while (n > 0) {
            const int k = aQ[--n], x = k % II_N, y = k / II_N;
            if (bg[k]) continue;
            if (!(x == 0 || x == II_N - 1 || y == 0 || y == II_N - 1 || ii_DarkTile(a + k * 4))) continue;
            bg[k] = 1;
            if (n + 4 > (int)(sizeof(aQ) / sizeof(aQ[0]))) continue;
            if (x + 1 < II_N) aQ[n++] = k + 1;
            if (x > 0) aQ[n++] = k - 1;
            if (y + 1 < II_N) aQ[n++] = k + II_N;
            if (y > 0) aQ[n++] = k - II_N;
        }
        for (int k = 0; k < II_N * II_N; k++)
            if (bg[k]) a[k * 4 + 3] = 0;
    }
    else if (item == IOSITEMICONS_BACTA) {
        for (int y = 0; y < II_N; y++)
            for (int x = 0; x < II_N; x++) {
                unsigned char* p = a + (y * II_N + x) * 4;
                const int mx = p[0] > p[1] ? (p[0] > p[2] ? p[0] : p[2]) : (p[1] > p[2] ? p[1] : p[2]);
                if (x < 4 || mx < 12 || y == 0 || y == II_N - 1 || x == II_N - 1) p[3] = 0;
            }
    }
    else {
        for (int y = 0; y < II_N; y++)
            for (int x = 0; x < II_N; x++)
                if (x == 0 || y == 0 || x == II_N - 1 || y == II_N - 1) a[(y * II_N + x) * 4 + 3] = 0;
    }
}

// Whether the three originals are the game's own as this was made for them
// (a mod's own icons, another layout: no, and the text buttons stay): the
// parts that light up found where they should be, each about its size, and
// each item about as big once its tile is cut away
static int ii_Count(const unsigned char* m)
{
    int n = 0;
    for (int i = 0; i < II_N * II_N; i++) n += m[i] != 0;
    return n;
}

static void ii_ItemAlone(int item, unsigned char* a); // (below)
static int ii_LooksRight(unsigned char aOrig[3][II_N * II_N * 4])
{
    unsigned char near_[II_N * II_N], far_[II_N * II_N], core[II_N * II_N], a[II_N * II_N * 4];
    memset(near_, 0, sizeof(near_));
    memset(far_, 0, sizeof(far_));
    ii_Flood(aOrig[1], 11, 16, 5, 12, 14, 19, ii_Dark, near_);
    ii_Flood(aOrig[1], 20, 9, 18, 6, 21, 12, ii_Dark, far_);
    for (int i = 0; i < II_N * II_N; i++) core[i] = aOrig[0][i * 4] + aOrig[0][i * 4 + 1] + aOrig[0][i * 4 + 2] >= 0xff + 0xa5 + 0xff - 4;
    // (the originals': the lenses 44 and 23 pixels, the light's core 26; the items 484, 297, 250 pixels)
    const int nNear = ii_Count(near_), nFar = ii_Count(far_), nCore = ii_Count(core);
    if (nNear < 22 || nNear > 80 || nFar < 10 || nFar > 45 || nCore < 8 || nCore > 60) return 0;
    static const int aMin[3] = { 330, 180, 150 }, aMax[3] = { 560, 420, 380 };
    for (int k = 0; k < 3; k++) {
        memcpy(a, aOrig[k], sizeof(a));
        ii_ItemAlone(k, a);
        int n = 0;
        for (int i = 0; i < II_N * II_N; i++) n += a[i * 4 + 3] != 0;
        if (n < aMin[k] || n > aMax[k]) return 0;
    }
    return 1;
}

// ------------------------------------------------------------ xBR-lv2 (see the top of the file)

static float ii_Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// src: 24 x 24 RGBA; out: (24 s)^2 RGBA (straight). xs: the shader's
// XBR_SCALE (how sharp its blends are: s, or less for softer ones)
static void ii_Xbr(const unsigned char* src, int s, float xs, unsigned char* out)
{
    static const float aW[3] = { 14.352f, 28.176f, 5.472f };
    const int W = II_N, N = II_N * s;
    float lum[II_N * II_N], pm[II_N * II_N * 4];
    for (int i = 0; i < W * W; i++) {
        const float a = src[i * 4 + 3] / 255.0f;
        float c[3];
        for (int k = 0; k < 3; k++) c[k] = src[i * 4 + k] / 255.0f;
        lum[i] = a > 0.5f ? c[0] * aW[0] + c[1] * aW[1] + c[2] * aW[2] : -200.0f; // (transparent: unlike any colour)
        for (int k = 0; k < 3; k++) pm[i * 4 + k] = c[k] * a;
        pm[i * 4 + 3] = a;
    }
#define II_CL(v) ((v) < 0 ? 0 : ((v) >= W ? W - 1 : (v)))
#define L(dx, dy) lum[II_CL(y + (dy)) * W + II_CL(x + (dx))]
#define PM(dx, dy) (pm + (II_CL(y + (dy)) * W + II_CL(x + (dx))) * 4)
    static const float Ao[4] = { 1, -1, -1, 1 }, Bo[4] = { 1, 1, -1, -1 }, Co[4] = { 1.5f, 0.5f, -0.5f, 0.5f };
    static const float Bx[4] = { 0.5f, 2.0f, -0.5f, -2.0f }, Cx[4] = { 1.0f, 1.0f, -0.5f, 0.0f };
    static const float By[4] = { 2.0f, 0.5f, -2.0f, -0.5f }, Cy[4] = { 2.0f, 0.0f, -1.0f, 0.5f };
    const float Ci = 0.25f;
    const float delta = 1.0f / xs, dl[4] = { 0.5f / xs, 1.0f / xs, 0.5f / xs, 1.0f / xs }, du[4] = { 1.0f / xs, 0.5f / xs, 1.0f / xs, 0.5f / xs };
    for (int y = 0; y < W; y++) {
        for (int x = 0; x < W; x++) {
            const float A1 = L(-1, -2), B1 = L(0, -2), C1 = L(1, -2);
            const float A = L(-1, -1), B = L(0, -1), C = L(1, -1);
            const float D = L(-1, 0), E = L(0, 0), F = L(1, 0);
            const float G = L(-1, 1), H = L(0, 1), I = L(1, 1);
            const float G5 = L(-1, 2), H5 = L(0, 2), I5 = L(1, 2);
            const float A0 = L(-2, -1), D0 = L(-2, 0), G0 = L(-2, 1);
            const float C4 = L(2, -1), F4 = L(2, 0), I4 = L(2, 1);
            const float b[4] = { B, D, H, F }, c[4] = { C, A, G, I }, d[4] = { D, H, F, B }, f[4] = { F, B, D, H };
            const float g[4] = { G, I, C, A }, h[4] = { H, F, B, D }, i_[4] = { I, C, A, G };
            const float i4[4] = { I4, C1, A0, G5 }, i5[4] = { I5, C4, A1, G0 }, h5[4] = { H5, F4, B1, D0 }, f4[4] = { F4, B1, D0, H5 };
            float edri[4], edr[4], edrl[4], edru[4], px[4];
#define DF(p, q) fabsf((p) - (q))
#define DIFF(p, q) ((p) != (q) ? 1.0f : 0.0f)
#define EQ(p, q) (DF(p, q) <= 15.0f ? 1.0f : 0.0f)
#define NEQ(p, q) (1.0f - EQ(p, q))
            for (int k = 0; k < 4; k++) {
                const float e = E;
                const float irlv0 = DIFF(e, f[k]) * DIFF(e, h[k]);
                const float irlv1 = irlv0 * (NEQ(f[k], b[k]) * NEQ(f[k], c[k]) + NEQ(h[k], d[k]) * NEQ(h[k], g[k])
                                             + EQ(e, i_[k]) * (NEQ(f[k], f4[k]) * NEQ(f[k], i4[k]) + NEQ(h[k], h5[k]) * NEQ(h[k], i5[k]))
                                             + EQ(e, g[k]) + EQ(e, c[k]));
                const float irlv2l = DIFF(e, g[k]) * DIFF(d[k], g[k]), irlv2u = DIFF(e, c[k]) * DIFF(b[k], c[k]);
                const float wd1 = DF(e, c[k]) + DF(e, g[k]) + DF(i_[k], h5[k]) + DF(i_[k], f4[k]) + 4.0f * DF(h[k], f[k]);
                const float wd2 = DF(h[k], d[k]) + DF(h[k], i5[k]) + DF(f[k], i4[k]) + DF(f[k], b[k]) + 4.0f * DF(e, i_[k]);
                edri[k] = (wd2 >= wd1 ? 1.0f : 0.0f) * irlv0;
                edr[k] = (wd2 >= wd1 + 0.1f ? 1.0f : 0.0f) * (irlv1 >= 0.5f ? 1.0f : 0.0f);
                edrl[k] = (DF(h[k], c[k]) >= 2.0f * DF(f[k], g[k]) ? 1.0f : 0.0f) * irlv2l * edr[k];
                edru[k] = (DF(f[k], g[k]) >= 2.0f * DF(h[k], c[k]) ? 1.0f : 0.0f) * irlv2u * edr[k];
                px[k] = DF(e, h[k]) >= DF(e, f[k]) ? 1.0f : 0.0f;
            }
            const float *cE = PM(0, 0), *cB = PM(0, -1), *cD = PM(-1, 0), *cH = PM(0, 1), *cF = PM(1, 0);
            for (int sy = 0; sy < s; sy++) {
                for (int sx = 0; sx < s; sx++) {
                    const float fpx = (sx + 0.5f) / s, fpy = (sy + 0.5f) / s;
                    float mx[4];
                    for (int k = 0; k < 4; k++) {
                        const float fx = Ao[k] * fpy + Bo[k] * fpx, fxl = Ao[k] * fpy + Bx[k] * fpx, fxu = Ao[k] * fpy + By[k] * fpx;
                        const float f45i = ii_Clamp01((fx + delta - Co[k] - Ci) / (2.0f * delta)) * edri[k];
                        const float f45 = ii_Clamp01((fx + delta - Co[k]) / (2.0f * delta)) * edr[k];
                        const float f30 = ii_Clamp01((fxl + dl[k] - Cx[k]) / (2.0f * dl[k])) * edrl[k];
                        const float f60 = ii_Clamp01((fxu + du[k] - Cy[k]) / (2.0f * du[k])) * edru[k];
                        mx[k] = fmaxf(fmaxf(f30, f60), fmaxf(f45, f45i));
                    }
                    float r1[4], r2[4], d1 = 0.0f, d2 = 0.0f;
                    for (int ch = 0; ch < 4; ch++) {
                        float t = cH[ch] + (cF[ch] - cH[ch]) * px[0];
                        r1[ch] = cE[ch] + (t - cE[ch]) * mx[0];
                        t = cB[ch] + (cD[ch] - cB[ch]) * px[2];
                        r1[ch] = r1[ch] + (t - r1[ch]) * mx[2];
                        t = cF[ch] + (cB[ch] - cF[ch]) * px[1];
                        r2[ch] = cE[ch] + (t - cE[ch]) * mx[1];
                        t = cD[ch] + (cH[ch] - cD[ch]) * px[3];
                        r2[ch] = r2[ch] + (t - r2[ch]) * mx[3];
                        d1 += fabsf(cE[ch] - r1[ch]);
                        d2 += fabsf(cE[ch] - r2[ch]);
                    }
                    const float* res = d2 >= d1 ? r2 : r1;
                    unsigned char* o = out + ((y * s + sy) * N + x * s + sx) * 4;
                    const float al = res[3];
                    for (int ch = 0; ch < 3; ch++) {
                        const float v = al > 1e-4f ? res[ch] / fmaxf(al, 1e-4f) : 0.0f;
                        o[ch] = (unsigned char)fminf(fmaxf(v * 255.0f + 0.5f, 0.0f), 255.0f);
                    }
                    o[3] = (unsigned char)fminf(fmaxf(al * 255.0f + 0.5f, 0.0f), 255.0f);
                }
            }
        }
    }
#undef DF
#undef DIFF
#undef EQ
#undef NEQ
#undef L
#undef PM
#undef II_CL
}

// ------------------------------------------------------------ filters

// (a row or column index reflected inside 0..n-1 without repeating the edge, as OpenCV's default border)
static int ii_Reflect101(int i, int n)
{
    if (n == 1) return 0;
    while (i < 0 || i >= n) i = i < 0 ? -i : 2 * n - 2 - i;
    return i;
}

static int ii_ClampI(int i, int n) { return i < 0 ? 0 : (i >= n ? n - 1 : i); }

// A Gaussian blur of an N x N image of ch channels (sigma in pixels), its
// kernel and borders as OpenCV's for float images
static void ii_Gauss(float* img, int N, int ch, float sigma)
{
    if (sigma <= 0.0f) return;
    int k = ((int)lrintf(sigma * 8.0f + 1.0f)) | 1;
    if (k < 3) k = 3;
    const int r = k / 2;
    float* w = (float*)malloc(sizeof(float) * k);
    float* tmp = (float*)malloc(sizeof(float) * N * N * ch);
    if (!w || !tmp) { free(w); free(tmp); return; }
    int* aIx = (int*)malloc(sizeof(int) * (N + 2 * r)); // (the border's reflections, looked up)
    if (!aIx) { free(w); free(tmp); return; }
    for (int i = -r; i < N + r; i++) aIx[i + r] = ii_Reflect101(i, N);
    float sum = 0.0f;
    for (int i = 0; i < k; i++) { w[i] = expf(-(float)((i - r) * (i - r)) / (2.0f * sigma * sigma)); sum += w[i]; }
    for (int i = 0; i < k; i++) w[i] /= sum;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            for (int c = 0; c < ch; c++) {
                float a = 0.0f;
                for (int i = 0; i < k; i++) a += w[i] * img[(y * N + aIx[x + i]) * ch + c];
                tmp[(y * N + x) * ch + c] = a;
            }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            for (int c = 0; c < ch; c++) {
                float a = 0.0f;
                for (int i = 0; i < k; i++) a += w[i] * tmp[(aIx[y + i] * N + x) * ch + c];
                img[(y * N + x) * ch + c] = a;
            }
    free(aIx);
    free(w);
    free(tmp);
}

// Pillow's Gaussian blur (radius = sigma), as it does it: three box blurs of
// a fractional width, the edges extended
static void ii_BoxLine(const float* in, float* out, int n, int stride, float fr)
{
    const int r = (int)fr;
    const float frac = fr - r, ww = 1.0f / (fr * 2.0f + 1.0f);
    for (int i = 0; i < n; i++) {
        float a = 0.0f;
        for (int j = -r; j <= r; j++) a += in[ii_ClampI(i + j, n) * stride];
        a += frac * (in[ii_ClampI(i - r - 1, n) * stride] + in[ii_ClampI(i + r + 1, n) * stride]);
        out[i * stride] = a * ww;
    }
}

static void ii_GaussPil(float* img, int N, int ch, float radius)
{
    if (radius <= 0.0f) return;
    const float sigma2 = radius * radius / 3.0f, Lb = sqrtf(12.0f * sigma2 + 1.0f), l = floorf((Lb - 1.0f) / 2.0f);
    float a = (2.0f * l + 1.0f) * (l * (l + 1.0f) - 3.0f * sigma2);
    a /= 6.0f * (sigma2 - (l + 1.0f) * (l + 1.0f));
    const float fr = l + a;
    float* line = (float*)malloc(sizeof(float) * N * 2);
    if (!line) return;
    for (int pass = 0; pass < 3; pass++) {
        for (int c = 0; c < ch; c++) {
            for (int y = 0; y < N; y++) {
                for (int x = 0; x < N; x++) line[x] = img[(y * N + x) * ch + c];
                ii_BoxLine(line, line + N, N, 1, fr);
                for (int x = 0; x < N; x++) img[(y * N + x) * ch + c] = line[N + x];
            }
            for (int x = 0; x < N; x++) {
                for (int y = 0; y < N; y++) line[y] = img[(y * N + x) * ch + c];
                ii_BoxLine(line, line + N, N, 1, fr);
                for (int y = 0; y < N; y++) img[(y * N + x) * ch + c] = line[N + y];
            }
        }
    }
    free(line);
}

// One pass of an edge-keeping (bilateral) filter over premultiplied RGB, as
// OpenCV's: a disc of radius d/2, the colour's distance the sum of the three
// channels' differences
static void ii_Bilateral(float* rgb, int N, int d, float sc, float ss)
{
    const int r = d / 2 > 1 ? d / 2 : 1;
    const float cc = -0.5f / (sc * sc), sc2 = -0.5f / (ss * ss);
    // (the colour weights from a table, a quarter step apart, as OpenCV does: no exp for each pair)
#define II_LUT_N 3080
    static float aLut[II_LUT_N + 2];
    static float lutCc = 0.0f;
    if (lutCc != cc) {
        for (int i = 0; i < II_LUT_N + 2; i++) aLut[i] = expf((i * 0.25f) * (i * 0.25f) * cc);
        lutCc = cc;
    }
    float* tmp = (float*)malloc(sizeof(float) * N * N * 3);
    int* aOff = (int*)malloc(sizeof(int) * (2 * r + 1) * (2 * r + 1) * 2);
    float* aW = (float*)malloc(sizeof(float) * (2 * r + 1) * (2 * r + 1));
    if (!tmp || !aOff || !aW) { free(tmp); free(aOff); free(aW); return; }
    int n = 0;
    for (int i = -r; i <= r; i++)
        for (int j = -r; j <= r; j++) {
            const float rr = sqrtf((float)(i * i + j * j));
            if (rr > r) continue;
            aOff[n * 2] = j;
            aOff[n * 2 + 1] = i;
            aW[n++] = expf(rr * rr * sc2);
        }
    memcpy(tmp, rgb, sizeof(float) * N * N * 3);
    int* aIx = (int*)malloc(sizeof(int) * (N + 2 * r)); // (the border's reflections, looked up)
    if (!aIx) { free(tmp); free(aOff); free(aW); return; }
    for (int i = -r; i < N + r; i++) aIx[i + r] = ii_Reflect101(i, N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float* p0 = tmp + (y * N + x) * 3;
            float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, ws = 0.0f;
            for (int k = 0; k < n; k++) {
                const float* p = tmp + (aIx[y + aOff[k * 2 + 1] + r] * N + aIx[x + aOff[k * 2] + r]) * 3;
                const float dd = (fabsf(p[0] - p0[0]) + fabsf(p[1] - p0[1]) + fabsf(p[2] - p0[2])) * 4.0f;
                int li = (int)dd;
                if (li > II_LUT_N) li = II_LUT_N;
                const float w = aW[k] * (aLut[li] + (dd - li) * (aLut[li + 1] - aLut[li]));
                s0 += p[0] * w;
                s1 += p[1] * w;
                s2 += p[2] * w;
                ws += w;
            }
            float* o = rgb + (y * N + x) * 3;
            o[0] = s0 / ws;
            o[1] = s1 / ws;
            o[2] = s2 / ws;
        }
    free(aIx);
    free(tmp);
    free(aOff);
    free(aW);
}

static unsigned char ii_U8(float v)
{
    return (unsigned char)(v < 0.0f ? 0 : (v > 255.0f ? 255 : (int)(v + 0.5f)));
}

// The picture made big: xBR-lv2 (a little softer than its scale), three
// passes of the edge-keeping filter, a light sharpen (Pillow's unsharp mask:
// on the colour only, 80%, from a difference of 2)
static void ii_Upscale(const unsigned char* src, int s, unsigned char* out)
{
    const int N = II_N * s;
    ii_Xbr(src, s, s * 0.67f, out);
    float* rgb = (float*)malloc(sizeof(float) * N * N * 3);
    if (!rgb) return;
    for (int i = 0; i < N * N; i++)
        for (int c = 0; c < 3; c++) rgb[i * 3 + c] = out[i * 4 + c] * (out[i * 4 + 3] / 255.0f);
    for (int it = 0; it < 3; it++) ii_Bilateral(rgb, N, ((int)(s * 1.5f)) | 1, 40.0f, s * 0.66f);
    for (int i = 0; i < N * N; i++) {
        const float al = out[i * 4 + 3] / 255.0f;
        for (int c = 0; c < 3; c++) out[i * 4 + c] = al > 1e-3f ? ii_U8(rgb[i * 3 + c] / al) : 0;
    }
    for (int i = 0; i < N * N * 3; i++) rgb[i] = out[(i / 3) * 4 + i % 3];
    ii_GaussPil(rgb, N, 3, s * 0.23f);
    for (int i = 0; i < N * N; i++)
        for (int c = 0; c < 3; c++) {
            const int v = out[i * 4 + c], diff = v - (int)ii_U8(rgb[i * 3 + c]);
            if (abs(diff) >= 2) {
                const int o = v + diff * 80 / 100;
                out[i * 4 + c] = (unsigned char)(o < 0 ? 0 : (o > 255 ? 255 : o));
            }
        }
    free(rgb);
}

// A mask of the originals' pixels made big the same way, softened a little (0..1)
static void ii_UpscaleMask(const unsigned char* m, int s, float* out)
{
    const int N = II_N * s;
    unsigned char src[II_N * II_N * 4];
    unsigned char* big = (unsigned char*)malloc(N * N * 4);
    if (!big) { memset(out, 0, sizeof(float) * N * N); return; }
    for (int i = 0; i < II_N * II_N; i++) memset(src + i * 4, m[i] ? 255 : 0, 4);
    ii_Xbr(src, s, (float)s, big);
    for (int i = 0; i < N * N; i++) out[i] = big[i * 4 + 3] / 255.0f;
    free(big);
    ii_Gauss(out, N, 1, s * 0.08f);
}

// The distance of each pixel of a mask to the nearest one outside it (two
// passes, the 5 x 5 chamfer OpenCV's L2 distance uses)
static void ii_Distance(const float* u, int N, float* dist)
{
    static const float a = 1.0f, b = 1.4f, c = 2.1969f;
    const float big = 1e9f;
    for (int i = 0; i < N * N; i++) dist[i] = u[i] > 0.5f ? big : 0.0f;
#define DD(x, y) ((x) < 0 || (y) < 0 || (x) >= N || (y) >= N ? big : dist[(y) * N + (x)])
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float v = dist[y * N + x];
            if (v == 0.0f) continue;
            v = fminf(v, DD(x - 1, y) + a); v = fminf(v, DD(x, y - 1) + a);
            v = fminf(v, DD(x - 1, y - 1) + b); v = fminf(v, DD(x + 1, y - 1) + b);
            v = fminf(v, DD(x - 2, y - 1) + c); v = fminf(v, DD(x + 2, y - 1) + c);
            v = fminf(v, DD(x - 1, y - 2) + c); v = fminf(v, DD(x + 1, y - 2) + c);
            dist[y * N + x] = v;
        }
    for (int y = N - 1; y >= 0; y--)
        for (int x = N - 1; x >= 0; x--) {
            float v = dist[y * N + x];
            if (v == 0.0f) continue;
            v = fminf(v, DD(x + 1, y) + a); v = fminf(v, DD(x, y + 1) + a);
            v = fminf(v, DD(x + 1, y + 1) + b); v = fminf(v, DD(x - 1, y + 1) + b);
            v = fminf(v, DD(x + 2, y + 1) + c); v = fminf(v, DD(x - 2, y + 1) + c);
            v = fminf(v, DD(x + 1, y + 2) + c); v = fminf(v, DD(x - 1, y + 2) + c);
            dist[y * N + x] = v;
        }
#undef DD
}

// A colour along stops (t 0..1)
typedef struct { float t, c[3]; } ii_Stop;
static void ii_Lerp(const ii_Stop* a, int n, float t, float* out)
{
    t = ii_Clamp01(t);
    out[0] = out[1] = out[2] = 0.0f;
    for (int i = 0; i + 1 < n; i++) {
        if (t < a[i].t || t > a[i + 1].t) continue;
        const float k = ii_Clamp01((t - a[i].t) / fmaxf(a[i + 1].t - a[i].t, 1e-6f));
        for (int c = 0; c < 3; c++) out[c] = a[i].c[c] + (a[i + 1].c[c] - a[i].c[c]) * k;
    }
}

static float ii_Screen(float base, float light)
{
    light = light < 0.0f ? 0.0f : (light > 255.0f ? 255.0f : light);
    return 255.0f - (255.0f - base) * (255.0f - light) / 255.0f;
}

// ------------------------------------------------------------ the pictures

static int ii_s = 0, ii_bReady = 0, ii_out = 0; // (ii_out: the pictures' pixels across, at most 24 s)
static unsigned char* ii_apStill[IOSITEMICON_NUM_STILL];
// BACTA's parts, for drawing it again as its liquid or glow changes: its
// colour (straight), its alpha, the glass's mask, and the dark edge under it
static float *ii_pBactaRgb = NULL, *ii_pBactaAl = NULL, *ii_pGlass = NULL;
static unsigned char* ii_pBactaEdge = NULL;

// The dark edge (premultiplied RGBA under the item): its alpha grown px
// pixels, softened, at 0.6, the tile's darkest purple
static void ii_EdgeUnder(const float* al, int N, int px, unsigned char* pOut)
{
    float* m = (float*)malloc(sizeof(float) * N * N);
    if (!m) { memset(pOut, 0, N * N * 4); return; }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float v = 0.0f;
            for (int j = -px; j <= px; j++)
                for (int i = -px; i <= px; i++) v = fmaxf(v, al[ii_ClampI(y + j, N) * N + ii_ClampI(x + i, N)]);
            m[y * N + x] = (float)(int)(v + 0.5f);
        }
    ii_GaussPil(m, N, 1, px * 0.35f);
    for (int i = 0; i < N * N; i++) {
        const int a = (int)((int)(m[i] + 0.5f) * 0.6f);
        pOut[i * 4 + 0] = (unsigned char)(20 * a / 255);
        pOut[i * 4 + 1] = 0;
        pOut[i * 4 + 2] = (unsigned char)(31 * a / 255);
        pOut[i * 4 + 3] = (unsigned char)a;
    }
    free(m);
}

// The item (straight colour 0..255, alpha 0..255) over its dark edge, premultiplied RGBA
static void ii_Finish(const float* rgb, const float* al, const unsigned char* edge, int N, unsigned char* pOut)
{
    for (int i = 0; i < N * N; i++) {
        const float a = (float)ii_U8(al[i]) / 255.0f, ea = edge[i * 4 + 3] / 255.0f, oa = a + ea * (1.0f - a);
        for (int c = 0; c < 3; c++) {
            const float v = (float)ii_U8(rgb[i * 3 + c]);
            pOut[i * 4 + c] = ii_U8(v * a + edge[i * 4 + c] * (1.0f - a));
        }
        pOut[i * 4 + 3] = ii_U8(oa * 255.0f);
    }
}

// An item's picture made big, as floats: straight colour (0..255) and alpha (0..255)
static void ii_Big(const unsigned char* orig, int item, int bOff, int s, float* rgb, float* al)
{
    const int N = II_N * s;
    unsigned char src[II_N * II_N * 4];
    memcpy(src, orig, sizeof(src));
    ii_ItemAlone(item, src);
    if (bOff) {
        // off: darker (LIGHT more so)
        static const float aLight[3] = { 0.62f, 0.5f, 0.68f }, aIr[3] = { 0.8f, 0.74f, 0.82f };
        const float* k = item == IOSITEMICONS_LIGHT ? aLight : aIr;
        for (int i = 0; i < II_N * II_N; i++)
            for (int c = 0; c < 3; c++) src[i * 4 + c] = (unsigned char)fminf(src[i * 4 + c] * k[c], 255.0f);
    }
    unsigned char* big = (unsigned char*)malloc(N * N * 4);
    if (!big) { memset(rgb, 0, sizeof(float) * N * N * 3); memset(al, 0, sizeof(float) * N * N); return; }
    ii_Upscale(src, s, big);
    for (int i = 0; i < N * N; i++) {
        for (int c = 0; c < 3; c++) rgb[i * 3 + c] = big[i * 4 + c];
        al[i] = big[i * 4 + 3];
    }
    free(big);
}

// LIGHT: off, its core unlit; on, brighter, its core white with a glow round
// it; the picture faded out round its head and kept over its lamp
static void ii_Light(const unsigned char* orig, int bOn, int s, float* rgb, float* al)
{
    const int N = II_N * s;
    unsigned char core[II_N * II_N];
    for (int i = 0; i < II_N * II_N; i++) core[i] = orig[i * 4] + orig[i * 4 + 1] + orig[i * 4 + 2] >= 0xff + 0xa5 + 0xff - 4;
    ii_Big(orig, IOSITEMICONS_LIGHT, !bOn, s, rgb, al);
    float* m = (float*)malloc(sizeof(float) * N * N * 4);
    if (!m) return;
    float *cm = m, *b1 = m + N * N, *b2 = m + N * N * 2, *lamp = m + N * N * 3;
    ii_UpscaleMask(core, s, cm);
    if (bOn) {
        memcpy(b1, cm, sizeof(float) * N * N);
        memcpy(b2, cm, sizeof(float) * N * N);
        ii_Gauss(b1, N, 1, s * 1.4f);
        ii_Gauss(b2, N, 1, s * 3.5f);
    }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) lamp[y * N + x] = (x / (float)s > 8.2f && x / (float)s < 15.8f && y / (float)s > 11.6f && y / (float)s < 22.2f) ? 1.0f : 0.0f;
    ii_Gauss(lamp, N, 1, s * 0.15f);
    const float r0 = bOn ? 8.0f : 7.0f, r1 = bOn ? 12.0f : 11.0f;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const int i = y * N + x;
            float* p = rgb + i * 3;
            if (bOn) {
                static const float aBloom[3] = { 255.0f, 225.0f, 255.0f };
                const float bl = b1[i] * 1.6f + b2[i] * 1.2f;
                for (int c = 0; c < 3; c++) {
                    float v = fminf(p[c] * 1.1f, 255.0f);
                    v = ii_Screen(v, bl * aBloom[c]);
                    p[c] = fmaxf(v, cm[i] * 255.0f);
                }
            }
            else
                for (int c = 0; c < 3; c++) p[c] *= 1.0f - 0.3f * cm[i];
            const float d = hypotf(x / (float)s - 12.0f, y / (float)s - 10.0f);
            const float fade = ii_Clamp01(1.0f - (d - r0) / (r1 - r0));
            al[i] *= fmaxf(fade, lamp[i]);
        }
    free(m);
}

// IR on: its two lens openings (the originals' own shapes) lit, red-pink,
// brighter towards their middles, with a little bloom
static void ii_IrOn(const unsigned char* orig, int s, float* rgb, float* al)
{
    static const ii_Stop aStops[4] = { { 0.0f, { 214, 20, 92 } }, { 0.35f, { 255, 70, 140 } }, { 0.75f, { 255, 170, 205 } }, { 1.0f, { 255, 236, 244 } } };
    const int N = II_N * s;
    unsigned char near_[II_N * II_N], far_[II_N * II_N];
    memset(near_, 0, sizeof(near_));
    memset(far_, 0, sizeof(far_));
    ii_Flood(orig, 11, 16, 5, 12, 14, 19, ii_Dark, near_);
    ii_Flood(orig, 20, 9, 18, 6, 21, 12, ii_Dark, far_);
    ii_FillHoles(near_);
    ii_FillHoles(far_);
    ii_Big(orig, IOSITEMICONS_IR, 0, s, rgb, al);
    float* m = (float*)malloc(sizeof(float) * N * N * 7);
    if (!m) return;
    float *u = m, *dist = m + N * N, *mm = m + N * N * 2, *lit = m + N * N * 3; // (lit: 3 channels)
    float* mmb = m + N * N * 6;
    memset(mm, 0, sizeof(float) * N * N);
    memset(lit, 0, sizeof(float) * N * N * 3);
    for (int k = 0; k < 2; k++) {
        ii_UpscaleMask(k ? far_ : near_, s, u);
        ii_Distance(u, N, dist);
        float dmax = 1.0f;
        for (int i = 0; i < N * N; i++) dmax = fmaxf(dmax, dist[i]);
        for (int i = 0; i < N * N; i++) {
            float col[3];
            ii_Lerp(aStops, 4, powf(dist[i] / dmax, 0.8f), col);
            for (int c = 0; c < 3; c++) lit[i * 3 + c] += col[c] * u[i];
            mm[i] = fmaxf(mm[i], u[i]);
        }
    }
    for (int i = 0; i < N * N; i++)
        for (int c = 0; c < 3; c++) {
            const float v = fminf(rgb[i * 3 + c] * 1.06f, 255.0f);
            rgb[i * 3 + c] = v * (1.0f - mm[i] * 0.97f) + lit[i * 3 + c] * 0.97f;
        }
    ii_Gauss(lit, N, 3, s * 0.7f);
    memcpy(mmb, mm, sizeof(float) * N * N);
    ii_Gauss(mmb, N, 1, s * 0.7f);
    for (int i = 0; i < N * N; i++) {
        for (int c = 0; c < 3; c++) rgb[i * 3 + c] = ii_Screen(rgb[i * 3 + c], lit[i * 3 + c] * 0.75f);
        al[i] = fmaxf(al[i], mmb[i] * 255.0f * 0.6f);
    }
    free(m);
}

static void ii_FreeDigits(void);
static void ii_FreeAll(void)
{
    ii_FreeDigits();
    for (int i = 0; i < IOSITEMICON_NUM_STILL; i++) { free(ii_apStill[i]); ii_apStill[i] = NULL; }
    free(ii_pBactaRgb); free(ii_pBactaAl); free(ii_pGlass); free(ii_pBactaEdge);
    ii_pBactaRgb = ii_pBactaAl = ii_pGlass = NULL;
    ii_pBactaEdge = NULL;
    ii_bReady = 0;
    ii_s = 0;
    ii_out = 0;
}

// An n x n premultiplied RGBA picture made m x m (m <= n), each pixel the
// average of what it covers (the screen's own pixels: none of the
// shimmer of a picture shrunk as it is drawn)
static void ii_Shrink(const unsigned char* src, int n, unsigned char* dst, int m)
{
    if (m == n) { memcpy(dst, src, (size_t)n * n * 4); return; }
    float* tmp = (float*)malloc(sizeof(float) * n * m * 4);
    if (!tmp) { memset(dst, 0, (size_t)m * m * 4); return; }
    const float k = (float)n / m;
    // (across, then down: each output pixel's span [o k, (o + 1) k) of the source's)
    for (int y = 0; y < n; y++)
        for (int o = 0; o < m; o++) {
            const float a0 = o * k, a1 = (o + 1) * k;
            float acc[4] = { 0, 0, 0, 0 };
            for (int x = (int)a0; x < n && x < a1; x++) {
                const float w = fminf(a1, x + 1.0f) - fmaxf(a0, (float)x);
                if (w <= 0.0f) continue;
                for (int c = 0; c < 4; c++) acc[c] += w * src[(y * n + x) * 4 + c];
            }
            for (int c = 0; c < 4; c++) tmp[(y * m + o) * 4 + c] = acc[c] / k;
        }
    for (int x = 0; x < m; x++)
        for (int o = 0; o < m; o++) {
            const float a0 = o * k, a1 = (o + 1) * k;
            float acc[4] = { 0, 0, 0, 0 };
            for (int y = (int)a0; y < n && y < a1; y++) {
                const float w = fminf(a1, y + 1.0f) - fmaxf(a0, (float)y);
                if (w <= 0.0f) continue;
                for (int c = 0; c < 4; c++) acc[c] += w * tmp[(y * m + x) * 4 + c];
            }
            for (int c = 0; c < 4; c++) dst[(o * m + x) * 4 + c] = ii_U8(acc[c] / k);
        }
    free(tmp);
}

void iosItemIcons_Free(void)
{
    ii_FreeAll();
}

int iosItemIcons_PrepareFrom(const unsigned char* const aBm[3], const int aLen[3], int s)
{
    return iosItemIcons_PrepareFromSized(aBm, aLen, s, 0);
}

int iosItemIcons_PrepareFromSized(const unsigned char* const aBm[3], const int aLen[3], int s, int outPx)
{
    unsigned char aOrig[3][II_N * II_N * 4];
    if (s < 1 || s > 16) return 0;
    if (outPx <= 0 || outPx > II_N * s) outPx = II_N * s;
    if (ii_bReady && ii_s == s && ii_out == outPx) return 1;
    ii_FreeAll();
    for (int k = 0; k < 3; k++)
        if (!aBm[k] || !ii_DecodeBm(aBm[k], aLen[k], aOrig[k])) return 0;
    if (!ii_LooksRight(aOrig)) return 0;
    const int N = II_N * s, px = (int)(s * 0.6f);
    float* rgb = (float*)malloc(sizeof(float) * N * N * 3);
    float* al = (float*)malloc(sizeof(float) * N * N);
    unsigned char* edge = (unsigned char*)malloc(N * N * 4);
    int bOk = rgb && al && edge;
    unsigned char* big = (unsigned char*)malloc(N * N * 4);
    bOk = bOk && big;
    for (int i = 0; i < IOSITEMICON_NUM_STILL && bOk; i++) {
        ii_apStill[i] = (unsigned char*)malloc(outPx * outPx * 4);
        if (!ii_apStill[i]) { bOk = 0; break; }
        if (i == IOSITEMICON_LIGHT_OFF || i == IOSITEMICON_LIGHT_ON) ii_Light(aOrig[0], i == IOSITEMICON_LIGHT_ON, s, rgb, al);
        else if (i == IOSITEMICON_IR_OFF) ii_Big(aOrig[1], IOSITEMICONS_IR, 1, s, rgb, al);
        else ii_IrOn(aOrig[1], s, rgb, al);
        ii_EdgeUnder(al, N, px, edge);
        ii_Finish(rgb, al, edge, N, big);
        ii_Shrink(big, N, ii_apStill[i], outPx);
    }
    free(big);
    if (bOk) {
        // BACTA: its parts kept, to draw it with its liquid (iosItemIcons_Bacta)
        unsigned char glass[II_N * II_N];
        ii_pBactaRgb = (float*)malloc(sizeof(float) * N * N * 3);
        ii_pBactaAl = (float*)malloc(sizeof(float) * N * N);
        ii_pGlass = (float*)malloc(sizeof(float) * N * N);
        ii_pBactaEdge = (unsigned char*)malloc(N * N * 4);
        bOk = ii_pBactaRgb && ii_pBactaAl && ii_pGlass && ii_pBactaEdge;
        if (bOk) {
            // (the glass: the tank between its two caps, rows 7-17, inside its outline, columns 5-15)
            for (int y = 0; y < II_N; y++)
                for (int x = 0; x < II_N; x++) {
                    const unsigned char* p = aOrig[2] + (y * II_N + x) * 4;
                    glass[y * II_N + x] = y >= 7 && y <= 17 && x >= 5 && x <= 15 && p[0] + p[1] + p[2] > 0;
                }
            ii_Big(aOrig[2], IOSITEMICONS_BACTA, 0, s, ii_pBactaRgb, ii_pBactaAl);
            ii_UpscaleMask(glass, s, ii_pGlass);
            ii_EdgeUnder(ii_pBactaAl, N, px, ii_pBactaEdge);
        }
    }
    free(rgb);
    free(al);
    free(edge);
    if (!bOk) { ii_FreeAll(); return 0; }
    ii_s = s;
    ii_out = outPx;
    ii_bReady = 1;
    return 1;
}

int iosItemIcons_IsReady(void)
{
    return ii_bReady;
}

int iosItemIcons_Size(void)
{
    return ii_bReady ? ii_out : 0;
}

const unsigned char* iosItemIcons_Still(int which)
{
    return (ii_bReady && which >= 0 && which < IOSITEMICON_NUM_STILL) ? ii_apStill[which] : NULL;
}

// BACTA drawn: the liquid at level, brighter below its glow's front (front:
// the share of the liquid's height from the bottom; strength: how much;
// bWhole: the whole liquid, its meniscus too)
static int ii_DrawBacta(float level, float front, float strength, int bWhole, unsigned char* pOut)
{
    static const ii_Stop aStops[3] = { { 0.0f, { 255, 200, 255 } }, { 0.45f, { 229, 125, 237 } }, { 1.0f, { 164, 64, 180 } } };
    if (!ii_bReady || !pOut) return 0;
    const int s = ii_s, N = II_N * s;
    float* rgb = (float*)malloc(sizeof(float) * N * N * 3);
    unsigned char* big = ii_out == N ? pOut : (unsigned char*)malloc(N * N * 4);
    if (!rgb || !big) {
        free(rgb);
        if (big != pOut) free(big);
        return 0;
    }
    memcpy(rgb, ii_pBactaRgb, sizeof(float) * N * N * 3);
    level = ii_Clamp01(level);
    front = ii_Clamp01(front);
    if (level > 0.0f) {
        // the liquid: its top level of the way up the glass (rows 7 to 18),
        // a meniscus, three bubbles; a glow through it from the bottom up to
        // its front (the whole liquid, and its meniscus, at 1)
        const float top = 7.0f + 11.0f * (1.0f - level), h = fmaxf(18.0f - top, 1e-3f);
        const float glowTop = bWhole ? top - 1.5f : 18.0f - front * h;
        static const float aB[3][3] = { { 8.2f, 0.62f, 0.55f }, { 10.6f, 0.32f, 0.4f }, { 7.0f, 0.3f, 0.3f } };
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const int i = y * N + x;
                const float g = ii_pGlass[i];
                if (g <= 0.0f) continue;
                const float xx = x / (float)s, yy = y / (float)s;
                const float fill = g * ii_Clamp01((yy - top) * s / 1.5f);
                float liq[3];
                ii_Lerp(aStops, 3, (yy - top) / h, liq);
                for (int k = 0; k < 3; k++) {
                    const float bm = ii_Clamp01((aB[k][2] - hypotf(xx - aB[k][0], yy - (top + h * aB[k][1]))) * s);
                    liq[0] = liq[0] * (1.0f - bm * 0.8f) + 255.0f * bm * 0.8f;
                    liq[1] = liq[1] * (1.0f - bm * 0.8f) + 230.0f * bm * 0.8f;
                    liq[2] = liq[2] * (1.0f - bm * 0.8f) + 255.0f * bm * 0.8f;
                }
                const float men = ii_Clamp01(1.0f - fabsf(yy - top - 0.15f) * s / 0.9f) * g;
                for (int c = 0; c < 3; c++) liq[c] = liq[c] * (1.0f - men) + 255.0f * men;
                if (front > 0.0f && strength > 0.0f) {
                    // (below the glow's front, brighter; the front itself soft)
                    const float gl = ii_Clamp01((yy - glowTop) * s / 1.2f) * strength;
                    liq[0] = ii_Screen(liq[0], 255.0f * gl);
                    liq[1] = ii_Screen(liq[1], 215.0f * gl);
                    liq[2] = ii_Screen(liq[2], 255.0f * gl);
                }
                float* p = rgb + i * 3;
                const float hl = ii_Clamp01(((p[0] + p[1] + p[2]) / 3.0f - 200.0f) / 40.0f) * g; // (the glass's own highlight stays on top)
                for (int c = 0; c < 3; c++) {
                    p[c] = p[c] * (1.0f - fill * 0.93f) + liq[c] * fill * 0.93f;
                    p[c] = fmaxf(p[c], hl * 255.0f);
                }
            }
    }
    ii_Finish(rgb, ii_pBactaAl, ii_pBactaEdge, N, big);
    if (big != pOut) {
        ii_Shrink(big, N, pOut, ii_out);
        free(big);
    }
    free(rgb);
    return 1;
}

#if defined(IOSITEMICONS_STANDALONE) || defined(IOSAUTOPLAY_RIG)
int iosItemIcons_Bacta(float level, float glow, unsigned char* pOut)
{
    glow = ii_Clamp01(glow);
    return ii_DrawBacta(level, glow, 0.45f + 0.35f * glow, 0, pOut);
}
#endif

int iosItemIcons_BactaFlash(float level, float flash, unsigned char* pOut)
{
    // (the whole liquid at once, as bright at its start as a full hold's glow)
    flash = ii_Clamp01(flash);
    return ii_DrawBacta(level, flash > 0.0f ? 1.0f : 0.0f, 0.8f * flash, 1, pOut);
}

// ------------------------------------------------------------ BACTA's count

// The game's HUD health digits (ui\sft\HelthNum16.sft): an SFNT font -- a
// 40-byte header ("SFNT", version 10, its charsets), each charset's first
// and last character and an 8-byte place for each (x in the sheet, width),
// then the sheet itself, a 16-bit .bm as the icons are. The game's has the
// digits 0-9 (and a colon) 5 pixels wide on a sheet 7 high, each lit in
// bright red over its dim unlit segments. A digit's lit pixels as a mask,
// a row a byte (bit 4 its leftmost pixel); 0 unless it is that, checked as
// strictly as the icons: every size, place and channel inside the file
#define II_DIG_W 5
#define II_DIG_H 7
static unsigned char ii_aDigit[10][II_DIG_H];
static unsigned char* ii_apCount[10]; // (counts 1-9, ii_out across)
static int ii_bDigits = 0;
static const unsigned char ii_aLilac[3] = { 248, 196, 255 }; // (the tank's palest highlight, its caps' brightest)

static void ii_FreeDigits(void)
{
    for (int i = 0; i < 10; i++) { free(ii_apCount[i]); ii_apCount[i] = NULL; }
    ii_bDigits = 0;
}

static int ii_ParseDigits(const unsigned char* d, int len, unsigned char aOut[10][II_DIG_H])
{
#define II_U16(o) ((unsigned int)d[(o)] | ((unsigned int)d[(o) + 1] << 8))
#define II_U32(o) ((unsigned int)d[(o)] | ((unsigned int)d[(o) + 1] << 8) | ((unsigned int)d[(o) + 2] << 16) | ((unsigned int)d[(o) + 3] << 24))
    if (!d || len < 40 || len > IOSITEMICONS_FILE_MAX || memcmp(d, "SFNT", 4) || II_U32(4) != 10) return 0;
    const unsigned int numSets = II_U32(20);
    if (numSets < 1 || numSets > 8) return 0;
    long off = 40, digits = -1;
    unsigned int first0 = 0;
    for (unsigned int k = 0; k < numSets; k++) {
        if (off + 4 > len) return 0;
        const unsigned int first = II_U16(off), last = II_U16(off + 2);
        if (last < first || last - first > 255) return 0;
        off += 4;
        const long n = (long)(last - first + 1);
        if (off + n * 8 > len) return 0;
        if (first <= '0' && last >= '9' && digits < 0) { digits = off; first0 = first; }
        off += n * 8;
    }
    if (digits < 0 || off + 0x88 > len) return 0;
    // the sheet: a 16-bit .bm, 7 high, its pixels all inside the file
    const unsigned char* b = d + off;
    const long blen = len - off;
    if (memcmp(b, "BM  ", 4)) return 0;
#undef II_U32
#define II_U32(o) ((unsigned int)b[(o)] | ((unsigned int)b[(o) + 1] << 8) | ((unsigned int)b[(o) + 2] << 16) | ((unsigned int)b[(o) + 3] << 24))
    const unsigned int key = II_U32(0x1C), is16 = II_U32(0x20), bpp = II_U32(0x24);
    const unsigned int rb = II_U32(0x28), gb = II_U32(0x2C), bb = II_U32(0x30), rs = II_U32(0x34), gs = II_U32(0x38), bs = II_U32(0x3C);
    const unsigned int W = II_U32(0x80), H = II_U32(0x84);
    if (is16 != 1 || bpp != 16 || H != II_DIG_H || W < 10 * II_DIG_W || W > 1024 || !rb || !gb || !bb || rb > 8 || gb > 8 || bb > 8) return 0;
    if (rs > 15 || gs > 15 || bs > 15 || rs + rb > 16 || gs + gb > 16 || bs + bb > 16) return 0;
    if (0x88 + (long)W * H * 2 > blen) return 0;
#undef II_U32
#define II_S32(o) ((int)((unsigned int)d[(o)] | ((unsigned int)d[(o) + 1] << 8) | ((unsigned int)d[(o) + 2] << 16) | ((unsigned int)d[(o) + 3] << 24)))
    const unsigned int mr = (1u << rb) - 1, mg = (1u << gb) - 1, mb = (1u << bb) - 1;
    for (int k = 0; k < 10; k++) {
        const long e = digits + (long)('0' + k - (int)first0) * 8;
        const int x0 = II_S32(e), w = II_S32(e + 4);
        if (w != II_DIG_W || x0 < 0 || x0 > (int)W - II_DIG_W) return 0;
        int lit = 0, rows = 0;
        for (int y = 0; y < II_DIG_H; y++) {
            unsigned char row = 0;
            for (int x = 0; x < II_DIG_W; x++) {
                const long o = 0x88 + ((long)y * W + x0 + x) * 2;
                const unsigned int v = b[o] | (b[o + 1] << 8);
                if (v == (key & 0xFFFF)) continue;
                const unsigned int r = ((v >> rs) & mr) * 255 / mr, g = ((v >> gs) & mg) * 255 / mg, bl = ((v >> bs) & mb) * 255 / mb;
                const unsigned int hi = r > g ? (r > bl ? r : bl) : (g > bl ? g : bl);
                if (hi >= 160) { row |= (unsigned char)(0x10 >> x); lit++; } // (a lit segment's pixel; the dim unlit ones dropped)
            }
            aOut[k][y] = row;
            rows += row != 0;
        }
        if (lit < 5 || lit > 30 || rows < 5) return 0; // (a digit: some of its 35 pixels, down most of its height)
    }
    for (int i = 0; i < 10; i++) // (ten different shapes)
        for (int j = i + 1; j < 10; j++)
            if (!memcmp(aOut[i], aOut[j], II_DIG_H)) return 0;
    return 1;
#undef II_S32
#undef II_U16
}

// A soft dark purple edge round the count: the icons' own faint one, and
// round it a little more, blurred, so it holds on the pink liquid
static void ii_CountEdge(const float* al, int N, int s, unsigned char* pOut)
{
    float* m = (float*)malloc(sizeof(float) * N * N);
    if (!m) { memset(pOut, 0, N * N * 4); return; }
    const int r = s >= 5 ? 1 : 0;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float v = 0.0f;
            for (int j = -r; j <= r; j++)
                for (int i = -r; i <= r; i++) v = fmaxf(v, al[ii_ClampI(y + j, N) * N + ii_ClampI(x + i, N)]);
            m[y * N + x] = v;
        }
    ii_GaussPil(m, N, 1, s * 0.25f);
    ii_EdgeUnder(al, N, (int)(s * 0.6f), pOut);
    for (int i = 0; i < N * N; i++) {
        const int a = (int)(fminf(fmaxf(m[i], 0.0f), 255.0f) * 0.8f);
        if (a > pOut[i * 4 + 3]) {
            pOut[i * 4 + 0] = (unsigned char)(0x31 * a / 255);
            pOut[i * 4 + 1] = 0;
            pOut[i * 4 + 2] = (unsigned char)(0x5a * a / 255);
            pOut[i * 4 + 3] = (unsigned char)a;
        }
    }
    free(m);
}

int iosItemIcons_PrepareDigits(const unsigned char* pSft, int len)
{
    ii_FreeDigits();
    unsigned char aMask[10][II_DIG_H];
    if (!ii_bReady || !ii_ParseDigits(pSft, len, aMask)) return 0;
    const int s = ii_s, N = II_N * s;
    unsigned char* big = (unsigned char*)malloc(N * N * 4);
    unsigned char* fin = (unsigned char*)malloc(N * N * 4);
    unsigned char* edge = (unsigned char*)malloc(N * N * 4);
    float* rgb = (float*)malloc(sizeof(float) * N * N * 3);
    float* al = (float*)malloc(sizeof(float) * N * N);
    int bOk = big && fin && edge && rgb && al;
    for (int c = 1; c <= 9 && bOk; c++) {
        // the digit's lit pixels on the icons' grid, the middle of its lit
        // columns the glass's (10.5 across; rows 9-15, 12.5 down), made big
        // as the icons are -- a half pixel's shift, where its lit columns
        // are an even number, made in the big picture's pixels
        int c0 = II_DIG_W, c1 = -1;
        for (int y = 0; y < II_DIG_H; y++)
            for (int x = 0; x < II_DIG_W; x++)
                if (aMask[c][y] & (0x10 >> x)) { if (x < c0) c0 = x; if (x > c1) c1 = x; }
        const float want = 10.5f - (c0 + c1 + 1) * 0.5f; // (the cell's left column)
        const int x0 = (int)floorf(want), shift = (int)lroundf((want - x0) * s);
        unsigned char src[II_N * II_N * 4];
        memset(src, 0, sizeof(src));
        for (int y = 0; y < II_DIG_H; y++)
            for (int x = 0; x < II_DIG_W; x++)
                if (aMask[c][y] & (0x10 >> x)) {
                    unsigned char* p = src + ((9 + y) * II_N + x0 + x) * 4;
                    p[0] = ii_aLilac[0]; p[1] = ii_aLilac[1]; p[2] = ii_aLilac[2]; p[3] = 255;
                }
        ii_Upscale(src, s, big);
        if (shift > 0)
            for (int y = 0; y < N; y++) {
                memmove(big + ((size_t)y * N + shift) * 4, big + (size_t)y * N * 4, (size_t)(N - shift) * 4);
                memset(big + (size_t)y * N * 4, 0, (size_t)shift * 4);
            }
        for (int i = 0; i < N * N; i++) {
            for (int k = 0; k < 3; k++) rgb[i * 3 + k] = big[i * 4 + k];
            al[i] = big[i * 4 + 3];
        }
        ii_CountEdge(al, N, s, edge);
        ii_Finish(rgb, al, edge, N, fin);
        ii_apCount[c] = (unsigned char*)malloc((size_t)ii_out * ii_out * 4);
        if (!ii_apCount[c]) { bOk = 0; break; }
        if (ii_out == N) memcpy(ii_apCount[c], fin, (size_t)N * N * 4);
        else ii_Shrink(fin, N, ii_apCount[c], ii_out);
    }
    free(big); free(fin); free(edge); free(rgb); free(al);
    if (!bOk) { ii_FreeDigits(); return 0; }
    memcpy(ii_aDigit, aMask, sizeof(ii_aDigit));
    ii_bDigits = 1;
    return 1;
}

const unsigned char* iosItemIcons_Count(int count)
{
    return (ii_bReady && ii_bDigits && count >= 1 && count <= 9) ? ii_apCount[count] : NULL;
}

static int ii_bHudStandIn = 0;

void iosItemIcons_SetHudStandIn(int bOn)
{
    ii_bHudStandIn = bOn != 0;
}

int iosItemIcons_HudLeavesOut(int bin)
{
    return ii_bHudStandIn && (bin == IOSITEMICONS_BIN_IRGOGGLES || bin == IOSITEMICONS_BIN_FIELDLIGHT);
}

static struct { float x, y, side, alpha, viewW, viewH; } ii_aPlace[2];

void iosItemIcons_SetStandInPlace(int item, float x, float y, float side, float alpha, float viewW, float viewH)
{
    if (item != IOSITEMICONS_LIGHT && item != IOSITEMICONS_IR) return;
    ii_aPlace[item].x = x;
    ii_aPlace[item].y = y;
    ii_aPlace[item].side = side;
    ii_aPlace[item].alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    ii_aPlace[item].viewW = viewW;
    ii_aPlace[item].viewH = viewH;
}

void iosItemIcons_DrawStandIns(unsigned char* pRgba, int w, int h)
{
    if (!ii_bHudStandIn || !ii_bReady || !pRgba || w <= 0 || h <= 0) return;
    const int n = ii_out;
    for (int k = IOSITEMICONS_LIGHT; k <= IOSITEMICONS_IR; k++) {
        const float side = ii_aPlace[k].side, a0 = ii_aPlace[k].alpha;
        if (side <= 0.0f || a0 <= 0.0f || ii_aPlace[k].viewW <= 0.0f || ii_aPlace[k].viewH <= 0.0f) continue;
        const unsigned char* src = iosItemIcons_Still(k == IOSITEMICONS_LIGHT ? IOSITEMICON_LIGHT_ON : IOSITEMICON_IR_ON);
        if (!src) continue;
        // (the overlay's points in the frame's pixels: the same scale as the
        // screen's, at which the pictures were made, so pixel for pixel)
        const float sx = w / ii_aPlace[k].viewW, sy = h / ii_aPlace[k].viewH;
        const float x0 = ii_aPlace[k].x * sx, y0 = ii_aPlace[k].y * sy, sw = side * sx, sh = side * sy;
        const int xa = x0 < 0.0f ? 0 : (int)x0, ya = y0 < 0.0f ? 0 : (int)y0;
        const int xb = (int)ceilf(x0 + sw) > w ? w : (int)ceilf(x0 + sw), yb = (int)ceilf(y0 + sh) > h ? h : (int)ceilf(y0 + sh);
        for (int y = ya; y < yb; y++) {
            const int v = (int)((y + 0.5f - y0) / sh * n);
            if (v < 0 || v >= n) continue;
            for (int x = xa; x < xb; x++) {
                const int u = (int)((x + 0.5f - x0) / sw * n);
                if (u < 0 || u >= n) continue;
                const unsigned char* s = src + ((size_t)v * n + u) * 4; // (premultiplied)
                const float a = s[3] / 255.0f * a0;
                if (a <= 0.0f) continue;
                unsigned char* d = pRgba + ((size_t)y * w + x) * 4;
                for (int c = 0; c < 3; c++) {
                    const float o = s[c] * a0 + d[c] * (1.0f - a);
                    d[c] = (unsigned char)(o > 255.0f ? 255.0f : o + 0.5f);
                }
            }
        }
    }
}

void iosItemIcons_Centre(int item, float* pX, float* pY)
{
    // (the mock-ups', but the lamp: its head (and its glow, lit) level with
    // the goggles' and the tank's middles -- it was 4 to 5 pt above them --
    // so its stand reaches a little lower; 5 pt lower than the mock-ups'
    // 12.6, at the overlay's 1.6 pt a pixel; the tank left of the middle)
    static const float a[3][2] = { { 12.0f, 9.475f }, { 11.9f, 11.9f }, { 10.5f, 12.0f } };
    if (item < 0 || item > 2) item = 0;
    if (pX) *pX = a[item][0];
    if (pY) *pY = a[item][1];
}

#ifndef IOSITEMICONS_STANDALONE
// Reads the three icons through the game's own file system (its GOBs), as it
// reads its other UI pictures
int iosItemIcons_ReadFiles(unsigned char aBuf[3][IOSITEMICONS_FILE_MAX], int aLen[3])
{
    static const char* aPath[3] = { "ui\\bm\\icfield16.bm", "ui\\bm\\icirgog16.bm", "ui\\bm\\icbacta16.bm" };
    if (!aBuf || !aLen || !std_g_pHS || !std_g_pHS->fileOpen) return 0;
    for (int k = 0; k < 3; k++) {
        stdFile_t f = std_g_pHS->fileOpen(aPath[k], "rb");
        if (!f) return 0;
        aLen[k] = (int)std_g_pHS->fileRead(f, aBuf[k], IOSITEMICONS_FILE_MAX);
        std_g_pHS->fileClose(f);
    }
    return 1;
}

// The HUD's health digits, read as the icons are (the game reads the same
// file for its own HUD)
int iosItemIcons_ReadDigits(unsigned char aBuf[IOSITEMICONS_FILE_MAX])
{
    if (!aBuf || !std_g_pHS || !std_g_pHS->fileOpen) return 0;
    stdFile_t f = std_g_pHS->fileOpen("ui\\sft\\HelthNum16.sft", "rb");
    if (!f) return 0;
    const int len = (int)std_g_pHS->fileRead(f, aBuf, IOSITEMICONS_FILE_MAX);
    std_g_pHS->fileClose(f);
    return len > 0 && len < IOSITEMICONS_FILE_MAX ? len : 0;
}

int iosItemIcons_Prepare(int s)
{
    static unsigned char aBuf[3][IOSITEMICONS_FILE_MAX], aSft[IOSITEMICONS_FILE_MAX];
    int aLen[3] = { 0, 0, 0 };
    const unsigned char* aBm[3] = { aBuf[0], aBuf[1], aBuf[2] };
    if (ii_bReady && ii_s == s && ii_out == II_N * s) return 1;
    if (!iosItemIcons_ReadFiles(aBuf, aLen)) return 0;
    if (!iosItemIcons_PrepareFrom(aBm, aLen, s)) return 0;
    const int n = iosItemIcons_ReadDigits(aSft);
    if (n > 0) iosItemIcons_PrepareDigits(aSft, n);
    return 1;
}
#endif

#endif // JK_ENHANCED
