// Added: entire file. See iosSaberStance.h.

#ifdef JK_ENHANCED

#include "Enhanced/iosSaberStance.h"

#include <math.h>
#include <string.h>

#include "Enhanced/iosSaber.h"
#include "Enhanced/iosStance.h"
#include "Enhanced/iosSelfie.h"
#include "Enhanced/iosPhotoCam.h"
#include "Engine/sithCamera.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/jkSaber.h"
#include "Gameplay/sithTime.h"
#include "Engine/rdroid.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdModel3.h"
#include "Primitives/rdVector.h"
#include "General/stdString.h"
#include "World/jkPlayer.h"
#include "World/sithWeapon.h"
#include "jk.h"

// ------------------------------------------------------------ timing

// Out of the guard this quickly as a swing begins (well before its hits
// count: the game's swings count them from a sixth of a second in, after their
// first marker), and back in this slowly after (seconds); in and out of a
// crouch this slowly
#define IOSSABERSTANCE_OUT_SEC 0.10f
#define IOSSABERSTANCE_IN_SEC 0.40f
#define IOSSABERSTANCE_CROUCH_SEC 0.25f
// (the two-saber style's left arm swings this long after the right, iosSaber: it
// comes back into the guard as much later)
#define IOSSABERSTANCE_ECHO_MS IOSSABER_ECHO_MS
// (a gap in the game's clock this long or more, or the clock gone back: from the
// game's own pose again)
#define IOSSABERSTANCE_GAP_MS 500

// ------------------------------------------------------------ the guard

// One arm's guard, for the right arm (the left's mirrored), in the body's own
// frame: the upper arm from hanging straight down, out to the side and
// forward (degrees); the elbow, the forearm bent on forward from the upper arm;
// the blade from straight ahead, out to the side and down (no blade: the free
// hand, its hand left as the arm carries it)
typedef struct {
    float out, fwd, bend;
    float bladeOut, bladeDown;
    int bBlade;
} iosSaberStanceArm;
enum { IOSSABERSTANCE_ONE = 0, IOSSABERSTANCE_FREE, IOSSABERSTANCE_TWO, IOSSABERSTANCE_NUM_ARMS };
// standing (walking, running, jumping), then crouched: the blade more level,
// its point clear of the floor
static const iosSaberStanceArm iosSaberStance_aArm[2][IOSSABERSTANCE_NUM_ARMS] = {
    { [IOSSABERSTANCE_ONE]  = { 18.0f, 12.0f, 35.0f, 75.0f, 36.0f, 1 },
      [IOSSABERSTANCE_FREE] = {  8.0f,  2.0f, 18.0f,  0.0f,  0.0f, 0 },
      [IOSSABERSTANCE_TWO]  = { 22.0f, 12.0f, 35.0f, 58.0f, 28.0f, 1 } },
    { [IOSSABERSTANCE_ONE]  = { 16.0f, 26.0f, 30.0f, 46.0f, 10.0f, 1 },
      [IOSSABERSTANCE_FREE] = { 10.0f, 12.0f, 22.0f,  0.0f,  0.0f, 0 },
      [IOSSABERSTANCE_TWO]  = { 24.0f, 26.0f, 30.0f, 54.0f, 10.0f, 1 } },
};

// First person: the arm in view turned round its hand, its blade tipped out
// (roll, degrees) and back (pitch, degrees: + forward), the hand moved out,
// down and forward (model units); one saber, or each of two
typedef struct {
    float roll, pitch, side, lower, fwd;
} iosSaberStancePov;
static const iosSaberStancePov iosSaberStance_aPov[2] = {
    { 32.0f, 12.0f, 0.004f, 0.0f, 0.0f },
    { 22.0f, 10.0f, 0.010f, -0.003f, 0.0f },
};

// ------------------------------------------------------------ the weights

static float iosSaberStance_Smooth(float p)
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p * p * (3.0f - 2.0f * p);
}

// How far into the guard each hand is (0..1, before easing), moved on by the game's
// clock once per moment: out while bBusy, in after (a two-saber left arm a moment later)
typedef struct {
    int bTime;
    uint32_t lastMs;
    int32_t leftBusyUntil;
    float aP[2];
} iosSaberStanceBlend;

static void iosSaberStance_Blend(iosSaberStanceBlend* b, int bBusy, int bEcho, uint32_t now)
{
    if (!b->bTime || now < b->lastMs || now - b->lastMs > IOSSABERSTANCE_GAP_MS) {
        b->bTime = 1;
        b->lastMs = now;
        b->aP[0] = b->aP[1] = 0.0f;
        b->leftBusyUntil = (int32_t)now;
        return;
    }
    if (now == b->lastMs) return; // (drawn again in the same moment, or the game holds still)
    const float dt = (float)(now - b->lastMs) * 0.001f;
    b->lastMs = now;
    if (bBusy) b->leftBusyUntil = (int32_t)(now + IOSSABERSTANCE_ECHO_MS);
    for (int hand = 0; hand < 2; hand++) {
        const int busy = bBusy || (hand == 1 && bEcho && (int32_t)(now - (uint32_t)b->leftBusyUntil) < 0);
        float p = b->aP[hand] + (busy ? -dt / IOSSABERSTANCE_OUT_SEC : dt / IOSSABERSTANCE_IN_SEC);
        b->aP[hand] = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }
}

// The animations a guard is held through: standing, walking, running, turning,
// crouching, jumping (the game's "rising"), falling and landing, and fidgeting (anything else the game
// plays on the body is its own: swings, blocks, a hit taken, Force powers, the
// saber drawn or put away)
static const int iosSaberStance_aCarry[] = {
    SITH_ANIM_STAND, SITH_ANIM_WALK, SITH_ANIM_RUN, SITH_ANIM_WALKBACK, SITH_ANIM_STRAFELEFT, SITH_ANIM_STRAFERIGHT,
    SITH_ANIM_TURNLEFT, SITH_ANIM_TURNRIGHT, SITH_ANIM_CROUCHFORWARD, SITH_ANIM_CROUCHBACK,
    SITH_ANIM_RISING, SITH_ANIM_JUMP, SITH_ANIM_LEAP, SITH_ANIM_FALL, SITH_ANIM_LAND, SITH_ANIM_FIDGET, SITH_ANIM_FIDGET2,
};

static int iosSaberStance_IsCarry(const SithThing* t, const rdKeyframe* k)
{
    const SithPuppetClass* pc = t->pPuppetClass;
    if (!pc) return 0;
    for (int m = 0; m < (int)(sizeof(pc->modes) / sizeof(pc->modes[0])); m++)
        for (int i = 0; i < (int)(sizeof(iosSaberStance_aCarry) / sizeof(iosSaberStance_aCarry[0])); i++)
            if (pc->modes[m].keyframe[iosSaberStance_aCarry[i]].keyframe == k) return 1;
    return 0;
}

// (third person: the saber not lit and held, or anything but carrying it playing)
static int iosSaberStance_IsBusy(const SithThing* t)
{
    if (!(t->jkFlags & JKFLAG_SABERON) || (t->jkFlags & (JKFLAG_SABERDAMAGE | JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT))) return 1;
    if (!t->playerInfo || !t->playerInfo->rd_thing.model3) return 1; // (no hilt in the hand)
    const rdPuppet* pup = t->renderData.puppet;
    if (!pup) return 0;
    for (int i = 0; i < 4; i++) {
        const rdPuppetTrack* tr = &pup->aTracks[i];
        // (one fading out at its end: the game easing back to its stand, and the guard with it)
        if (!tr->status || !tr->keyframe || (tr->status & 8) || tr->playSpeed <= 0.01f) continue;
        if (!iosSaberStance_IsCarry(t, tr->keyframe)) return 1;
    }
    return 0;
}

// ------------------------------------------------------------ third person

// Each low guard's (the player's own, and in multiplayer others'): its arms and weights
#define IOSSABERSTANCE_SLOTS 8
#define IOSSABERSTANCE_MAX_NODES 64
enum { IOSSABERSTANCE_SHOULDER = 0, IOSSABERSTANCE_FOREARM, IOSSABERSTANCE_HAND, IOSSABERSTANCE_NUM_JOINTS };
typedef struct {
    SithThing* pThing;
    uint32_t signature; // (the thing's: another one in its place starts over)
    uint32_t usedMs;
    iosSaberStanceBlend blend;
    float crouch;
    rdModel3* pModel;
    int bArms;
    int aJoint[2][IOSSABERSTANCE_NUM_JOINTS]; // (right, left)
    uint64_t aCarries[2][IOSSABERSTANCE_NUM_JOINTS]; // (the nodes each joint carries, itself among them)
} iosSaberStanceSlot;
static iosSaberStanceSlot iosSaberStance_aSlot[IOSSABERSTANCE_SLOTS];

static iosSaberStanceSlot* iosSaberStance_Find(const SithThing* pThing)
{
    for (int i = 0; i < IOSSABERSTANCE_SLOTS; i++)
        if (iosSaberStance_aSlot[i].pThing == pThing && iosSaberStance_aSlot[i].signature == pThing->signature) return &iosSaberStance_aSlot[i];
    return NULL;
}

// A thing's slot: its own, or the one used longest ago (started over)
static iosSaberStanceSlot* iosSaberStance_SlotOf(SithThing* pThing, uint32_t now)
{
    iosSaberStanceSlot* s = iosSaberStance_Find(pThing);
    if (s) { s->usedMs = now; return s; }
    s = &iosSaberStance_aSlot[0];
    for (int i = 0; i < IOSSABERSTANCE_SLOTS; i++) {
        iosSaberStanceSlot* a = &iosSaberStance_aSlot[i];
        if (!a->pThing || (s->pThing && (int32_t)(a->usedMs - s->usedMs) < 0)) s = a;
    }
    memset(s, 0, sizeof(*s));
    s->pThing = pThing;
    s->signature = pThing->signature;
    s->usedMs = now;
    return s;
}

// (a node by the end of its name: "...rshouldr", "...rforearm", "...rhand" and the same with "l")
static int iosSaberStance_Node(const rdModel3* m, char side, const char* const* aSuffix)
{
    for (int i = 0; i < (int)m->numHNodes; i++) {
        const char* name = m->aHierarchyNodes[i].name;
        const int l = (int)strlen(name);
        for (int k = 0; aSuffix[k]; k++) {
            char want[32];
            snprintf(want, sizeof(want), "%c%s", side, aSuffix[k]);
            const int n = (int)strlen(want);
            if (l >= n && !__strcmpi(name + l - n, want)) return i;
        }
    }
    return -1;
}

static void iosSaberStance_FindArms(iosSaberStanceSlot* s, rdModel3* m)
{
    static const char* const aShoulder[] = { "shouldr", "shoulder", NULL };
    static const char* const aForearm[] = { "forearm", NULL };
    static const char* const aHand[] = { "hand", NULL };
    const char* const* aaName[IOSSABERSTANCE_NUM_JOINTS] = { aShoulder, aForearm, aHand };
    s->pModel = m;
    s->bArms = m->numHNodes <= IOSSABERSTANCE_MAX_NODES;
    for (int side = 0; s->bArms && side < 2; side++) {
        for (int j = 0; j < IOSSABERSTANCE_NUM_JOINTS; j++) {
            const int n = iosSaberStance_Node(m, side ? 'l' : 'r', aaName[j]);
            s->aJoint[side][j] = n;
            if (n < 0) { s->bArms = 0; break; }
            uint64_t carries = 0;
            for (int i = 0; i < (int)m->numHNodes; i++)
                for (const rdHierarchyNode* p = &m->aHierarchyNodes[i]; p; p = p->parent)
                    if (p == &m->aHierarchyNodes[n]) { carries |= (uint64_t)1 << i; break; }
            s->aCarries[side][j] = carries;
        }
        // (each joint on from the one before: the arm as the guard turns it)
        for (int j = 1; s->bArms && j < IOSSABERSTANCE_NUM_JOINTS; j++)
            if (!(s->aCarries[side][j - 1] & ((uint64_t)1 << s->aJoint[side][j]))) s->bArms = 0;
    }
}

// Where a joint turns (world): its matrix places the model's part, offset from it by the node's pivot
static void iosSaberStance_JointAt(rdVector3* out, const rdThing* r, int node)
{
    const rdMatrix34* w = &r->paJointMatrices[node];
    rdVector3 off;
    rdMatrix_TransformVector34(&off, &r->model3->aHierarchyNodes[node].pivot, w);
    rdVector_Sub3(out, &w->scale, &off);
}

static int iosSaberStance_Unit(rdVector3* v)
{
    const flex_t m = rdVector_Len3(v);
    if (m < (flex_t)1e-6) return 0;
    rdVector_Scale3Acc(v, 1.0f / m);
    return 1;
}

// The nodes `carries` turned round the point at, so that direction a (unit) turns w of the
// way to b (unit)
static void iosSaberStance_Turn(rdThing* r, uint64_t carries, const rdVector3* at, const rdVector3* a, const rdVector3* b, float w)
{
    rdVector3 axis;
    rdVector_Cross3(&axis, a, b);
    const flex_t s = rdVector_Len3(&axis), c = rdVector_Dot3(a, b);
    if (s < (flex_t)1e-5) return; // (already; or straight opposite, left)
    rdVector_Scale3Acc(&axis, 1.0f / s);
    const float ang = atan2f((float)s, (float)c) * w;
    const flex_t cs = cosf(ang), sn = sinf(ang), k = 1 - cs, x = axis.x, y = axis.y, z = axis.z;
    rdMatrix34 t;
    t.rvec = (rdVector3){ cs + x * x * k,     y * x * k + z * sn, z * x * k - y * sn };
    t.lvec = (rdVector3){ x * y * k - z * sn, cs + y * y * k,     z * y * k + x * sn };
    t.uvec = (rdVector3){ x * z * k + y * sn, y * z * k - x * sn, cs + z * z * k };
    rdVector_Zero3(&t.scale);
    for (int i = 0; i < (int)r->model3->numHNodes; i++) {
        if (!(carries & ((uint64_t)1 << i))) continue;
        rdMatrix34* m = &r->paJointMatrices[i];
        rdVector3 v, d;
        rdMatrix_TransformVector34(&v, &m->rvec, &t); m->rvec = v;
        rdMatrix_TransformVector34(&v, &m->lvec, &t); m->lvec = v;
        rdMatrix_TransformVector34(&v, &m->uvec, &t); m->uvec = v;
        rdVector_Sub3(&d, &m->scale, at);
        rdMatrix_TransformVector34(&v, &d, &t);
        rdVector_Add3(&m->scale, at, &v);
    }
}

// A direction in the body's frame (x: its right, y: forward, z: up; the left arm's x the other way) in the world
static void iosSaberStance_Body(rdVector3* out, const SithThing* t, float x, float y, float z, int side)
{
    if (side) x = -x;
    rdVector_Scale3(out, &t->orient.rvec, x);
    rdVector_ScaleAdd3Acc(out, &t->orient.lvec, y);
    rdVector_ScaleAdd3Acc(out, &t->orient.uvec, z);
    iosSaberStance_Unit(out);
}

static float iosSaberStance_Lerp(float a, float b, float k)
{
    return a + (b - a) * k;
}

// One arm into its guard, w of the way: the upper arm, then the forearm, then the blade
static void iosSaberStance_PoseArm(rdThing* r, const SithThing* t, const iosSaberStanceSlot* s, int side, int weapon, int kind, float w)
{
    const iosSaberStanceArm* a0 = &iosSaberStance_aArm[0][kind];
    const iosSaberStanceArm* a1 = &iosSaberStance_aArm[1][kind];
    const float c = s->crouch, d2r = (float)M_PI / 180.0f;
    const float out = iosSaberStance_Lerp(a0->out, a1->out, c) * d2r, fwd = iosSaberStance_Lerp(a0->fwd, a1->fwd, c) * d2r;
    const float bend = iosSaberStance_Lerp(a0->bend, a1->bend, c) * d2r;
    const int* j = s->aJoint[side];
    rdVector3 from, to, now, want;

    // the upper arm: down, out and forward
    iosSaberStance_Body(&want, t, tanf(out), tanf(fwd), -1.0f, side);
    iosSaberStance_JointAt(&from, r, j[IOSSABERSTANCE_SHOULDER]);
    iosSaberStance_JointAt(&to, r, j[IOSSABERSTANCE_FOREARM]);
    rdVector_Sub3(&now, &to, &from);
    if (!iosSaberStance_Unit(&now)) return;
    iosSaberStance_Turn(r, s->aCarries[side][IOSSABERSTANCE_SHOULDER], &from, &now, &want, w);

    // the forearm: on from it, bent forward at the elbow (in the plane of the upper arm and ahead)
    rdVector3 upper = want, ahead, fore;
    iosSaberStance_Body(&ahead, t, 0.0f, 1.0f, 0.0f, side);
    rdVector_ScaleAdd3Acc(&ahead, &upper, -rdVector_Dot3(&ahead, &upper));
    if (!iosSaberStance_Unit(&ahead)) return;
    rdVector_Scale3(&fore, &upper, cosf(bend));
    rdVector_ScaleAdd3Acc(&fore, &ahead, sinf(bend));
    iosSaberStance_JointAt(&from, r, j[IOSSABERSTANCE_FOREARM]);
    iosSaberStance_JointAt(&to, r, j[IOSSABERSTANCE_HAND]);
    rdVector_Sub3(&now, &to, &from);
    if (!iosSaberStance_Unit(&now)) return;
    iosSaberStance_Turn(r, s->aCarries[side][IOSSABERSTANCE_FOREARM], &from, &now, &fore, w);

    // the blade: ahead and out to the side, down (turned at the wrist)
    if (!a0->bBlade || weapon < 0 || !(s->aCarries[side][IOSSABERSTANCE_HAND] & ((uint64_t)1 << weapon))) return;
    const float bo = iosSaberStance_Lerp(a0->bladeOut, a1->bladeOut, c) * d2r, bd = iosSaberStance_Lerp(a0->bladeDown, a1->bladeDown, c) * d2r;
    iosSaberStance_Body(&want, t, sinf(bo) * cosf(bd), cosf(bo) * cosf(bd), -sinf(bd), side);
    now = r->paJointMatrices[weapon].lvec;
    if (!iosSaberStance_Unit(&now)) return;
    iosSaberStance_JointAt(&from, r, j[IOSSABERSTANCE_HAND]);
    iosSaberStance_Turn(r, s->aCarries[side][IOSSABERSTANCE_HAND], &from, &now, &want, w);
}

void iosSaberStance_PoseArms(rdThing* pRdThing)
{
    SithThing* t = pRdThing ? pRdThing->pThing : NULL;
    // (a player's body: only players have a style of their own)
    if (!t || t->type != SITH_THING_PLAYER || pRdThing != &t->renderData || !pRdThing->model3 || !pRdThing->paJointMatrices || !t->pPuppetClass) return;
    const int bRight = iosSaber_HandHilt(t, 0) == IOSSABER_HILT_CROSS, bLeft = iosSaber_HandHilt(t, 1) == IOSSABER_HILT_CROSS;
    if (!bRight && !bLeft) return;
    // (a selfie poses the arms itself)
    if (iosSelfie_IsActive()) return;
    const uint32_t now = sithTime_g_msecGameTime;
    iosSaberStanceSlot* s = iosSaberStance_SlotOf(t, now);
    if (pRdThing->model3 != s->pModel) iosSaberStance_FindArms(s, pRdThing->model3);
    if (!s->bArms) return;
    const int bTwo = iosSaber_HasSecondBlade(t);
    // (crouched or not, eased by the same clock; then the guard's weights)
    const float crouched = (t->physicsParams.flags & SITH_PF_CROUCHING) ? 1.0f : 0.0f;
    if (!s->blend.bTime || now < s->blend.lastMs || now - s->blend.lastMs > IOSSABERSTANCE_GAP_MS) s->crouch = crouched;
    else if (now != s->blend.lastMs) {
        const float step = (float)(now - s->blend.lastMs) * 0.001f / IOSSABERSTANCE_CROUCH_SEC;
        s->crouch = s->crouch < crouched ? fminf(crouched, s->crouch + step) : fmaxf(crouched, s->crouch - step);
    }
    iosSaberStance_Blend(&s->blend, iosSaberStance_IsBusy(t), bTwo, now);
    // (the game counting hits: its own pose, exactly)
    if (t->jkFlags & JKFLAG_SABERDAMAGE) s->blend.aP[0] = s->blend.aP[1] = 0.0f;
    // (the player's own body unseen, in first person: left as the game poses it, so the saber's light,
    // which the game puts at this body's blade, stays near the blade in view, as it always has)
    if (t == sithPlayer_g_pLocalPlayerThing && sithCamera_g_pCurCamera == &sithCamera_g_aCameras[0] && !iosPhotoCam_IsActive()) return;

    const int primary = t->pPuppetClass->aJoints[JOINTTYPE_PRIMARYWEAP];
    for (int side = 0; side < 2; side++) {
        // the right arm with its saber; the left with its own (two sabers), or free (one)
        int kind, weapon = -1;
        if (side == 0) {
            if (!bRight) continue;
            kind = bTwo ? IOSSABERSTANCE_TWO : IOSSABERSTANCE_ONE;
            weapon = primary;
        } else if (bTwo) {
            if (!bLeft) continue;
            kind = IOSSABERSTANCE_TWO;
            weapon = jkSaber_SecondaryJoint(t);
        } else {
            if (!bRight) continue;
            kind = IOSSABERSTANCE_FREE;
        }
        const float w = iosSaberStance_Smooth(s->blend.aP[side]);
        if (w <= 0.0f) continue;
        iosSaberStance_PoseArm(pRdThing, t, s, side, weapon, kind, w);
    }
}


// ------------------------------------------------------------ first person

#define IOSSABERSTANCE_POV_ECHO_FRAMES 48
static struct {
    iosSaberStanceBlend blend;
    // (the left arm's, its swings the right's of a moment before: busy as the right was then)
    iosSaberStanceBlend left;
    struct { uint32_t ms; int bBusy; } aBusy[IOSSABERSTANCE_POV_ECHO_FRAMES];
    int busyHead, numBusy;
    rdModel3* pModel;
    int rhand;
    rdMatrix34 lastPlace; // (the right arm's, as last drawn)
    int bLastPlace;
    uint32_t lastFrame;
} iosSaberStance_pov;

int iosSaberStance_PovPlace(SithThing* pActor, rdThing* pPov, int hand, const rdVector3* pPivot, rdMatrix34* pPlace)
{
    if (!pActor || !pPov || !pPov->model3 || !pPlace || hand < 0 || hand > 1) return 0;
    if (iosSaber_HandHilt(pActor, hand) != IOSSABER_HILT_CROSS) {
        if (!hand) iosSaberStance_pov.bLastPlace = 0;
        return 0;
    }
    const int bTwo = iosSaber_HasSecondBlade(pActor);
    rdVector3 pivot;
    if (!hand) {
        // (once a frame, with the right arm: how far into the guard; its hand as it was last drawn)
        const uint32_t now = sithTime_g_msecGameTime;
        const int bBusy = !(pActor->jkFlags & JKFLAG_SABERON) || iosStance_IsPovBusy(pPov->puppet) || sithTime_g_secGameTime < sithWeapon_secMountWait;
        iosSaberStance_Blend(&iosSaberStance_pov.blend, bBusy, 0, now);
        // (the right arm's of IOSSABERSTANCE_ECHO_MS before, or the oldest kept, for the left)
        if (!iosSaberStance_pov.numBusy || iosSaberStance_pov.aBusy[iosSaberStance_pov.busyHead].ms != now) {
            iosSaberStance_pov.busyHead = (iosSaberStance_pov.busyHead + 1) % IOSSABERSTANCE_POV_ECHO_FRAMES;
            if (iosSaberStance_pov.numBusy < IOSSABERSTANCE_POV_ECHO_FRAMES) iosSaberStance_pov.numBusy++;
            iosSaberStance_pov.aBusy[iosSaberStance_pov.busyHead].ms = now;
        }
        iosSaberStance_pov.aBusy[iosSaberStance_pov.busyHead].bBusy = bBusy;
        int bPast = bBusy;
        for (int k = 0; k < iosSaberStance_pov.numBusy; k++) {
            const int i = (iosSaberStance_pov.busyHead - k + IOSSABERSTANCE_POV_ECHO_FRAMES) % IOSSABERSTANCE_POV_ECHO_FRAMES;
            if (iosSaberStance_pov.aBusy[i].ms > now) break; // (the clock gone back: a load)
            bPast = iosSaberStance_pov.aBusy[i].bBusy;
            if (now - iosSaberStance_pov.aBusy[i].ms >= IOSSABERSTANCE_ECHO_MS) break;
        }
        iosSaberStance_Blend(&iosSaberStance_pov.left, bPast, 0, now);
        if (iosSaberStance_pov.pModel != pPov->model3) {
            iosSaberStance_pov.pModel = pPov->model3;
            iosSaberStance_pov.rhand = -1;
            iosSaberStance_pov.bLastPlace = 0;
            for (int i = 0; i < (int)pPov->model3->numHNodes; i++) {
                const char* name = pPov->model3->aHierarchyNodes[i].name;
                const int l = (int)strlen(name);
                if (l >= 5 && !__strcmpi(name + l - 5, "rhand")) { iosSaberStance_pov.rhand = i; break; }
            }
        }
        const int ok = iosSaberStance_pov.bLastPlace && iosSaberStance_pov.rhand >= 0 && pPov->paJointMatrices
                       && pPov->rdFrameNum == iosSaberStance_pov.lastFrame;
        if (ok) {
            rdMatrix34 inv;
            rdMatrix_InvertOrtho34(&inv, &iosSaberStance_pov.lastPlace);
            rdMatrix_TransformPoint34(&pivot, &pPov->paJointMatrices[iosSaberStance_pov.rhand].scale, &inv);
        }
        iosSaberStance_pov.bLastPlace = 0;
        if (!ok) {
            // (none yet: as the game places it, the hand found once drawn)
            iosSaberStance_pov.lastPlace = *pPlace;
            iosSaberStance_pov.bLastPlace = 1;
            iosSaberStance_pov.lastFrame = rdroid_frameTrue;
            return 0;
        }
    } else {
        if (!pPivot) return 0;
        pivot = *pPivot;
    }
    const float w = iosSaberStance_Smooth(hand ? iosSaberStance_pov.left.aP[0] : iosSaberStance_pov.blend.aP[0]);
    rdMatrix34 out = *pPlace;
    if (w > 0.0f) {
        const iosSaberStancePov* g = &iosSaberStance_aPov[bTwo ? 1 : 0];
        rdVector3 pyr = { w * g->pitch, 0.0f, w * g->roll };
        rdMatrix34 m;
        rdMatrix_BuildRotate34(&m, &pyr);
        // turned round the hand, then moved
        rdVector3 rp;
        rdMatrix_TransformVector34(&rp, &pivot, &m);
        m.scale.x = pivot.x - rp.x + w * g->side;
        m.scale.y = pivot.y - rp.y + w * g->fwd;
        m.scale.z = pivot.z - rp.z - w * g->lower;
        rdMatrix_Multiply34(&out, pPlace, &m);
    }
    if (!hand) {
        iosSaberStance_pov.lastPlace = out;
        iosSaberStance_pov.bLastPlace = 1;
        iosSaberStance_pov.lastFrame = rdroid_frameTrue;
    }
    if (w <= 0.0f) return 0;
    *pPlace = out;
    return 1;
}

float iosSaberStance_Weight(SithThing* pThing, int hand)
{
    if (hand == 2 || hand == 3) return iosSaberStance_Smooth(hand == 3 ? iosSaberStance_pov.left.aP[0] : iosSaberStance_pov.blend.aP[0]);
    const iosSaberStanceSlot* s = iosSaberStance_Find(pThing);
    if (!s || hand < 0 || hand > 1) return 0.0f;
    return iosSaberStance_Smooth(s->blend.aP[hand]);
}

#endif // JK_ENHANCED
