// Added: entire file. See iosSaberStaff.h.

#ifdef JK_ENHANCED

#include "Enhanced/iosSaberStaff.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Enhanced/iosSaber.h"
#include "Enhanced/iosSaberFold.h"
#include "Enhanced/iosSaberShape.h"
#include "Enhanced/iosSaberStance.h"
#include "Enhanced/iosSaberThrow.h"
#include "Enhanced/iosSelfie.h"
#include "Enhanced/iosPhotoCam.h"
#include "Enhanced/iosFreeze.h"
#include "Enhanced/iosCinema.h"
#include "Enhanced/iosGame.h"
#include "Devices/sithSound.h"
#include "Devices/sithSoundMixer.h"
#include "Engine/rdCamera.h"
#include "Engine/sithCamera.h"
#include "Engine/rdroid.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithTime.h"
#include "General/stdMath.h"
#include "General/stdString.h"
#include "Main/Main.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdModel3.h"
#include "Primitives/rdPolyline.h"
#include "Primitives/rdVector.h"
#include "Raster/rdFace.h"
#include "World/jkPlayer.h"
#include "World/sithThing.h"
#include "World/sithWorld.h"
#include "jk.h"

// ------------------------------------------------------------ the numbers

// The game's hilts (a third person sabg.3do's, a first person hand's), when
// they can't be measured: length along the blade (its base at 0, the hilt
// below it), and the radius ours have for that length
#define IOSSABERSTAFF_HILT_LEN_3P 0.0277f
#define IOSSABERSTAFF_HILT_LEN_POV 0.0333f
#define IOSSABERSTAFF_RADIUS_3P 0.0866f   // (of the length: 0.0024)
#define IOSSABERSTAFF_RADIUS_POV 0.078f   // (0.0026)
#define IOSSABERSTAFF_SIDES 10
// FOLD: the halves' axes this far apart folded (in the radius)
#define IOSSABERSTAFF_FOLD_GAP 2.3f
// Held: turned in the hand this far (degrees, round its own z: the upper blade
// leaning to his left, the lower one clear of the floor), round the grip
// (along the blade from its base: the hand's middle)
#define IOSSABERSTAFF_TILT_DEG 45.0f
#define IOSSABERSTAFF_GRIP_Y (-0.01f)
// One of two sabers, first person: turned in the hand (round its grip, as far as the arm is in its
// guard, iosSaberStance) to lean across the view this far from upright (degrees, its front blade
// toward the middle) and into it this far (degrees: its front blade away from the eye, its back
// blade toward it, faded there as ever); third person the arm carries it so, not the hand
#define IOSSABERSTAFF_TWO_POV_LEAN 74.0f
#define IOSSABERSTAFF_TWO_POV_INTO 3.0f
// The left arm's turns at the shoulder (degrees): on the staff beside the right
// hand (roll), and let go (pitch)
#define IOSSABERSTAFF_GRIP_ROLL (-20.0f)
#define IOSSABERSTAFF_LETGO_PITCH (-25.0f)
// Out of the held pose this quickly as anything but carrying begins (well
// before a swing's hits count, a sixth of a second in), back this slowly
// after; standing still or not, this slowly (seconds); still: slower than
// this (units a second)
#define IOSSABERSTAFF_OUT_SEC 0.08f
#define IOSSABERSTAFF_IN_SEC 0.25f
#define IOSSABERSTAFF_STAND_SEC 0.25f
#define IOSSABERSTAFF_STILL_SPEED 0.2f
// (a gap in the game's clock this long or more, or the clock gone back: as things are now)
#define IOSSABERSTAFF_GAP_MS 500
// The spin: every 8 to 20 s (ms), standing still, nothing going on for 6 s, no
// one awake within 1.5 units: the left hand lets go, then one turn round the
// hand, eased, then it takes hold again
#define IOSSABERSTAFF_SPIN_EVERY_MIN 8000
#define IOSSABERSTAFF_SPIN_EVERY_MAX 20000
#define IOSSABERSTAFF_SPIN_CALM_MS 6000
#define IOSSABERSTAFF_SPIN_CLEAR 1.5f
#define IOSSABERSTAFF_SPIN_LEAD_MS 100
#define IOSSABERSTAFF_SPIN_MS 700
#define IOSSABERSTAFF_SPIN_BACK_MS 250
#define IOSSABERSTAFF_SPIN_RETRY_MS 1000
// First person: a blade is drawn see-through where it is nearer the eye than
// FADE (depth, units), and not at all nearer than NEAR (the game's own blade,
// at rest, starts a little further off than FADE: unchanged)
#define IOSSABERSTAFF_FADE 0.038f
#define IOSSABERSTAFF_NEAR 0.018f
// Thrown: the hilt's share of the game's thrown saber's length, back from its
// blade's base (where the two meet, its pommel); FOLD's gap, of the length
#define IOSSABERSTAFF_THROWN_HILT 0.22f
#define IOSSABERSTAFF_THROWN_GAP 0.046f
// The level of detail, by the hilt's length on the screen (pixels; first person always the most):
// this long or more, every part (the close-up look); shorter, the small parts left out (the grip's
// rings and grooves, the shroud's fins, the buttons, the emitter's rings: none of them a pixel
// wide by then), fewer sides; shorter still, only the hilt's main bands
#define IOSSABERSTAFF_LOD_NEAR_PX 64.0f
#define IOSSABERSTAFF_LOD_FAR_PX 20.0f

// ------------------------------------------------------------ the hilts

int iosSaberStaff_IsDouble(int hilt)
{
    return hilt == IOSSABER_HILT_STAFF || hilt == IOSSABER_HILT_FOLD;
}

int iosSaberStaff_SaberInHand(SithThing* pThing, int hand)
{
    return iosSaber_SaberInHand(pThing, hand);
}

int iosSaberStaff_SaberHilt(SithThing* pThing, int saber)
{
    return iosSaber_SaberHilt(pThing, saber);
}

int iosSaberStaff_HiltOf(SithThing* pThing, int hand)
{
    if (hand < 0 || hand > 1) return IOSSABER_HILT_NORMAL;
    return iosSaber_HandHilt(pThing, hand);
}

// (the hilt just drawn the saber's: the blade lit, or the model the game's saber hand or hilt; as iosSaber's)
static int iosSaberStaff_InHand(SithThing* pThing, const rdModel3* pHilt)
{
    if (pThing->jkFlags & JKFLAG_SABERON) return 1;
    return pHilt && (!__strnicmp(pHilt->filename, "sabv", 4) || !__strcmpi(pHilt->filename, "sabg.3do"));
}

// (the game's hilt in a model: the faces with its material, kysaber.mat; NULL if none)
static rdMaterial* iosSaberStaff_HiltMaterial(const rdModel3* m)
{
    for (uint32_t i = 0; m && m->aMaterials && i < m->sizeMaterials; i++) {
        rdMaterial* mat = m->aMaterials[i];
        if (!mat) continue;
        char name[64];
        stdString_SafeStrCopy(name, mat->mat_fpath, sizeof(name));
        for (char* q = name; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 'a' - 'A';
        if (strstr(name, "kysaber")) return mat;
    }
    return NULL;
}

// The game's hilt's length (its faces' reach down the blade), measured once a model (forgotten with
// the level: iosSaberStaff_FreeWorld)
static const rdModel3* iosSaberStaff_apMeasured[4];
static flex_t iosSaberStaff_aLen[4];
static flex_t iosSaberStaff_MeasureLength(const rdModel3* m, flex_t dflt)
{
    const rdModel3** apDone = iosSaberStaff_apMeasured;
    flex_t* aLen = iosSaberStaff_aLen;
    static int next = 0;
    if (!m || !m->numGeos) return dflt;
    for (int i = 0; i < 4; i++) if (apDone[i] == m) return aLen[i];
    const rdMaterial* mat = iosSaberStaff_HiltMaterial(m);
    flex_t lo = 0;
    int bAny = 0;
    for (uint32_t k = 0; mat && k < m->aGeos[0].numMeshes; k++) {
        const rdMesh* mesh = &m->aGeos[0].aMeshes[k];
        for (int f = 0; f < mesh->numFaces; f++) {
            const rdFace* face = &mesh->faces[f];
            if (face->material != mat || !face->vertexPosIdx) continue;
            for (uint32_t v = 0; v < face->numVertices; v++) {
                const int idx = face->vertexPosIdx[v];
                if (idx < 0 || idx >= mesh->numVertices) continue;
                if (!bAny || mesh->aVertices[idx].y < lo) lo = mesh->aVertices[idx].y;
                bAny = 1;
            }
        }
    }
    const flex_t len = (bAny && lo < (flex_t)-0.01 && lo > (flex_t)-0.08) ? -lo : dflt;
    apDone[next] = m;
    aLen[next] = len;
    next = (next + 1) % 4;
    return len;
}

// A hand's hilt's size (first person: the hand's model; third, the thing's hilt)
static void iosSaberStaff_Size(SithThing* pThing, int bPov, flex_t* pL, flex_t* pR)
{
    const rdModel3* m = NULL;
    if (bPov) m = playerThings[playerThingIdx].povModel.model3;
    else if (pThing && pThing->playerInfo) m = pThing->playerInfo->rd_thing.model3;
    const flex_t L = iosSaberStaff_MeasureLength(m, bPov ? IOSSABERSTAFF_HILT_LEN_POV : IOSSABERSTAFF_HILT_LEN_3P);
    *pL = L;
    *pR = L * (bPov ? IOSSABERSTAFF_RADIUS_POV : IOSSABERSTAFF_RADIUS_3P);
}

// ------------------------------------------------------------ frames

static void iosSaberStaff_Cross(rdVector3* out, const rdVector3* a, const rdVector3* b)
{
    const rdVector3 c = { a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x };
    *out = c;
}

static flex_t iosSaberStaff_Dot(const rdVector3* a, const rdVector3* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static int iosSaberStaff_Unit(rdVector3* v)
{
    const flex_t m = stdMath_Sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    if (m < (flex_t)1e-7) return 0;
    v->x /= m; v->y /= m; v->z /= m;
    return 1;
}

// v turned round the unit axis n by a (radians)
static void iosSaberStaff_Turn(rdVector3* out, const rdVector3* v, const rdVector3* n, float a)
{
    const flex_t c = cosf(a), s = sinf(a), d = iosSaberStaff_Dot(n, v) * (1 - c);
    rdVector3 nv;
    iosSaberStaff_Cross(&nv, n, v);
    out->x = v->x * c + nv.x * s + n->x * d;
    out->y = v->y * c + nv.y * s + n->y * d;
    out->z = v->z * c + nv.z * s + n->z * d;
}

// A frame from its axes (x, y; z = x cross y) and origin
static void iosSaberStaff_Axes(rdMatrix34* m, const rdVector3* x, const rdVector3* y, const rdVector3* o)
{
    m->rvec = *x;
    m->lvec = *y;
    iosSaberStaff_Cross(&m->uvec, x, y);
    m->scale = *o;
}

// FOLD's free half (STAFF's other half, open f = 1), in the held blade's frame
// (y up the blade, its base at 0, the held hilt down to -L): its pommel and
// its axes (y toward its emitter), open f (0 folded: alongside, the same way
// up, side the way it lies from the held half; 1 open: straight on). It turns
// half a turn round the hinge, between the pommels.
static void iosSaberStaff_FreeFrame(float f, flex_t L, flex_t R, const rdVector3* s, rdMatrix34* pOut)
{
    const flex_t g = R * IOSSABERSTAFF_FOLD_GAP;
    const float t = f * (float)M_PI;
    const rdVector3 x = { 1, 0, 0 }, y = { 0, 1, 0 }, z = { 0, 0, 1 };
    rdVector3 n;
    iosSaberStaff_Cross(&n, &y, s);
    iosSaberStaff_Unit(&n);
    const flex_t c = cosf(t), sn = sinf(t);
    pOut->scale.x = s->x * g * (flex_t)0.5 + (g * (flex_t)0.5) * (s->x * c);
    pOut->scale.y = -L - (g * (flex_t)0.5) * sn;
    pOut->scale.z = s->z * g * (flex_t)0.5 + (g * (flex_t)0.5) * (s->z * c);
    iosSaberStaff_Turn(&pOut->rvec, &x, &n, t);
    iosSaberStaff_Turn(&pOut->lvec, &y, &n, t);
    iosSaberStaff_Turn(&pOut->uvec, &z, &n, t);
}

// The way FOLD's free half lies from the held one (the held blade's frame, square
// to its blade): third person, the hand's x; first person, across the view
// (square to the line of sight and the blade, so both are as far from the eye),
// toward the middle of the view (the right hand's to its left)
static void iosSaberStaff_Side(int bPov, int hand, const rdMatrix34* pHeld, rdVector3* pS)
{
    pS->x = 1; pS->y = 0; pS->z = 0;
    if (!bPov || !rdCamera_g_pCurCamera) return;
    rdMatrix34 toCam;
    rdMatrix_Multiply34(&toCam, &rdCamera_g_pCurCamera->orient, pHeld);
    rdVector3 b = toCam.lvec, f;
    if (!iosSaberStaff_Unit(&b)) return;
    iosSaberStaff_Cross(&f, &b, &toCam.scale);
    const flex_t em = stdMath_Sqrt(iosSaberStaff_Dot(&toCam.scale, &toCam.scale)), fm = stdMath_Sqrt(iosSaberStaff_Dot(&f, &f));
    if (fm < (flex_t)0.2 * em || fm <= (flex_t)1e-7) return; // (the blade toward the eye: as the hand has it)
    f.x /= fm; f.y /= fm; f.z /= fm;
    if ((hand == 0) == (f.x > 0)) { f.x = -f.x; f.y = -f.y; f.z = -f.z; }
    rdVector3 l = { iosSaberStaff_Dot(&f, &toCam.rvec), 0, iosSaberStaff_Dot(&f, &toCam.uvec) };
    if (iosSaberStaff_Unit(&l)) *pS = l;
}

// ------------------------------------------------------------ the parts

static rdMatrix34 iosSaberStaff_toCam; // (the part being built: its frame to the camera's)
static rdThing* iosSaberStaff_pLine;
static int iosSaberStaff_lod; // (the hilt being built's level of detail: 0 every part, 1 the small ones left out, 2 the main bands)

static void iosSaberStaff_Part(const rdMatrix34* pHeld, const rdMatrix34* pLocal)
{
    rdMatrix34 part;
    rdMatrix_Multiply34(&part, pHeld, pLocal);
    rdMatrix_Multiply34(&iosSaberStaff_toCam, &rdCamera_g_pCurCamera->orient, &part);
}

static void iosSaberStaff_P(rdVector3* out, flex_t x, flex_t y, flex_t z)
{
    const rdVector3 p = { x, y, z };
    rdMatrix_TransformPoint34(out, &p, &iosSaberStaff_toCam);
}

// A ring of sides round the part's y (centre cx, cz), from y0 (radius r0) to y1 (r1); caps: 1 the y0
// end, 2 the y1 end
static void iosSaberStaff_Tube(int kind, flex_t y0, flex_t y1, flex_t r0, flex_t r1, int sides, flex_t cx, flex_t cz, int caps)
{
    if (sides > IOSSABERSHAPE_METAL_MAX_CORNERS) sides = IOSSABERSHAPE_METAL_MAX_CORNERS;
    rdVector3 a[IOSSABERSHAPE_METAL_MAX_CORNERS], b[IOSSABERSHAPE_METAL_MAX_CORNERS];
    for (int i = 0; i < sides; i++) {
        const float th = (float)i / sides * (float)(2.0 * M_PI);
        const flex_t c = cosf(th), s = sinf(th);
        iosSaberStaff_P(&a[i], cx + c * r0, y0, cz + s * r0);
        iosSaberStaff_P(&b[i], cx + c * r1, y1, cz + s * r1);
    }
    for (int i = 0; i < sides; i++) {
        const int j = (i + 1) % sides;
        const rdVector3 q[4] = { a[i], a[j], b[j], b[i] };
        const rdVector2 uv[4] = { { (flex_t)i / sides, 0 }, { (flex_t)(i + 1) / sides, 0 }, { (flex_t)(i + 1) / sides, 1 }, { (flex_t)i / sides, 1 } };
        iosSaberShape_MetalPoly(iosSaberStaff_pLine, kind, q, uv, 4);
    }
    rdVector2 uvc[IOSSABERSHAPE_METAL_MAX_CORNERS];
    for (int i = 0; i < sides; i++) { uvc[i].x = (flex_t)0.5; uvc[i].y = (flex_t)0.5; }
    if (caps & 1) {
        rdVector3 e[IOSSABERSHAPE_METAL_MAX_CORNERS];
        for (int i = 0; i < sides; i++) e[i] = a[sides - 1 - i];
        iosSaberShape_MetalPoly(iosSaberStaff_pLine, kind, e, uvc, sides);
    }
    if (caps & 2) iosSaberShape_MetalPoly(iosSaberStaff_pLine, kind, b, uvc, sides);
}

// A box: its middle, half sizes, in the part's frame (turned ang radians round y first)
static void iosSaberStaff_Box(int kind, flex_t x, flex_t y, flex_t z, flex_t hx, flex_t hy, flex_t hz, flex_t ang)
{
    rdVector3 c[8];
    const flex_t ca = cosf((float)ang), sa = sinf((float)ang);
    for (int i = 0; i < 8; i++) {
        const flex_t lx = (i & 1) ? hx : -hx, ly = (i & 2) ? hy : -hy, lz = (i & 4) ? hz : -hz;
        iosSaberStaff_P(&c[i], x + lx * ca - lz * sa, y + ly, z + lx * sa + lz * ca);
    }
    // (each face round its outside)
    static const int f[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
    static const rdVector2 uv[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    for (int k = 0; k < 6; k++) {
        const rdVector3 q[4] = { c[f[k][0]], c[f[k][1]], c[f[k][2]], c[f[k][3]] };
        iosSaberShape_MetalPoly(iosSaberStaff_pLine, kind, q, uv, 4);
    }
}

enum {
    IOSSABERSTAFF_SILVER = IOSSABERSHAPE_METAL_SILVER, IOSSABERSTAFF_BLACK = IOSSABERSHAPE_METAL_BLACK,
    IOSSABERSTAFF_RED = IOSSABERSHAPE_METAL_RED, IOSSABERSTAFF_DARK = IOSSABERSHAPE_METAL_DARK,
    IOSSABERSTAFF_CHROME = IOSSABERSHAPE_METAL_CHROME, IOSSABERSTAFF_REDBAND = IOSSABERSHAPE_METAL_REDBAND
};

// Maul's half: from the middle (y = 0) out to its emitter (y = L). Out from the
// middle: a black ring; silver; the ridged grip (four silver rings, dark
// grooves); silver; a black ring; the body (a red button and a silver one on
// its front, a red one at its side); a black ring; the finned shroud (a dark
// core in a cage of eight silver bars, rings at both ends); the grooved emitter.
static void iosSaberStaff_MaulHalf(flex_t L, flex_t R)
{
    if (iosSaberStaff_lod == 2) {
        // (far: the silver body, its two black rings, the shroud's cage, the emitter, end to end)
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, 0, L * 0.24f, R * 1.02f, R * 1.02f, 6, 0, 0, 1);
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.24f, L * 0.265f, R * 1.02f, R * 1.02f, 6, 0, 0, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.265f, L * 0.58f, R * 1.02f, R * 1.02f, 6, 0, 0, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.58f, L * 0.61f, R * 1.02f, R * 1.02f, 6, 0, 0, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.61f, L * 0.84f, R * 1.02f, R * 1.02f, 6, 0, 0, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.84f, L * 1.0f, R * 1.2f, R * 1.15f, 6, 0, 0, 1 | 2);
        return;
    }
    if (iosSaberStaff_lod == 1) {
        // (further off: the black rings, the silver body and grip, the red button, the shroud's cage, the emitter)
        const int S = 8;
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, 0, L * 0.025f, R * 1.06f, R * 1.06f, S, 0, 0, 1);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.025f, L * 0.24f, R * 1.03f, R * 1.03f, S, 0, 0, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.24f, L * 0.265f, R * 1.08f, R * 1.08f, S, 0, 0, 1 | 2);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.265f, L * 0.58f, R, R, S, 0, 0, 0);
        iosSaberStaff_Box(IOSSABERSTAFF_RED, 0, L * 0.36f, R * 1.0f, R * 0.32f, R * 0.32f, R * 0.18f, 0);
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.58f, L * 0.61f, R * 1.08f, R * 1.08f, S, 0, 0, 1 | 2);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.61f, L * 0.64f, R * 1.05f, R * 1.2f, S, 0, 0, 1);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.64f, L * 0.84f, R * 1.0f, R * 1.0f, S, 0, 0, 0); // (the cage: its bars, as one)
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.84f, L * 0.935f, R * 1.22f, R * 1.22f, S, 0, 0, 1 | 2);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.935f, L * 1.0f, R * 1.15f, R * 1.1f, S, 0, 0, 2);
        return;
    }
    const int S = IOSSABERSTAFF_SIDES;
    iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, 0, L * 0.025f, R * 1.06f, R * 1.06f, S, 0, 0, 1);
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.025f, L * 0.08f, R, R, S, 0, 0, 0);
    for (int k = 0; k < 4; k++) {
        const flex_t y = L * (0.08f + k * 0.03f);
        iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, y, y + L * 0.018f, R * 1.07f, R * 1.07f, S, 0, 0, 1 | 2);
        iosSaberStaff_Tube(IOSSABERSTAFF_DARK, y + L * 0.018f, y + L * 0.03f, R * 0.92f, R * 0.92f, S, 0, 0, 0);
    }
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.20f, L * 0.24f, R, R, S, 0, 0, 0);
    iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.24f, L * 0.265f, R * 1.08f, R * 1.08f, S, 0, 0, 1 | 2);
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.265f, L * 0.58f, R, R, S, 0, 0, 0);
    iosSaberStaff_Box(IOSSABERSTAFF_RED, 0, L * 0.36f, R * 1.0f, R * 0.32f, R * 0.32f, R * 0.18f, 0);
    iosSaberStaff_Box(IOSSABERSTAFF_SILVER, 0, L * 0.45f, R * 1.0f, R * 0.22f, R * 0.22f, R * 0.14f, 0);
    iosSaberStaff_Box(IOSSABERSTAFF_RED, R * 1.0f, L * 0.50f, 0, R * 0.16f, R * 0.30f, R * 0.30f, 0);
    iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.58f, L * 0.61f, R * 1.08f, R * 1.08f, S, 0, 0, 1 | 2);
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.61f, L * 0.64f, R * 1.05f, R * 1.2f, S, 0, 0, 1);
    iosSaberStaff_Tube(IOSSABERSTAFF_DARK, L * 0.64f, L * 0.84f, R * 0.72f, R * 0.72f, 8, 0, 0, 0);
    for (int k = 0; k < 8; k++) {
        const flex_t a = (flex_t)(k * 2.0 * M_PI / 8);
        iosSaberStaff_Box(IOSSABERSTAFF_SILVER, cosf((float)a) * R, L * 0.74f, sinf((float)a) * R, R * 0.26f, L * 0.10f, R * 0.07f, a);
    }
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.84f, L * 0.87f, R * 1.2f, R * 1.2f, S, 0, 0, 1 | 2);
    iosSaberStaff_Tube(IOSSABERSTAFF_DARK, L * 0.87f, L * 0.90f, R * 1.0f, R * 1.0f, S, 0, 0, 0);
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.90f, L * 0.935f, R * 1.25f, R * 1.25f, S, 0, 0, 1 | 2);
    iosSaberStaff_Tube(IOSSABERSTAFF_DARK, L * 0.935f, L * 0.955f, R * 1.05f, R * 1.05f, S, 0, 0, 0);
    iosSaberStaff_Tube(IOSSABERSTAFF_SILVER, L * 0.955f, L * 1.0f, R * 1.22f, R * 1.1f, S, 0, 0, 1 | 2);
}

// Dark Rey's half: from its pommel (y = 0, at the hinge) to its emitter (y = L):
// a black pommel ring, a short dark grip, a polished silver body with three
// wide red bands, a black flared emitter with two black prongs. The grip, body
// and bands are one run of pieces end to end, never one over another (so close
// together as a hilt is thin, the depth buffer can't tell which is in front:
// neither shows true), each step out closed by its own end; the body and bands
// are FOLD's own metals, so it reads silver with red bands held upright as
// well as open and turned in the hand.
static void iosSaberStaff_ReyHalf(flex_t L, flex_t R)
{
    // the grip, then the body: where each piece begins (the last: where the body ends), silver and red by turns
    static const float aY[] = { 0.05f, 0.16f, 0.24f, 0.325f, 0.43f, 0.515f, 0.62f, 0.705f, 0.74f };
    const int nPieces = (int)(sizeof(aY) / sizeof(aY[0])) - 1;
    const int lod = iosSaberStaff_lod, S = lod == 2 ? 6 : (lod ? 8 : IOSSABERSTAFF_SIDES);
    // the pommel (far: the grip silver too, from the pommel)
    if (lod < 2) iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, 0, L * aY[0], R * 1.02f, R * 1.02f, S, 0, 0, 1 | 2);
    for (int i = 0; i < nPieces; i++) {
        const int kind = !i ? (lod == 2 ? IOSSABERSTAFF_CHROME : IOSSABERSTAFF_DARK) : ((i & 1) ? IOSSABERSTAFF_CHROME : IOSSABERSTAFF_REDBAND);
        const flex_t y0 = (lod == 2 && !i) ? 0 : L * aY[i];
        iosSaberStaff_Tube(kind, y0, L * aY[i + 1], R * 0.95f, R * 0.95f, S, 0, 0, (lod == 2 && !i) ? 1 : 0);
    }
    // the emitter
    if (lod == 2) {
        iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * aY[nPieces], L * 0.96f, R * 1.0f, R * 1.1f, S, 0, 0, 1 | 2);
        return;
    }
    iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * aY[nPieces], L * 0.92f, R * 1.0f, R * 1.15f, S, 0, 0, 1);
    iosSaberStaff_Tube(IOSSABERSTAFF_BLACK, L * 0.92f, L * 0.96f, R * 1.15f, R * 0.9f, S, 0, 0, 2);
    for (int k = 0; k < 2; k++) {
        const flex_t sx = k ? -1.0f : 1.0f;
        iosSaberStaff_Box(IOSSABERSTAFF_BLACK, sx * R * 1.0f, L * 0.98f, 0, R * 0.22f, L * 0.08f, R * 0.32f, 0);
        if (!lod) iosSaberStaff_Box(IOSSABERSTAFF_BLACK, sx * R * 0.95f, L * 1.07f, 0, R * 0.14f, L * 0.04f, R * 0.22f, 0);
    }
}

// ------------------------------------------------------------ what each one is doing (third person)

typedef struct {
    SithThing* pThing;
    uint32_t signature;
    uint32_t lastMs;
    int bTime;
    float rest;    // (0..1) not doing anything but carrying it: held turned, the left hand on it
    float active;  // (0..1) lit and in hand (not drawn, put away, ...)
    float stand;   // (0..1) standing still
    rdModel3* pModel;
    int lshoulder;
} iosSaberStaffSlot;
// (each player's own, by their number, as the looks are kept (jkPlayerLook); the last, any other thing's while it is the one drawn)
#define IOSSABERSTAFF_SLOTS (JKPLAYER_NUM_INFOS + 1)
static iosSaberStaffSlot iosSaberStaff_aSlot[IOSSABERSTAFF_SLOTS];

// The animations a saber is carried through: standing, walking, running, turning, crouching, jumping,
// falling, landing and fidgeting (as iosSaberStance's low guard: anything else is the game's own)
static const int iosSaberStaff_aCarry[] = {
    SITH_ANIM_STAND, SITH_ANIM_WALK, SITH_ANIM_RUN, SITH_ANIM_WALKBACK, SITH_ANIM_STRAFELEFT, SITH_ANIM_STRAFERIGHT,
    SITH_ANIM_TURNLEFT, SITH_ANIM_TURNRIGHT, SITH_ANIM_CROUCHFORWARD, SITH_ANIM_CROUCHBACK,
    SITH_ANIM_RISING, SITH_ANIM_JUMP, SITH_ANIM_LEAP, SITH_ANIM_FALL, SITH_ANIM_LAND, SITH_ANIM_FIDGET, SITH_ANIM_FIDGET2,
};

static int iosSaberStaff_IsCarry(const SithThing* t, const rdKeyframe* k)
{
    const SithPuppetClass* pc = t->pPuppetClass;
    if (!pc) return 0;
    for (int m = 0; m < (int)(sizeof(pc->modes) / sizeof(pc->modes[0])); m++)
        for (int i = 0; i < (int)(sizeof(iosSaberStaff_aCarry) / sizeof(iosSaberStaff_aCarry[0])); i++)
            if (pc->modes[m].keyframe[iosSaberStaff_aCarry[i]].keyframe == k) return 1;
    return 0;
}

int iosSaberStaff_IsBusy(SithThing* t)
{
    if (!t) return 1;
    if (t->jkFlags & JKFLAG_SABERDAMAGE) return 1; // (the game counting a swing's hits)
    const rdPuppet* pup = t->renderData.puppet;
    if (!pup) return 0;
    for (int i = 0; i < 4; i++) {
        const rdPuppetTrack* tr = &pup->aTracks[i];
        if (!tr->status || !tr->keyframe || (tr->status & 8) || tr->playSpeed <= 0.01f) continue;
        if (!iosSaberStaff_IsCarry(t, tr->keyframe)) return 1;
    }
    return 0;
}

static int iosSaberStaff_IsActive(SithThing* t)
{
    return (t->jkFlags & JKFLAG_SABERON) && !(t->jkFlags & (JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT)) && t->playerInfo && t->playerInfo->rd_thing.model3
        && !(t == sithPlayer_g_pLocalPlayerThing && iosSelfie_IsActive());
}

int iosSaberStaff_NoFidget(SithThing* t)
{
    // (one saber, a STAFF or a FOLD not folded, lit and in hand)
    if (!t || t->type != SITH_THING_PLAYER || iosSaber_HasSecondBlade(t) || !(t->jkFlags & JKFLAG_SABERON)) return 0;
    const int hilt = iosSaberStaff_HiltOf(t, 0);
    if (!iosSaberStaff_IsDouble(hilt) || !t->playerInfo || !iosSaberStaff_InHand(t, t->playerInfo->rd_thing.model3)) return 0;
    return hilt == IOSSABER_HILT_STAFF || iosSaberFold_OpenFraction(t, iosSaberStaff_SaberInHand(t, 0)) > 0.0f;
}

static int iosSaberStaff_IsStanding(SithThing* t)
{
    if (!(t->attach_flags & (SITH_ATTACH_SURFACE | SITH_ATTACH_THINGFACE)) || (t->physicsParams.flags & SITH_PF_CROUCHING)) return 0;
    const flex_t v = stdMath_Sqrt(t->physicsParams.vel.x * t->physicsParams.vel.x + t->physicsParams.vel.y * t->physicsParams.vel.y);
    return v < IOSSABERSTAFF_STILL_SPEED;
}

static float iosSaberStaff_Toward(float x, float target, float step)
{
    return x < target ? fminf(target, x + step) : fmaxf(target, x - step);
}

static iosSaberStaffSlot* iosSaberStaff_SlotOf(SithThing* t)
{
    const uint32_t now = sithTime_g_msecGameTime;
    iosSaberStaffSlot* s = &iosSaberStaff_aSlot[IOSSABERSTAFF_SLOTS - 1];
    if (t->type == SITH_THING_PLAYER && t->actorParams.pPlayer) {
        const uintptr_t at = (uintptr_t)t->actorParams.pPlayer, first = (uintptr_t)&jkPlayer_playerInfos[0];
        const uintptr_t i = (at - first) / sizeof(jkPlayer_playerInfos[0]);
        if (at >= first && !((at - first) % sizeof(jkPlayer_playerInfos[0])) && i < JKPLAYER_NUM_INFOS) s = &iosSaberStaff_aSlot[i];
    }
    if (s->pThing != t || s->signature != t->signature) {
        // (new to its slot: a player joined, a new level; as it is now, its shoulder looked up once)
        memset(s, 0, sizeof(*s));
        s->pThing = t;
        s->signature = t->signature;
        s->lshoulder = -1;
    }
    // (moved on once a moment, by the game's clock)
    const float rest = iosSaberStaff_IsBusy(t) ? 0.0f : 1.0f, active = iosSaberStaff_IsActive(t) ? 1.0f : 0.0f;
    const float stand = iosSaberStaff_IsStanding(t) ? 1.0f : 0.0f;
    if (!s->bTime || now < s->lastMs || now - s->lastMs > IOSSABERSTAFF_GAP_MS) {
        s->bTime = 1;
        s->rest = rest;
        s->active = active;
        s->stand = stand;
    }
    else if (now != s->lastMs) {
        const float dt = (float)(now - s->lastMs) * 0.001f;
        s->rest = iosSaberStaff_Toward(s->rest, rest, dt / (rest < s->rest ? IOSSABERSTAFF_OUT_SEC : IOSSABERSTAFF_IN_SEC));
        s->active = iosSaberStaff_Toward(s->active, active, dt / (active < s->active ? IOSSABERSTAFF_OUT_SEC : IOSSABERSTAFF_IN_SEC));
        s->stand = iosSaberStaff_Toward(s->stand, stand, dt / IOSSABERSTAFF_STAND_SEC);
    }
    s->lastMs = now;
    // (the game counting a swing's hits: its own pose, exactly)
    if (t->jkFlags & JKFLAG_SABERDAMAGE) s->rest = 0.0f;
    return s;
}

static float iosSaberStaff_Smooth(float p)
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p * p * (3.0f - 2.0f * p);
}

// ------------------------------------------------------------ the spin (the local player's)

static struct {
    uint32_t nextMs;     // (due then; 0: not yet set)
    uint32_t startMs;    // (going since then; 0: not going)
    float heldDeg;       // (stopped part way: its angle, eased out with the held pose)
    uint32_t calmMs;     // nothing going on since then
    float lastHealth;
    SithThing* pPlayer;
    uint32_t rnd;
} iosSaberStaff_spin;

static uint32_t iosSaberStaff_Rand(uint32_t lo, uint32_t hi)
{
    iosSaberStaff_spin.rnd = iosSaberStaff_spin.rnd * 1664525u + 1013904223u;
    return lo + (iosSaberStaff_spin.rnd >> 8) % (hi - lo + 1);
}

// Its angle now (degrees), and how far the left hand has let go for it
static float iosSaberStaff_SpinDeg(uint32_t now, float* pLetGo)
{
    if (pLetGo) *pLetGo = 0.0f;
    if (!iosSaberStaff_spin.startMs || now < iosSaberStaff_spin.startMs) return 0.0f;
    const uint32_t u = now - iosSaberStaff_spin.startMs;
    const uint32_t end = IOSSABERSTAFF_SPIN_LEAD_MS + IOSSABERSTAFF_SPIN_MS;
    if (pLetGo) {
        if (u < IOSSABERSTAFF_SPIN_LEAD_MS) *pLetGo = (float)u / IOSSABERSTAFF_SPIN_LEAD_MS;
        else if (u < end) *pLetGo = 1.0f;
        else if (u < end + IOSSABERSTAFF_SPIN_BACK_MS) *pLetGo = 1.0f - (float)(u - end) / IOSSABERSTAFF_SPIN_BACK_MS;
    }
    if (u <= IOSSABERSTAFF_SPIN_LEAD_MS || u >= end) return 0.0f;
    return 360.0f * iosSaberStaff_Smooth((float)(u - IOSSABERSTAFF_SPIN_LEAD_MS) / IOSSABERSTAFF_SPIN_MS);
}

// (an actor awake this near)
static int iosSaberStaff_AnyoneNear(SithThing* p)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    for (int i = 0; w && i <= w->numThings; i++) {
        SithThing* t = &w->aThings[i];
        if (t == p || (t->type != SITH_THING_ACTOR && t->type != SITH_THING_PLAYER) || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED | SITH_TF_DISABLED))) continue;
        if (t->type == SITH_THING_ACTOR && (!t->actor || (t->actor->flags & SITHAI_MODE_SLEEPING))) continue;
        if (rdVector_Dist3(&t->position, &p->position) < IOSSABERSTAFF_SPIN_CLEAR) return 1;
    }
    return 0;
}

// (its saber one of these, open, one in hand)
static int iosSaberStaff_CanSpin(SithThing* p)
{
    if (iosSaber_HasSecondBlade(p)) return 0;
    const int hilt = iosSaberStaff_HiltOf(p, 0);
    if (hilt == IOSSABER_HILT_STAFF) return 1;
    return hilt == IOSSABER_HILT_FOLD && iosSaberFold_IsOpen(p, 0) && iosSaberFold_OpenFraction(p, iosSaberStaff_SaberInHand(p, 0)) >= 1.0f;
}

void iosSaberStaff_Swing(SithThing* pThing)
{
    if (pThing && pThing == sithPlayer_g_pLocalPlayerThing) {
        iosSaberStaff_spin.calmMs = sithTime_g_msecGameTime;
        if (iosSaberStaff_spin.startMs) {
            iosSaberStaff_spin.heldDeg = iosSaberStaff_SpinDeg(sithTime_g_msecGameTime, NULL);
            iosSaberStaff_spin.startMs = 0;
        }
    }
    iosSaberFold_Swing(pThing);
}

void iosSaberStaff_Tick(void)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    if (!p || p->type != SITH_THING_PLAYER || !sithWorld_g_pCurrentWorld) return;
    const uint32_t now = sithTime_g_msecGameTime;
    if (p != iosSaberStaff_spin.pPlayer || now < iosSaberStaff_spin.calmMs) {
        memset(&iosSaberStaff_spin, 0, sizeof(iosSaberStaff_spin));
        iosSaberStaff_spin.pPlayer = p;
        iosSaberStaff_spin.calmMs = now;
        iosSaberStaff_spin.lastHealth = p->actorParams.health;
        iosSaberStaff_spin.rnd = 0x5eed1234u ^ now;
    }
    // anything going on (a swing, a block, a hit taken, a Force power...), or hurt: not calm
    const int bBusy = iosSaberStaff_IsBusy(p);
    if (bBusy || p->actorParams.health < iosSaberStaff_spin.lastHealth) iosSaberStaff_spin.calmMs = now;
    iosSaberStaff_spin.lastHealth = p->actorParams.health;
    iosSaberStaff_Weight(-1); // (its pose's weights moved on)
    if (iosSaberStaff_spin.heldDeg != 0.0f) {
        iosSaberStaffSlot* s = iosSaberStaff_SlotOf(p);
        if (s->rest <= 0.0f || s->active <= 0.0f) iosSaberStaff_spin.heldDeg = 0.0f;
    }
    if (!iosSaberStaff_CanSpin(p) || !iosSaberStaff_IsActive(p)) {
        iosSaberStaff_spin.startMs = 0;
        iosSaberStaff_spin.nextMs = 0;
        return;
    }
    if (iosSaberStaff_spin.startMs) {
        if (bBusy) {
            // (cut short: eased out with the held pose)
            iosSaberStaff_spin.heldDeg = iosSaberStaff_SpinDeg(now, NULL);
            iosSaberStaff_spin.startMs = 0;
        }
        else if (now - iosSaberStaff_spin.startMs >= IOSSABERSTAFF_SPIN_LEAD_MS + IOSSABERSTAFF_SPIN_MS + IOSSABERSTAFF_SPIN_BACK_MS)
            iosSaberStaff_spin.startMs = 0;
        return;
    }
    if (!iosSaberStaff_spin.nextMs) iosSaberStaff_spin.nextMs = now + iosSaberStaff_Rand(IOSSABERSTAFF_SPIN_EVERY_MIN, IOSSABERSTAFF_SPIN_EVERY_MAX);
    if ((int32_t)(now - iosSaberStaff_spin.nextMs) < 0) return;
    // due: only standing still, well away from any fight, in the player's own view of the game
    const iosSaberStaffSlot* s = iosSaberStaff_SlotOf(p);
    const int bView = (sithCamera_g_pCurCamera == &sithCamera_g_aCameras[0] || sithCamera_g_pCurCamera == &sithCamera_g_aCameras[1])
                      && !iosPhotoCam_IsActive() && !iosSelfie_IsActive() && !iosFreeze_IsActive() && !iosCinema_IsActive() && !iosGame_IsHolding();
    const int bWater = p->sector && (p->sector->flags & SITH_SECTOR_UNDERWATER);
    if (!bView || bWater || bBusy || s->rest < 1.0f || s->active < 1.0f || s->stand < 1.0f || now - iosSaberStaff_spin.calmMs < IOSSABERSTAFF_SPIN_CALM_MS
        || iosSaberStaff_AnyoneNear(p)) {
        iosSaberStaff_spin.nextMs = now + IOSSABERSTAFF_SPIN_RETRY_MS;
        return;
    }
    iosSaberStaff_spin.startMs = now ? now : 1;
    iosSaberStaff_spin.nextMs = now + IOSSABERSTAFF_SPIN_LEAD_MS + IOSSABERSTAFF_SPIN_MS + IOSSABERSTAFF_SPIN_BACK_MS
                                + iosSaberStaff_Rand(IOSSABERSTAFF_SPIN_EVERY_MIN, IOSSABERSTAFF_SPIN_EVERY_MAX);
    // the whoosh: one of the game's own swing sounds
    static const char* aSwing[3] = { "LtSaberSwing01.WAV", "LtSaberSwing02.WAV", "LtSaberSwing03.WAV" };
    sithSound* pSnd = sithSound_Load((char*)aSwing[iosSaberStaff_Rand(0, 2)], 0);
    if (pSnd) sithSoundMixer_PlaySoundThing(pSnd, p, 1.0f, 0.5f, 2.5f, SITHSOUNDFLAG_FOLLOWSTHING);
}

// The local player's spin now (degrees), and its hand let go for it
static float iosSaberStaff_LocalSpin(SithThing* t, float rest, float* pLetGo)
{
    if (pLetGo) *pLetGo = 0.0f;
    if (!t || t != sithPlayer_g_pLocalPlayerThing || t != iosSaberStaff_spin.pPlayer) return 0.0f;
    if (iosSaberStaff_spin.startMs) return iosSaberStaff_SpinDeg(sithTime_g_msecGameTime, pLetGo);
    return iosSaberStaff_spin.heldDeg * rest;
}

// ------------------------------------------------------------ where it is held

// pHand turned round its grip, w of the way, so its blade points along pWant (a direction seen from the
// camera: x right, y ahead, z up); 0 if it can't be (pOut as pHand)
static int iosSaberStaff_PointInView(const rdMatrix34* pHand, const rdVector3* pWant, float w, rdMatrix34* pOut)
{
    *pOut = *pHand;
    if (!rdCamera_g_pCurCamera || w <= 0.0f) return 0;
    rdMatrix34 fromCam;
    rdMatrix_InvertOrtho34(&fromCam, &rdCamera_g_pCurCamera->orient);
    rdVector3 want, now = pHand->lvec, n, grip, off, turned;
    rdMatrix_TransformVector34(&want, pWant, &fromCam);
    if (!iosSaberStaff_Unit(&want) || !iosSaberStaff_Unit(&now)) return 0;
    iosSaberStaff_Cross(&n, &now, &want);
    const flex_t sn = stdMath_Sqrt(iosSaberStaff_Dot(&n, &n)), c = iosSaberStaff_Dot(&now, &want);
    if (sn < (flex_t)1e-5) return 0; // (already; or straight opposite: left)
    n.x /= sn; n.y /= sn; n.z /= sn;
    const float a = atan2f((float)sn, (float)c) * w;
    iosSaberStaff_Turn(&pOut->rvec, &pHand->rvec, &n, a);
    iosSaberStaff_Turn(&pOut->lvec, &pHand->lvec, &n, a);
    iosSaberStaff_Turn(&pOut->uvec, &pHand->uvec, &n, a);
    // (the grip stays where it is: the origin turned round it)
    rdVector_Copy3(&grip, &pHand->scale);
    rdVector_ScaleAdd3Acc(&grip, &pHand->lvec, IOSSABERSTAFF_GRIP_Y);
    rdVector_Sub3(&off, &pHand->scale, &grip);
    iosSaberStaff_Turn(&turned, &off, &n, a);
    rdVector_Add3(&pOut->scale, &grip, &turned);
    return 1;
}

int iosSaberStaff_HeldFrame(SithThing* pThing, int hand, int bPov, const rdModel3* pHilt, const rdMatrix34* pHand, rdMatrix34* pOut)
{
    *pOut = *pHand;
    if (!pThing || hand < 0 || hand > 1) return 0;
    const int hilt = iosSaberStaff_HiltOf(pThing, hand);
    if (!iosSaberStaff_IsDouble(hilt) || !iosSaberStaff_InHand(pThing, pHilt)) return 0;
    if (hand && !iosSaber_HasSecondBlade(pThing)) return 0;
    // (a selfie poses the arms and the saber itself)
    if (pThing == sithPlayer_g_pLocalPlayerThing && iosSelfie_IsActive()) return 1;
    if (iosSaber_HasSecondBlade(pThing)) {
        // two sabers: the arm carries each as a staff (iosSaberStance), third person by its joints; first
        // person turned across the view in the hand too, as far as that arm is in its guard
        if (!bPov || pThing != sithPlayer_g_pLocalPlayerThing) return 1;
        const float open = hilt == IOSSABER_HILT_STAFF ? 1.0f : iosSaberFold_OpenFraction(pThing, iosSaberStaff_SaberInHand(pThing, hand));
        const float w = iosSaberStance_Weight(pThing, 2 + hand) * iosSaberStaff_Smooth(open);
        const float lean = IOSSABERSTAFF_TWO_POV_LEAN * (float)M_PI / 180.0f, into = IOSSABERSTAFF_TWO_POV_INTO * (float)M_PI / 180.0f;
        const rdVector3 want = { (hand ? 1.0f : -1.0f) * sinf(lean) * cosf(into), sinf(into), cosf(lean) * cosf(into) };
        iosSaberStaff_PointInView(pHand, &want, w, pOut);
        return 1;
    }
    const iosSaberStaffSlot* s = iosSaberStaff_SlotOf(pThing);
    const float open = hilt == IOSSABER_HILT_STAFF ? 1.0f : iosSaberFold_OpenFraction(pThing, iosSaberStaff_SaberInHand(pThing, hand));
    float deg = iosSaberStaff_LocalSpin(pThing, s->rest, NULL);
    if (!bPov) deg += IOSSABERSTAFF_TILT_DEG * iosSaberStaff_Smooth(s->rest) * iosSaberStaff_Smooth(s->active) * iosSaberStaff_Smooth(open);
    if (fabsf(deg) < 0.01f) return 1;
    // (round the grip: there, turned round z, back)
    rdMatrix34 r, t, a, b;
    const rdVector3 pyr = { 0.0f, deg, 0.0f };
    rdMatrix_BuildRotate34(&r, &pyr);
    rdMatrix_Identity34(&t);
    t.scale.y = IOSSABERSTAFF_GRIP_Y;
    rdMatrix_Multiply34(&a, &t, &r);
    t.scale.y = -IOSSABERSTAFF_GRIP_Y;
    rdMatrix_Multiply34(&b, &a, &t);
    rdMatrix_Multiply34(pOut, pHand, &b);
    return 1;
}

int iosSaberStaff_SecondBlade(SithThing* pThing, int hand, int bPov, const rdMatrix34* pHeld, rdMatrix34* pOut)
{
    if (!pThing || hand < 0 || hand > 1) return 0;
    const int hilt = iosSaberStaff_HiltOf(pThing, hand);
    if (!iosSaberStaff_IsDouble(hilt)) return 0;
    flex_t L, R;
    iosSaberStaff_Size(pThing, bPov, &L, &R);
    rdVector3 side = { 1, 0, 0 };
    float f = 1.0f;
    if (hilt == IOSSABER_HILT_FOLD) {
        f = iosSaberFold_OpenFraction(pThing, iosSaberStaff_SaberInHand(pThing, hand));
        iosSaberStaff_Side(bPov, hand, pHeld, &side);
    }
    rdMatrix34 local;
    iosSaberStaff_FreeFrame(f, L, R, &side, &local);
    rdVector_ScaleAdd3Acc(&local.scale, &local.lvec, L); // (its emitter)
    rdMatrix_Multiply34(pOut, pHeld, &local);
    return 1;
}

// The level of detail for a hilt of length L held so (pHeld: the blade's base): by its length on the
// screen at its depth; first person, or at or behind the eye, every part
static int iosSaberStaff_Lod(int bPov, const rdMatrix34* pHeld, flex_t L)
{
    if (bPov || !rdCamera_g_pCurCamera || rdCamera_g_pCurCamera->focalLength <= (flex_t)0) return 0;
    rdMatrix34 toCam;
    rdMatrix_Multiply34(&toCam, &rdCamera_g_pCurCamera->orient, pHeld);
    const flex_t depth = toCam.scale.y;
    if (depth <= L) return 0;
    const flex_t px = L * rdCamera_g_pCurCamera->focalLength / depth;
    return px >= (flex_t)IOSSABERSTAFF_LOD_NEAR_PX ? 0 : (px >= (flex_t)IOSSABERSTAFF_LOD_FAR_PX ? 1 : 2);
}

void iosSaberStaff_DrawHilt(SithThing* pThing, int hand, int bPov, rdThing* pLine, const rdMatrix34* pHeld)
{
    if (!pThing || !pLine || !pLine->polyline || !pHeld || !rdCamera_g_pCurCamera || hand < 0 || hand > 1) return;
    const int hilt = iosSaberStaff_HiltOf(pThing, hand);
    if (!iosSaberStaff_IsDouble(hilt) || !iosSaberShape_MetalsReady()) return;
    flex_t L, R;
    iosSaberStaff_Size(pThing, bPov, &L, &R);
    iosSaberStaff_pLine = pLine;
    iosSaberStaff_lod = iosSaberStaff_Lod(bPov, pHeld, L);
    const rdVector3 x = { 1, 0, 0 }, y = { 0, 1, 0 }, pommel = { 0, -L, 0 };
    rdMatrix34 held, free;
    iosSaberStaff_Axes(&held, &x, &y, &pommel);
    if (hilt == IOSSABER_HILT_STAFF) {
        // two halves from the middle, mirror images
        iosSaberStaff_Part(pHeld, &held);
        iosSaberStaff_MaulHalf(L, R);
        iosSaberStaff_FreeFrame(1.0f, L, R, &x, &free);
        iosSaberStaff_Part(pHeld, &free);
        iosSaberStaff_MaulHalf(L, R);
        return;
    }
    // FOLD: the held half, the hinge block across to the free half, the free half
    rdVector3 s;
    iosSaberStaff_Side(bPov, hand, pHeld, &s);
    const float f = iosSaberFold_OpenFraction(pThing, iosSaberStaff_SaberInHand(pThing, hand));
    iosSaberStaff_Part(pHeld, &held);
    iosSaberStaff_ReyHalf(L, R);
    {
        const flex_t g = R * IOSSABERSTAFF_FOLD_GAP;
        rdVector3 n, at = { s.x * g * (flex_t)0.5, -L - L * (flex_t)0.03, s.z * g * (flex_t)0.5 };
        rdMatrix34 block, pin;
        iosSaberStaff_Axes(&block, &s, &y, &at);
        iosSaberStaff_Part(pHeld, &block);
        iosSaberStaff_Box(IOSSABERSTAFF_BLACK, 0, 0, 0, g * 0.5f + R * 0.9f, L * 0.06f, R * 1.05f, 0);
        iosSaberStaff_Cross(&n, &y, &s);
        at.y = -L;
        iosSaberStaff_Axes(&pin, &s, &n, &at);
        iosSaberStaff_Part(pHeld, &pin);
        if (iosSaberStaff_lod < 2) iosSaberStaff_Tube(IOSSABERSTAFF_DARK, -R * 1.15f, R * 1.15f, R * 0.45f, R * 0.45f, 8, 0, 0, 1 | 2);
    }
    iosSaberStaff_FreeFrame(f, L, R, &s, &free);
    iosSaberStaff_Part(pHeld, &free);
    iosSaberStaff_ReyHalf(L, R);
}

// ------------------------------------------------------------ first person

void iosSaberStaff_BeginPovHand(SithThing* pActor, rdThing* pPov, int hand)
{
    rdModel3_pSkipMaterial = NULL;
    if (!pActor || !pPov || !pPov->model3 || hand < 0 || hand > 1) return;
    if (!iosSaberStaff_IsDouble(iosSaberStaff_HiltOf(pActor, hand)) || !iosSaberStaff_InHand(pActor, pPov->model3)) return;
    if (hand && !iosSaber_HasSecondBlade(pActor)) return;
    rdModel3_pSkipMaterial = iosSaberStaff_HiltMaterial(pPov->model3);
}

void iosSaberStaff_EndPovHand(void)
{
    rdModel3_pSkipMaterial = NULL;
}

// (for checks: the last first person blades' tips on screen)
static float iosSaberStaff_aPovTip[4][4];
static uint32_t iosSaberStaff_aPovTipFrame[4];

int iosSaberStaff_PovTip(int i, float* pX, float* pY, float* pDepth)
{
    if (i >= 4 && i < 8 && iosSaberStaff_aPovTipFrame[i - 4] == (uint32_t)rdroid_frameTrue) {
        *pX = *pY = 0.0f;
        *pDepth = iosSaberStaff_aPovTip[i - 4][3]; // (its base's depth)
        return 1;
    }
    if (i < 0 || i > 3 || iosSaberStaff_aPovTipFrame[i] != (uint32_t)rdroid_frameTrue) return 0;
    *pX = iosSaberStaff_aPovTip[i][0];
    *pY = iosSaberStaff_aPovTip[i][1];
    *pDepth = iosSaberStaff_aPovTip[i][2];
    return 1;
}

// One stretch of a blade (from a to b along it), drawn see-through or not
static void iosSaberStaff_Stretch(rdThing* pLine, const rdMatrix34* pFrame, flex_t a, flex_t b, int bSeeThrough)
{
    rdPolyline* l = pLine->polyline;
    const flex_t len = l->length, rb = l->baseRadius, rt = l->tipRadius;
    const uint32_t type = l->face.type, tipType = l->tipFace.type;
    rdMatrix34 m = *pFrame;
    rdVector_ScaleAdd3Acc(&m.scale, &m.lvec, a);
    l->length = b - a;
    if (iosSaberShape_Get() != IOSSABERSHAPE_BEAM && len > 0) {
        // (the game's tapering blade: as wide at each end as it is there; FILM's is even)
        l->baseRadius = rb + (rt - rb) * (a / len);
        l->tipRadius = rb + (rt - rb) * (b / len);
    }
    if (bSeeThrough) {
        l->face.type |= RD_FF_TEX_TRANSLUCENT;
        l->tipFace.type |= RD_FF_TEX_TRANSLUCENT;
    }
    rdThing_Draw(pLine, &m);
    l->length = len;
    l->baseRadius = rb;
    l->tipRadius = rt;
    l->face.type = type;
    l->tipFace.type = tipType;
}

// A blade of these in first person: as ever where it is further from the eye than IOSSABERSTAFF_FADE,
// see-through nearer, cut off at IOSSABERSTAFF_NEAR (each changing with the depth along it, so a
// blade swung at the eye fades and shortens smoothly, from its near end)
static void iosSaberStaff_PovBlade(SithThing* pActor, rdThing* pLine, const rdMatrix34* pFrame, int blade, int tipSlot)
{
    iosSaber_BeginBlade(pActor, pLine, blade);
    rdPolyline* l = pLine->polyline;
    if (!l || !rdCamera_g_pCurCamera) {
        iosSaber_EndBlade(pLine);
        return;
    }
    rdMatrix34 toCam;
    rdMatrix_Multiply34(&toCam, &rdCamera_g_pCurCamera->orient, pFrame);
    const flex_t len = l->length, d0 = toCam.scale.y, step = toCam.lvec.y, d1 = d0 + step * len;
    if (tipSlot >= 0 && tipSlot < 4) {
        const flex_t tx = toCam.scale.x + toCam.lvec.x * len, tz = toCam.scale.z + toCam.lvec.z * len;
        iosSaberStaff_aPovTip[tipSlot][0] = d1 > (flex_t)0.0001 ? (float)(tx / d1) : 0.0f;
        iosSaberStaff_aPovTip[tipSlot][1] = d1 > (flex_t)0.0001 ? (float)(tz / d1) : 0.0f;
        iosSaberStaff_aPovTip[tipSlot][2] = (float)d1;
        iosSaberStaff_aPovTip[tipSlot][3] = (float)d0;
        iosSaberStaff_aPovTipFrame[tipSlot] = (uint32_t)rdroid_frameTrue;
    }
    if (d0 >= IOSSABERSTAFF_FADE && d1 >= IOSSABERSTAFF_FADE) {
        rdThing_Draw(pLine, (rdMatrix34*)pFrame);
        iosSaber_EndBlade(pLine);
        return;
    }
    // (where along it the depth is at least D: from u0 to u1)
#define IOSSABERSTAFF_SPAN(D, u0, u1) do { \
        if (step > (flex_t)1e-6) { u0 = ((D) - d0) / step; u1 = len; } \
        else if (step < (flex_t)-1e-6) { u0 = 0; u1 = (d0 - (D)) / -step; } \
        else { u0 = 0; u1 = d0 >= (D) ? len : 0; } \
        if (u0 < 0) u0 = 0; \
        if (u1 > len) u1 = len; } while (0)
    flex_t sa, sb, va, vb;
    IOSSABERSTAFF_SPAN(IOSSABERSTAFF_FADE, sa, sb);
    IOSSABERSTAFF_SPAN(IOSSABERSTAFF_NEAR, va, vb);
#undef IOSSABERSTAFF_SPAN
    const flex_t minLen = (flex_t)0.0005;
    if (sb - sa > minLen) iosSaberStaff_Stretch(pLine, pFrame, sa, sb, 0);
    // (the see-through stretch: the rest of what is seen, on the eye's side)
    if (vb - va > minLen) {
        flex_t ta = va, tb = vb;
        if (sb - sa > minLen) {
            if (step < 0) ta = sb;
            else tb = sa;
        }
        if (tb - ta > minLen) iosSaberStaff_Stretch(pLine, pFrame, ta, tb, 1);
    }
    iosSaber_EndBlade(pLine);
}

int iosSaberStaff_DrawPovBlades(SithThing* pActor, rdThing* pLine, const rdModel3* pPovModel, const rdMatrix34* pHand, int hand, int blade)
{
    rdMatrix34 held, second;
    if (!pLine || !pLine->polyline || !iosSaberStaff_HeldFrame(pActor, hand, 1, pPovModel, pHand, &held)) return 0;
    iosSaberStaff_PovBlade(pActor, pLine, &held, blade, hand * 2);
    if (iosSaberStaff_SecondBlade(pActor, hand, 1, &held, &second)) iosSaberStaff_PovBlade(pActor, pLine, &second, blade | IOSSABER_BLADE_SECOND, hand * 2 + 1);
    return 1;
}

// ------------------------------------------------------------ third person: the left arm

void iosSaberStaff_PoseArms(rdThing* pRdThing)
{
    SithThing* t = pRdThing ? pRdThing->pThing : NULL;
    if (!t || t->type != SITH_THING_PLAYER || pRdThing != &t->renderData || !pRdThing->model3 || !pRdThing->paJointMatrices || !t->pPuppetClass) return;
    const int hilt = iosSaberStaff_HiltOf(t, 0);
    if (!iosSaberStaff_IsDouble(hilt) || iosSaber_HasSecondBlade(t) || !t->playerInfo || !iosSaberStaff_InHand(t, t->playerInfo->rd_thing.model3)) return;
    // (a selfie poses the arms itself; the player's own body unseen in first person, as the game poses it)
    if (t == sithPlayer_g_pLocalPlayerThing && (iosSelfie_IsActive() || (sithCamera_g_pCurCamera == &sithCamera_g_aCameras[0] && !iosPhotoCam_IsActive())))
        return;
    iosSaberStaffSlot* s = iosSaberStaff_SlotOf(t);
    if (s->pModel != pRdThing->model3) {
        static const char* const aName[] = { "lshouldr", "lshoulder" };
        s->pModel = pRdThing->model3;
        s->lshoulder = -1;
        for (int i = 0; i < (int)s->pModel->numHNodes && s->lshoulder < 0; i++) {
            const char* name = s->pModel->aHierarchyNodes[i].name;
            const int l = (int)strlen(name);
            for (int k = 0; k < 2; k++) {
                const int n = (int)strlen(aName[k]);
                if (l >= n && !__strcmpi(name + l - n, aName[k])) s->lshoulder = i;
            }
        }
    }
    if (s->lshoulder < 0) return;
    float letGo = 0.0f;
    iosSaberStaff_LocalSpin(t, s->rest, &letGo);
    const float active = iosSaberStaff_Smooth(s->active);
    float grip = 0.0f, let = 0.0f;
    if (hilt == IOSSABER_HILT_STAFF) {
        // both hands on it standing still; let go to swing (the game's swings are one-handed), to walk
        // and run (the game's own arm swings), and for the spin
        grip = iosSaberStaff_Smooth(s->rest) * iosSaberStaff_Smooth(s->stand) * (1.0f - letGo);
        let = (1.0f - grip) * active;
        grip *= active;
    }
    else {
        // FOLD: open, one hand; folded, both as the game holds its saber
        let = iosSaberStaff_Smooth(iosSaberFold_OpenFraction(t, iosSaberStaff_SaberInHand(t, 0))) * active;
    }
    if (grip <= 0.001f && let <= 0.001f) return;
    const rdVector3 pyr = { IOSSABERSTAFF_LETGO_PITCH * let, 0.0f, IOSSABERSTAFF_GRIP_ROLL * grip };
    rdMatrix_PreRotate34(&pRdThing->paJointMatrices[s->lshoulder], &pyr);
}

float iosSaberStaff_Weight(int what)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    if (!p || p->type != SITH_THING_PLAYER) return 0.0f;
    const iosSaberStaffSlot* s = iosSaberStaff_SlotOf(p);
    if (what < 0) return 0.0f;
    const int hilt = iosSaberStaff_HiltOf(p, 0);
    if (what == 0) {
        if (!iosSaberStaff_IsDouble(hilt) || iosSaber_HasSecondBlade(p)) return 0.0f;
        const float open = hilt == IOSSABER_HILT_STAFF ? 1.0f : iosSaberFold_OpenFraction(p, iosSaberStaff_SaberInHand(p, 0));
        return iosSaberStaff_Smooth(s->rest) * iosSaberStaff_Smooth(s->active) * iosSaberStaff_Smooth(open);
    }
    if (what == 1) {
        float letGo = 0.0f;
        iosSaberStaff_LocalSpin(p, s->rest, &letGo);
        if (hilt == IOSSABER_HILT_STAFF) {
            const float grip = iosSaberStaff_Smooth(s->rest) * iosSaberStaff_Smooth(s->stand) * (1.0f - letGo);
            return (grip - (1.0f - grip)) * iosSaberStaff_Smooth(s->active);
        }
        return hilt == IOSSABER_HILT_FOLD ? -iosSaberStaff_Smooth(iosSaberFold_OpenFraction(p, iosSaberStaff_SaberInHand(p, 0))) * iosSaberStaff_Smooth(s->active) : 0.0f;
    }
    return iosSaberStaff_LocalSpin(p, s->rest, NULL);
}

// ------------------------------------------------------------ thrown

// The game's thrown saber's blade (its model's: the end at its hilt A, its tip B, the axis it is wider
// across), found once a model
static const rdModel3* iosSaberStaff_apThrownAxis[4];
static int iosSaberStaff_ThrownAxis(const rdModel3* m, rdVector3* pA, rdVector3* pB, int* pAcross)
{
    const rdModel3** apDone = iosSaberStaff_apThrownAxis;
    static rdVector3 aA[4], aB[4];
    static int aAcross[4], aOk[4], next = 0;
    for (int i = 0; i < 4; i++) {
        if (apDone[i] != m) continue;
        *pA = aA[i]; *pB = aB[i]; *pAcross = aAcross[i];
        return aOk[i];
    }
    // (its blade: Mysteries of the Sith's models' first material, Jedi Knight's second)
    const int k = Main_bMotsCompat ? 0 : 1;
    const rdMaterial* pBlade = (m->aMaterials && (int)m->sizeMaterials > k) ? m->aMaterials[k] : NULL;
    const int ok = pBlade && iosSaberShape_ThrownAxis(m, pBlade, pA, pB, pAcross);
    apDone[next] = m;
    aA[next] = *pA; aB[next] = *pB; aAcross[next] = *pAcross; aOk[next] = ok;
    next = (next + 1) % 4;
    return ok;
}

// One copy's placement (copy 0 as thrown; 1 the other half, open f): the model turned to lie flat,
// spinning round the world's up as the game spins it, its hilt's end (the pommel) where the thing is
static int iosSaberStaff_ThrownOrient(SithThing* pThing, int copy, float f, rdMatrix34* pIo)
{
    const rdModel3* m = pThing->renderData.model3;
    rdVector3 A, B;
    int across = 0;
    if (!iosSaberStaff_ThrownAxis(m, &A, &B, &across)) return 0;
    rdVector3 d = { B.x - A.x, B.y - A.y, B.z - A.z };
    const flex_t len = stdMath_Sqrt(iosSaberStaff_Dot(&d, &d));
    if (!iosSaberStaff_Unit(&d)) return 0;
    rdVector3 c = { across == 0, across == 1, across == 2 }, e;
    const flex_t dc = iosSaberStaff_Dot(&c, &d);
    c.x -= d.x * dc; c.y -= d.y * dc; c.z -= d.z * dc;
    if (!iosSaberStaff_Unit(&c)) return 0;
    iosSaberStaff_Cross(&e, &d, &c);
    // (the way it points now: as the game turns the thing, round the world's up)
    rdVector3 v = pThing->orient.lvec;
    if (v.x * v.x + v.y * v.y < (flex_t)0.01) v = pThing->orient.rvec;
    const float phi = atan2f((float)v.y, (float)v.x);
    const rdVector3 w1 = { cosf(phi), sinf(phi), 0 }, w2 = { -w1.y, w1.x, 0 }, w3 = { 0, 0, 1 };
    const rdVector3* ae[3] = { &d, &c, &e };
    const rdVector3* aw[3] = { &w1, &w2, &w3 };
    rdMatrix34 o;
    memset(&o, 0, sizeof(o));
    for (int i = 0; i < 3; i++) {
        rdVector_ScaleAdd3Acc(&o.rvec, aw[i], ae[i]->x);
        rdVector_ScaleAdd3Acc(&o.lvec, aw[i], ae[i]->y);
        rdVector_ScaleAdd3Acc(&o.uvec, aw[i], ae[i]->z);
    }
    // (its pommel: back along the hilt from the blade's base)
    const rdVector3 P = { A.x - d.x * len * IOSSABERSTAFF_THROWN_HILT, A.y - d.y * len * IOSSABERSTAFF_THROWN_HILT, A.z - d.z * len * IOSSABERSTAFF_THROWN_HILT };
    rdVector3 at = pIo->scale;
    if (copy) {
        // the other half: turned round the hinge (the world's up, beside the pommels) as far as it is open
        const flex_t g = len * IOSSABERSTAFF_THROWN_GAP;
        const float t = f * (float)M_PI;
        rdVector3 off = { w2.x * g * (flex_t)0.5, w2.y * g * (flex_t)0.5, 0 }, offT;
        iosSaberStaff_Turn(&offT, &off, &w3, t);
        at.x += off.x + offT.x;
        at.y += off.y + offT.y;
        at.z += off.z + offT.z;
        rdVector3 r;
        iosSaberStaff_Turn(&r, &o.rvec, &w3, t); o.rvec = r;
        iosSaberStaff_Turn(&r, &o.lvec, &w3, t); o.lvec = r;
        iosSaberStaff_Turn(&r, &o.uvec, &w3, t); o.uvec = r;
    }
    rdVector3 oP;
    rdMatrix_TransformVector34(&oP, &P, &o);
    o.scale.x = at.x - oP.x;
    o.scale.y = at.y - oP.y;
    o.scale.z = at.z - oP.z;
    *pIo = o;
    return 1;
}

void iosSaberStaff_FreeWorld(void)
{
    memset(iosSaberStaff_apMeasured, 0, sizeof(iosSaberStaff_apMeasured));
    memset(iosSaberStaff_apThrownAxis, 0, sizeof(iosSaberStaff_apThrownAxis));
    for (int i = 0; i < IOSSABERSTAFF_SLOTS; i++) {
        iosSaberStaff_aSlot[i].pThing = NULL;
        iosSaberStaff_aSlot[i].pModel = NULL;
    }
}

int iosSaberStaff_DrawThrown(SithThing* pThing, int* pRet)
{
    if (!pThing || pThing->type != SITH_THING_WEAPON || pThing->renderData.type != RD_THING_MODEL3 || !pThing->renderData.model3) return 0;
    const int saber = iosSaberThrow_ThrownHand(pThing);
    if (saber < 0) return 0;
    // (the hilt this saber flies with, as the throw kept it for this thing: the one saber's, from either hand)
    int hilt = iosSaberThrow_ThrownHilt(pThing);
    if (hilt < 0) hilt = iosSaberFold_ThrownHilt(saber);
    if (hilt < 0) hilt = iosSaberStaff_SaberHilt(sithPlayer_g_pLocalPlayerThing, saber);
    if (!iosSaberStaff_IsDouble(hilt)) return 0;
    const float f = hilt == IOSSABER_HILT_STAFF ? 1.0f : iosSaberFold_ThrownOpenFraction(saber);
    const rdMatrix34 saved = pThing->orient;
    int ret = 0;
    for (int copy = 0; copy < 2; copy++) {
        rdMatrix34 o = saved;
        if (!iosSaberStaff_ThrownOrient(pThing, copy, f, &o)) {
            pThing->orient = saved;
            if (!copy) return 0;
            break;
        }
        pThing->orient = o;
        pThing->renderData.rdFrameNum = 0; // (its joints built again, at this place)
        iosSaber_BeginThrownBlade(pThing, copy); // (each in its blade's colour)
        const int r = rdThing_Draw(&pThing->renderData, &pThing->orient);
        if (!copy) ret = r;
        iosSaber_EndThrownBlade(pThing, copy);
    }
    pThing->orient = saved;
    *pRet = ret;
    return 1;
}

#endif // JK_ENHANCED
