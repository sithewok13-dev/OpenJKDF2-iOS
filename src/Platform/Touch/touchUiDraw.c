// Added: entire file. See touchUiDraw.h.

#include "Platform/Touch/touchUiDraw.h"
#include "Platform/Touch/touchUiFont.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// A line of text is this many times its size tall (about what UIKit's bold
// system font gives a label), and the text sits in it as UIKit centres it
#define TOUCHUI_LINE_HEIGHT 1.19f

void touchUiDraw_Begin(touchUiDrawList* l)
{
    l->numItems = 0;
    l->poolLen = 0;
    l->alpha = 1.0f;
}

static touchUiColor touchUiDraw_Fade(const touchUiDrawList* l, touchUiColor c)
{
    c.a *= l->alpha;
    return c;
}

static touchUiDrawItem* touchUiDraw_Add(touchUiDrawList* l, int type)
{
    if (l->numItems >= TOUCHUI_DRAW_MAX_ITEMS) return NULL;
    touchUiDrawItem* it = &l->aItems[l->numItems++];
    memset(it, 0, sizeof(*it));
    it->type = type;
    it->text = -1;
    return it;
}

void touchUiDraw_Circle(touchUiDrawList* l, float x, float y, float r, touchUiColor fill, float lw, touchUiColor border)
{
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_CIRCLE);
    if (!it) return;
    it->x0 = x; it->y0 = y; it->r = r; it->lw = lw;
    it->fill = touchUiDraw_Fade(l, fill);
    it->stroke = touchUiDraw_Fade(l, border);
}

void touchUiDraw_Arc(touchUiDrawList* l, float x, float y, float r, float lw, float a0, float sweep, int bRound, touchUiColor c)
{
    if (sweep <= 0.0f) return;
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_ARC);
    if (!it) return;
    it->x0 = x; it->y0 = y; it->r = r; it->lw = lw; it->a0 = a0;
    it->sweep = sweep > 6.2831853f ? 6.2831853f : sweep;
    it->bRound = bRound;
    it->stroke = touchUiDraw_Fade(l, c);
}

void touchUiDraw_Glow(touchUiDrawList* l, float x, float y, float r, float lw, float a0, float sweep, float blur, touchUiColor c)
{
    if (sweep <= 0.0f) return;
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_ARC);
    if (!it) return;
    it->x0 = x; it->y0 = y; it->r = r; it->lw = lw; it->a0 = a0;
    it->sweep = sweep > 6.2831853f ? 6.2831853f : sweep;
    it->bRound = 1;
    it->blur = blur;
    it->stroke = touchUiDraw_Fade(l, c);
}

void touchUiDraw_Wedge(touchUiDrawList* l, float x, float y, float deg0, float deg1, float r0, float r1,
                       touchUiColor fill, float lw, touchUiColor stroke)
{
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_WEDGE);
    if (!it) return;
    it->x0 = x; it->y0 = y; it->r0 = r0; it->r = r1; it->a0 = deg0; it->a1 = deg1; it->lw = lw;
    it->fill = touchUiDraw_Fade(l, fill);
    it->stroke = touchUiDraw_Fade(l, stroke);
}

void touchUiDraw_Line(touchUiDrawList* l, float x0, float y0, float x1, float y1, float lw, touchUiColor c)
{
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_LINE);
    if (!it) return;
    it->x0 = x0; it->y0 = y0; it->x1 = x1; it->y1 = y1; it->lw = lw;
    it->stroke = touchUiDraw_Fade(l, c);
}

void touchUiDraw_RRect(touchUiDrawList* l, float x, float y, float w, float h, float r, touchUiColor fill, float lw, touchUiColor border)
{
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_RRECT);
    if (!it) return;
    it->x0 = x; it->y0 = y; it->x1 = w; it->y1 = h; it->r = r; it->lw = lw;
    it->fill = touchUiDraw_Fade(l, fill);
    it->stroke = touchUiDraw_Fade(l, border);
}

void touchUiDraw_Tri(touchUiDrawList* l, float x0, float y0, float x1, float y1, float x2, float y2, int edgeMask, touchUiColor c)
{
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_TRI);
    if (!it) return;
    it->x0 = x0; it->y0 = y0; it->x1 = x1; it->y1 = y1; it->x2 = x2; it->y2 = y2;
    it->edgeMask = edgeMask;
    it->fill = touchUiDraw_Fade(l, c);
}

// Twice the signed area of a polygon
static float touchUiDraw_Area2(const float* p, const int* idx, int n)
{
    float a = 0.0f;
    for (int i = 0; i < n; i++) {
        const float* u = &p[2 * idx[i]];
        const float* v = &p[2 * idx[(i + 1) % n]];
        a += u[0] * v[1] - v[0] * u[1];
    }
    return a;
}

static int touchUiDraw_InTri(float px, float py, const float* a, const float* b, const float* c)
{
    float d1 = (px - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (py - b[1]);
    float d2 = (px - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (py - c[1]);
    float d3 = (px - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (py - a[1]);
    int neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

// Ear clipping. Edges of the polygon's own outline are smoothed; the cuts
// between its triangles are not (they would show as seams).
void touchUiDraw_Polygon(touchUiDrawList* l, const float* aXY, int n, touchUiColor c)
{
    enum { MAXN = 64 };
    if (n < 3 || n > MAXN) return;
    int idx[MAXN];
    for (int i = 0; i < n; i++) idx[i] = i;
    float s = touchUiDraw_Area2(aXY, idx, n) > 0.0f ? 1.0f : -1.0f;
    int m = n, guard = 0;
    while (m > 3 && guard++ < 4 * MAXN) {
        int bCut = 0;
        for (int i = 0; i < m; i++) {
            int ia = idx[(i + m - 1) % m], ib = idx[i], ic = idx[(i + 1) % m];
            const float* a = &aXY[2 * ia];
            const float* b = &aXY[2 * ib];
            const float* cc = &aXY[2 * ic];
            float cross = (b[0] - a[0]) * (cc[1] - a[1]) - (b[1] - a[1]) * (cc[0] - a[0]);
            if (cross * s <= 0.0f) continue; // a reflex corner
            int bEar = 1;
            for (int k = 0; k < m && bEar; k++) {
                int ik = idx[k];
                if (ik == ia || ik == ib || ik == ic) continue;
                if (touchUiDraw_InTri(aXY[2 * ik], aXY[2 * ik + 1], a, b, cc)) bEar = 0;
            }
            if (!bEar) continue;
            // corners a, b, c: the edge opposite a is b-c, opposite b is c-a (the cut), opposite c is a-b
            int mask = 0;
            if ((ib + 1) % n == ic || (ic + 1) % n == ib) mask |= 1;
            if ((ic + 1) % n == ia || (ia + 1) % n == ic) mask |= 2;
            if ((ia + 1) % n == ib || (ib + 1) % n == ia) mask |= 4;
            touchUiDraw_Tri(l, a[0], a[1], b[0], b[1], cc[0], cc[1], mask, c);
            for (int k = i; k < m - 1; k++) idx[k] = idx[k + 1];
            m--;
            bCut = 1;
            break;
        }
        if (!bCut) break; // (not a simple polygon)
    }
    if (m == 3) {
        int ia = idx[0], ib = idx[1], ic = idx[2];
        int mask = 0;
        if ((ib + 1) % n == ic || (ic + 1) % n == ib) mask |= 1;
        if ((ic + 1) % n == ia || (ia + 1) % n == ic) mask |= 2;
        if ((ia + 1) % n == ib || (ib + 1) % n == ia) mask |= 4;
        touchUiDraw_Tri(l, aXY[2 * ia], aXY[2 * ia + 1], aXY[2 * ib], aXY[2 * ib + 1], aXY[2 * ic], aXY[2 * ic + 1], mask, c);
    }
}

void touchUiDraw_Text(touchUiDrawList* l, const char* text, float x, float y, float size, float xscale, touchUiColor c)
{
    if (!text || !*text || c.a <= 0.0f) return;
    int len = (int)strlen(text);
    if (l->poolLen + len + 1 > TOUCHUI_DRAW_POOL) return;
    touchUiDrawItem* it = touchUiDraw_Add(l, TOUCHUI_DRAW_TEXT);
    if (!it) return;
    memcpy(&l->aPool[l->poolLen], text, len + 1);
    it->text = l->poolLen;
    l->poolLen += len + 1;
    it->x0 = x; it->y0 = y; it->size = size; it->xscale = xscale;
    it->fill = touchUiDraw_Fade(l, c);
}

float touchUiDraw_LineHeight(float size)
{
    return size * TOUCHUI_LINE_HEIGHT;
}

void touchUiDraw_MeasureText(const char* text, float size, float xscale, float* pW, float* pH)
{
    float k = size / (float)TOUCHUI_FONT_EM * xscale;
    float w = 0.0f, best = 0.0f;
    int lines = 1;
    for (const char* p = text ? text : ""; *p; p++) {
        if (*p == '\n') {
            if (w > best) best = w;
            w = 0.0f;
            lines++;
            continue;
        }
        unsigned char ch = (unsigned char)*p;
        if (ch < 32 || ch > 126) ch = '?';
        w += touchUiFont_aGlyphs[ch - 32].adv * k;
    }
    if (w > best) best = w;
    if (pW) *pW = best;
    if (pH) *pH = lines * touchUiDraw_LineHeight(size);
}

// (parts' widths w, gap g: the lines greedily filled to at most T wide; how many)
static int touchUiDraw_FillLines(const float* w, int n, float g, float T, int* aBreak)
{
    int lines = 1;
    float cur = 0.0f;
    for (int i = 0; i < n; i++) {
        const float add = (cur > 0.0f ? g : 0.0f) + w[i];
        if (cur > 0.0f && cur + add > T + 0.001f) {
            if (aBreak) aBreak[i] = 1;
            lines++;
            cur = w[i];
        }
        else {
            if (aBreak) aBreak[i] = 0;
            cur += add;
        }
    }
    return lines;
}

int touchUiDraw_BreakLines(char* t, float size, float fitMin, float maxW, int maxLines, float* pW)
{
    float tw = 0.0f;
    touchUiDraw_MeasureText(t, size, 1.0f, &tw, NULL);
    *pW = tw;
    if (tw * fitMin <= maxW || maxLines < 2) return 1;
    // the parts (after each three spaces), their widths
    enum { MAX_PARTS = 32 };
    const char* aStart[MAX_PARTS];
    int aLen[MAX_PARTS], n = 0;
    float aW[MAX_PARTS], widest = 0.0f, g = 0.0f;
    char part[512];
    touchUiDraw_MeasureText("   ", size, 1.0f, &g, NULL);
    for (const char* p = t; n < MAX_PARTS;) {
        const char* e = n < MAX_PARTS - 1 ? strstr(p, "   ") : NULL; // (the last one takes the rest)
        aStart[n] = p;
        aLen[n] = e ? (int)(e - p) : (int)strlen(p);
        snprintf(part, sizeof(part), "%.*s", aLen[n], p);
        touchUiDraw_MeasureText(part, size, 1.0f, &aW[n], NULL);
        widest = aW[n] > widest ? aW[n] : widest;
        n++;
        if (!e) break;
        p = e + 3;
    }
    if (n < 2) return 1;
    // the fewest lines that fit at fitMin (maxLines at the most), each as
    // short as it can be: the least widest line for that many
    int aBreak[MAX_PARTS], lines = 2;
    float T = tw;
    for (; lines <= maxLines; lines++) {
        float lo = widest, hi = tw;
        for (int it = 0; it < 40 && hi - lo > 0.01f; it++) {
            const float mid = (lo + hi) * 0.5f;
            if (touchUiDraw_FillLines(aW, n, g, mid, NULL) <= lines) hi = mid;
            else lo = mid;
        }
        T = hi;
        if (T * fitMin <= maxW || lines == maxLines) break;
    }
    lines = touchUiDraw_FillLines(aW, n, g, T, aBreak);
    // (the text again, a '\n' for each three spaces broken at)
    char out[512];
    int o = 0;
    float W = 0.0f, cur = 0.0f;
    for (int i = 0; i < n; i++) {
        if (i) {
            if (aBreak[i]) {
                W = cur > W ? cur : W;
                cur = 0.0f;
                if (o < (int)sizeof(out) - 1) out[o++] = '\n';
            }
            else {
                cur += g;
                for (int k = 0; k < 3 && o < (int)sizeof(out) - 1; k++) out[o++] = ' ';
            }
        }
        cur += aW[i];
        for (int k = 0; k < aLen[i] && o < (int)sizeof(out) - 1; k++) out[o++] = aStart[i][k];
    }
    out[o] = 0;
    W = cur > W ? cur : W;
    memcpy(t, out, (size_t)o + 1);
    *pW = W;
    return lines;
}
