// Added: entire file. See iosSaberFold.h.

#ifdef JK_ENHANCED

#include "Enhanced/iosSaberFold.h"

#include <string.h>

#include "Enhanced/iosGame.h"
#include "Enhanced/iosSaber.h"
#include "Enhanced/iosSaberStaff.h"
#include "Enhanced/iosSaberThrow.h"
#include "Devices/sithSound.h"
#include "Devices/sithSoundMixer.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithTime.h"
#include "Main/Main.h"
#include "World/jkPlayer.h"
#include "World/sithThing.h"
#include "World/sithWorld.h"
#include "jk.h"
#include "Dss/sithMulti.h"
#include "Main/jkPlayerLook.h"

// The click (the game's own: the thermal detonator's arming click, in both games)
#define IOSSABERFOLD_CLICK "ThermClick01.wav"
#define IOSSABERFOLD_CLICK_VOLUME 0.8f
// (a saber out on a throw longer than this is gone: lit again later, it is lit afresh)
#define IOSSABERFOLD_OUT_MAX_MS 12000
// (Mysteries of the Sith's Saber Throw power: on while its own throw is out)
#define IOSSABERFOLD_MOTS_THROW_BIN 37
// (a gap in the game's clock this long or more, or the clock gone back: as it is, no snap)
#define IOSSABERFOLD_GAP_MS 1000

// ------------------------------------------------------------ the snap

// How far open (0..1): from `from` toward `target`, eased, over the snap's time
// (shorter from part way)
typedef struct {
    float from;
    int target;
    uint32_t startMs;
    int bMoving;
} iosSaberFoldSnap;

static float iosSaberFold_Ease(float p)
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p * p * (3.0f - 2.0f * p);
}

static float iosSaberFold_SnapNow(const iosSaberFoldSnap* s, uint32_t now)
{
    if (!s->bMoving) return s->target ? 1.0f : 0.0f;
    const float dist = s->target ? 1.0f - s->from : s->from;
    const float ms = (float)IOSSABERFOLD_SNAP_MS * (dist > 0.05f ? dist : 0.05f);
    const float p = (now >= s->startMs) ? (float)(now - s->startMs) / ms : 0.0f;
    if (p >= 1.0f || now - s->startMs > IOSSABERFOLD_GAP_MS * 10u) return s->target ? 1.0f : 0.0f;
    const float e = iosSaberFold_Ease(p);
    return s->from + ((s->target ? 1.0f : 0.0f) - s->from) * e;
}

// (toward target from where it is now; 1 if it moves)
static int iosSaberFold_SnapTo(iosSaberFoldSnap* s, int target, uint32_t now)
{
    const float f = iosSaberFold_SnapNow(s, now);
    target = target ? 1 : 0;
    if (s->target == target && (!s->bMoving || f == (float)target)) return 0;
    s->from = f;
    s->target = target;
    s->startMs = now;
    s->bMoving = f != (float)target;
    return s->bMoving;
}

static void iosSaberFold_SnapSet(iosSaberFoldSnap* s, int target)
{
    s->target = target ? 1 : 0;
    s->from = (float)s->target;
    s->bMoving = 0;
}

// The click, on a thing (its own channel, heard as the game's sounds round it are). The sound is
// looked up each time (the game's sounds by name; loaded if this level hasn't it): never kept from
// a level gone, whose sounds are freed
static void iosSaberFold_Click(SithThing* pThing)
{
    if (!pThing || !sithWorld_g_pCurrentWorld) return;
    sithSound* pSnd = sithSound_Load(IOSSABERFOLD_CLICK, 0);
    if (pSnd) sithSoundMixer_PlaySoundThing(pSnd, pThing, IOSSABERFOLD_CLICK_VOLUME, 0.5f, 2.5f, SITHSOUNDFLAG_FOLLOWSTHING);
}

// ------------------------------------------------------------ the local player's

typedef struct {
    int bOut;            // out on a throw (or caught, not lit again yet: bBack)
    int bSeenAway;       // (seen out of the hand since: back in it is the catch)
    int bBack;           // caught: in the hand again (drawn there, as caught), until it is lit
    int bByInventory;    // (Mysteries of the Sith's own throw, no hand of the throws': back once the saber is had again, the game's throw taking it while it flies)
    int hilt;            // the hilt it was thrown with
    int bOpenAtRelease;  // thrown open (else folded, snapping open as it flies)
    int bClicked;        // (its snap's click played)
    uint32_t releaseMs;
    int thingIdx;        // the saber flying (its click is played there)
    uint32_t thingSig;
} iosSaberFoldOut;

static struct {
    SithThing* pPlayer;
    uint32_t signature;
    int bAuto;
    // each saber's own ([0] the right hand's, [1] the left's)
    int aTarget[2];      // folded 0, open 1 (with AUTO off: as the player left it)
    int aHeldFolded[2];  // folded by hand: AUTO doesn't open it until IOSSABERFOLD_CALM_MS without a swing
    uint32_t aHeldMs[2];
    int aSwung[2];       // a swing since it opened: AUTO may fold it
    uint32_t lastSwingMs;
    int bKeyAgain;       // (the saber's key flipped the right hand's of two: pressed again soon, the left's)
    uint32_t keyMs;
    int bWasOn;          // a FOLD lit in hand, last tick
    int bWasThrowing;    // Mysteries of the Sith's own throw out, last tick
    int bWasDead;
    uint32_t lastMs;
    int bTime;
    iosSaberFoldSnap aSaber[2];
    iosSaberFoldOut aOut[2];
} iosSaberFold_s = { NULL, 0, 1 };

static SithThing* iosSaberFold_Player(void)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    if (!sithWorld_g_pCurrentWorld || !p || p->type != SITH_THING_PLAYER || !p->actorParams.pPlayer) return NULL;
    return p;
}

static int iosSaberFold_SaberBin(void)
{
    return Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER;
}

// (a FOLD among the sabers in its hands now)
static int iosSaberFold_HoldsFold(SithThing* p)
{
    if (iosSaberStaff_HiltOf(p, 0) == IOSSABER_HILT_FOLD) return 1;
    return iosSaber_HasSecondBlade(p) && iosSaberStaff_HiltOf(p, 1) == IOSSABER_HILT_FOLD;
}

// (a FOLD lit in hand)
static int iosSaberFold_LitFold(SithThing* p)
{
    return p && (p->jkFlags & JKFLAG_SABERON) && p->actorParams.pPlayer && p->actorParams.pPlayer->curWeaponID == iosSaberFold_SaberBin()
        && iosSaberFold_HoldsFold(p);
}

static uint32_t iosSaberFold_Now(void)
{
    return sithTime_g_msecGameTime;
}

// (a saber of the local player's out of the hand now: thrown, not caught yet)
static int iosSaberFold_Flying(int saber)
{
    return iosSaberFold_s.aOut[saber].bOut && !iosSaberFold_s.aOut[saber].bBack;
}

// (a saber of the player's a FOLD in hand: the one in the right hand, and the left one with two)
static int iosSaberFold_Shown(SithThing* p, int saber)
{
    return p && (saber == iosSaberStaff_SaberInHand(p, 0) || (saber == 1 && iosSaber_HasSecondBlade(p))) && iosSaberStaff_SaberHilt(p, saber) == IOSSABER_HILT_FOLD;
}

// (the FOLDs lit in hand now, by saber: bit 0 the right hand's, bit 1 the left's)
static int iosSaberFold_InHand(SithThing* p)
{
    int mask = 0;
    for (int k = 0; k < 2 && iosSaberFold_LitFold(p); k++)
        if (!iosSaberFold_Flying(k) && iosSaberFold_Shown(p, k)) mask |= 1 << k;
    return mask;
}

// Those sabers (by bit, as above; none out on a throw) toward target, each from where it is; 1 if a FOLD in
// hand moves (iosSaberFold_Clicked)
static int iosSaberFold_Target(int mask, int target)
{
    const uint32_t now = iosSaberFold_Now();
    SithThing* p = iosSaberFold_s.pPlayer;
    int bMoved = 0;
    for (int k = 0; k < 2; k++) {
        if (!(mask & (1 << k)) || iosSaberFold_Flying(k)) continue;
        iosSaberFold_s.aTarget[k] = target ? 1 : 0;
        if (iosSaberFold_SnapTo(&iosSaberFold_s.aSaber[k], iosSaberFold_s.aTarget[k], now) && iosSaberFold_Shown(p, k)) bMoved = 1;
    }
    return bMoved;
}

// (one click for whatever moved at once, a FOLD lit)
static void iosSaberFold_Clicked(int bMoved)
{
    if (bMoved && iosSaberFold_LitFold(iosSaberFold_s.pPlayer)) iosSaberFold_Click(iosSaberFold_s.pPlayer);
}

// Folded at once (AUTO on: lit afresh, a level, a load, a death); with AUTO off, each as the player left it
static void iosSaberFold_Fresh(void)
{
    for (int k = 0; k < 2; k++) {
        if (iosSaberFold_s.bAuto) iosSaberFold_s.aTarget[k] = 0;
        iosSaberFold_SnapSet(&iosSaberFold_s.aSaber[k], iosSaberFold_s.aTarget[k]);
        iosSaberFold_s.aHeldFolded[k] = 0;
        iosSaberFold_s.aSwung[k] = 0;
    }
}

void iosSaberFold_Reset(void)
{
    memset(iosSaberFold_s.aOut, 0, sizeof(iosSaberFold_s.aOut));
    iosSaberFold_s.bWasThrowing = 1; // (a game loaded with Mysteries of the Sith's throw power still on: not a throw now)
    iosSaberFold_s.bWasOn = 0;
    iosSaberFold_s.bTime = 0;
    iosSaberFold_s.pPlayer = NULL;
    iosSaberFold_Fresh();
}

int iosSaberFold_GetAuto(void)
{
    return iosSaberFold_s.bAuto;
}

void iosSaberFold_SetAuto(int bOn)
{
    iosSaberFold_s.bAuto = bOn ? 1 : 0;
    for (int k = 0; k < 2; k++) {
        iosSaberFold_s.aHeldFolded[k] = 0;
        iosSaberFold_s.aSwung[k] = 0; // (on: AUTO folds it after the next swing, not at once)
    }
}

int iosSaberFold_GetSaberHeldOpen(int saber)
{
    return (saber == 0 || saber == 1) && iosSaberFold_s.aTarget[saber];
}

void iosSaberFold_SetSaberHeldOpen(int saber, int bOpen)
{
    if (iosSaberFold_s.bAuto || saber < 0 || saber > 1) return;
    iosSaberFold_s.aTarget[saber] = bOpen ? 1 : 0;
    if (!iosSaberFold_Flying(saber)) iosSaberFold_SnapSet(&iosSaberFold_s.aSaber[saber], iosSaberFold_s.aTarget[saber]);
}

int iosSaberFold_GetHeldOpen(void)
{
    return iosSaberFold_GetSaberHeldOpen(0);
}

void iosSaberFold_SetHeldOpen(int bOpen)
{
    for (int k = 0; k < 2; k++) iosSaberFold_SetSaberHeldOpen(k, bOpen);
}

int iosSaberFold_CanFlip(void)
{
    SithThing* p = iosSaberFold_Player();
    return p && !(p->flags & SITH_TF_DEAD) && iosSaberFold_LitFold(p);
}

// Those FOLDs in hand (by saber bit) flipped, each from how it is; one click for them all
static int iosSaberFold_FlipSabers(int mask)
{
    if (!mask || !iosSaberFold_CanFlip()) return 0;
    const uint32_t now = iosSaberFold_Now();
    int fold = 0, open = 0;
    for (int k = 0; k < 2; k++) {
        if (!(mask & (1 << k))) continue;
        if (iosSaberFold_s.aTarget[k]) fold |= 1 << k;
        else open |= 1 << k;
        // (folded by hand: held so; opened by hand: open until a swing and the calm after)
        iosSaberFold_s.aHeldFolded[k] = iosSaberFold_s.aTarget[k];
        iosSaberFold_s.aHeldMs[k] = now;
        iosSaberFold_s.aSwung[k] = 0;
    }
    const int bMoved = iosSaberFold_Target(fold, 0);
    iosSaberFold_Clicked(iosSaberFold_Target(open, 1) || bMoved);
    return 1;
}

int iosSaberFold_Flip(void)
{
    SithThing* p = iosSaberFold_Player();
    return p && iosSaberFold_FlipSabers(iosSaberFold_InHand(p));
}

int iosSaberFold_TapFlip(int bAgain)
{
    SithThing* p = iosSaberFold_Player();
    const int mask = p ? iosSaberFold_InHand(p) : 0;
    if (mask != 3) return iosSaberFold_FlipSabers(mask); // (one FOLD in hand: every tap flips it)
    if (!iosSaberFold_FlipSabers(bAgain ? 2 : 1)) return 0;
    return bAgain ? 1 : 2;
}

int iosSaberFold_KeyFlip(void)
{
    const uint32_t now = iosSaberFold_Now();
    const int bAgain = iosSaberFold_s.bKeyAgain && now >= iosSaberFold_s.keyMs && now - iosSaberFold_s.keyMs < IOSSABERFOLD_AGAIN_MS;
    const int r = iosSaberFold_TapFlip(bAgain);
    iosSaberFold_s.bKeyAgain = r == 2;
    iosSaberFold_s.keyMs = now;
    return r != 0;
}

void iosSaberFold_Swing(SithThing* pThing)
{
    SithThing* p = iosSaberFold_Player();
    if (!p || pThing != p) return;
    iosSaberFold_s.lastSwingMs = iosSaberFold_Now();
    if (!iosSaberFold_LitFold(p)) return;
    // AUTO: each folded one in hand opens (both together), but one folded by hand (not one out of sight, as the
    // left one with one saber: a FOLD put there later lights folded)
    int open = 0;
    for (int k = 0; k < 2; k++) {
        if (iosSaberFold_Flying(k) || !iosSaberFold_Shown(p, k)) continue;
        if (iosSaberFold_s.aTarget[k]) iosSaberFold_s.aSwung[k] = 1;
        else if (iosSaberFold_s.bAuto && !iosSaberFold_s.aHeldFolded[k]) {
            open |= 1 << k;
            iosSaberFold_s.aSwung[k] = 1;
        }
    }
    iosSaberFold_Clicked(iosSaberFold_Target(open, 1));
}

// ------------------------------------------------------------ throws

static float iosSaberFold_OutFraction(const iosSaberFoldOut* o, uint32_t now)
{
    if (o->bOpenAtRelease) return 1.0f;
    const uint32_t at = o->releaseMs + IOSSABERFOLD_THROWN_SNAP_MS;
    if (now <= at) return 0.0f;
    return iosSaberFold_Ease((float)(now - at) / (float)IOSSABERFOLD_SNAP_MS);
}

void iosSaberFold_Released(int saber, SithThing* pSaber, int shown)
{
    SithThing* p = iosSaberFold_Player();
    if (!p || saber < 0 || saber > 1 || shown < 0 || shown > 1) return;
    const uint32_t now = iosSaberFold_Now();
    // (as it was shown in the hand: its hilt, and how far open)
    const int hilt = iosSaberStaff_SaberHilt(p, shown);
    const float open = iosSaberFold_OpenFraction(p, shown);
    iosSaberFoldOut* o = &iosSaberFold_s.aOut[saber];
    memset(o, 0, sizeof(*o));
    o->bOut = 1;
    o->hilt = hilt;
    o->bOpenAtRelease = open >= 0.5f;
    o->releaseMs = now;
    o->thingIdx = pSaber ? pSaber->idx : -1;
    o->thingSig = pSaber ? pSaber->signature : 0;
}

float iosSaberFold_ThrownOpenFraction(int saber)
{
    if (saber < 0 || saber > 1) return 1.0f;
    // (not known to be out yet, as it is drawn the moment it leaves the hand: as it was in the hand)
    if (!iosSaberFold_Flying(saber)) return iosSaberFold_SnapNow(&iosSaberFold_s.aSaber[saber], iosSaberFold_Now());
    return iosSaberFold_OutFraction(&iosSaberFold_s.aOut[saber], iosSaberFold_Now());
}

int iosSaberFold_ThrownHilt(int saber)
{
    if (saber < 0 || saber > 1 || !iosSaberFold_Flying(saber)) return -1;
    return iosSaberFold_s.aOut[saber].hilt;
}

static SithThing* iosSaberFold_OutThing(const iosSaberFoldOut* o)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    if (!w || o->thingIdx < 0 || o->thingIdx > w->numThings) return NULL;
    SithThing* t = &w->aThings[o->thingIdx];
    if (!t->type || t->signature != o->thingSig || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED))) return NULL;
    return t;
}

// Mysteries of the Sith's own throw (one saber, the game's script's): the thrown saber
static SithThing* iosSaberFold_FindThrown(SithThing* p)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    for (int i = 0; w && i <= w->numThings; i++) {
        SithThing* t = &w->aThings[i];
        if (t->type != SITH_THING_WEAPON || t->pParent != p || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED))) continue;
        if (iosSaberThrow_ThrownColour(t) >= 0) return t;
    }
    return NULL;
}

// A saber back in its hand (the moment it is, not once it is lit again: drawn in the hand from then on,
// as caught): AUTO on, open (as it flew), folding 5 s on without a swing; off, as the player left it
// (thrown folded: snapping shut, with the click; with another weapon in hand, and none going to the saber: shut, silent)
static void iosSaberFold_Caught(SithThing* p, int saber, uint32_t now)
{
    iosSaberFoldOut* o = &iosSaberFold_s.aOut[saber];
    const float f = iosSaberFold_OutFraction(o, now);
    o->bBack = 1;
    // (in the hand as it flew, part way through its snap open if caught that soon: the snaps on go from there)
    iosSaberFoldSnap* s = &iosSaberFold_s.aSaber[saber];
    s->from = f;
    s->target = f >= 0.5f;
    s->startMs = now;
    s->bMoving = f != (float)s->target;
    if (o->hilt != IOSSABER_HILT_FOLD) {
        iosSaberFold_SnapSet(s, iosSaberFold_s.aTarget[saber]);
        return;
    }
    if (iosSaberFold_s.bAuto) {
        // (as a swing: the other in hand opens with it, but one folded by hand)
        int open = 1 << saber;
        iosSaberFold_s.aHeldFolded[saber] = 0;
        for (int k = 0; k < 2; k++) {
            if (k != saber && iosSaberFold_Shown(p, k) && !iosSaberFold_Flying(k) && !iosSaberFold_s.aTarget[k] && !iosSaberFold_s.aHeldFolded[k]) open |= 1 << k;
            if (open & (1 << k)) iosSaberFold_s.aSwung[k] = 1;
        }
        iosSaberFold_s.lastSwingMs = now;
        iosSaberFold_Clicked(iosSaberFold_Target(open, 1));
    }
    else if (p->actorParams.pPlayer->curWeaponID == iosSaberFold_SaberBin() || iosGame_GetCurWeapon() == iosSaberFold_SaberBin()) {
        if (iosSaberFold_SnapTo(s, iosSaberFold_s.aTarget[saber], now)) iosSaberFold_Click(p);
    }
    else iosSaberFold_SnapSet(s, iosSaberFold_s.aTarget[saber]);
}

// ------------------------------------------------------------ once a frame

static void iosSaberFold_TickOthers(void); // (below)

void iosSaberFold_Tick(void)
{
    iosSaberFold_TickOthers();
    SithThing* p = iosSaberFold_Player();
    if (!p) return;
    const uint32_t now = iosSaberFold_Now();
    if (p != iosSaberFold_s.pPlayer || p->signature != iosSaberFold_s.signature) {
        // (another player: a new level, a game loaded)
        iosSaberFold_Reset();
        iosSaberFold_s.pPlayer = p;
        iosSaberFold_s.signature = p->signature;
    }
    if (iosSaberFold_s.bTime && (now < iosSaberFold_s.lastMs || now - iosSaberFold_s.lastMs > 60000u)) {
        // (the clock gone back, or far on: a game loaded)
        memset(iosSaberFold_s.aOut, 0, sizeof(iosSaberFold_s.aOut));
        iosSaberFold_s.bWasThrowing = 1;
        iosSaberFold_Fresh();
    }
    iosSaberFold_s.bTime = 1;
    iosSaberFold_s.lastMs = now;

    // a death: folded (AUTO on), nothing out
    const int bDead = (p->flags & SITH_TF_DEAD) || p->actorParams.health <= 0.0;
    if (bDead && !iosSaberFold_s.bWasDead) {
        memset(iosSaberFold_s.aOut, 0, sizeof(iosSaberFold_s.aOut));
        iosSaberFold_Fresh();
    }
    iosSaberFold_s.bWasDead = bDead;

    // Mysteries of the Sith's own throw (one saber): its power going on is the saber leaving the hand (on
    // already as a game is loaded or a level begun, it is not)
    const int bThrowing = Main_bMotsCompat && iosSaber_GetStyle() != IOSSABER_STYLE_DUAL && sithInventory_IsInventoryActivated(p, IOSSABERFOLD_MOTS_THROW_BIN);
    if (bThrowing && !iosSaberFold_s.bWasThrowing && !iosSaberFold_s.aOut[0].bOut) {
        iosSaberFold_Released(0, iosSaberFold_FindThrown(p), 0);
        iosSaberFold_s.aOut[0].bByInventory = 1;
    }
    iosSaberFold_s.bWasThrowing = bThrowing;

    // sabers out: away from the hand, then back in it (the catch: the throws' hands say so, or for
    // Mysteries of the Sith's own throw, the saber had again), then lit again (the throw over: not lit
    // afresh); the thrown snap's click
    const int mask = iosSaberThrow_HandsInHand();
    const int bHad = sithInventory_GetInventory(p, iosSaberFold_SaberBin()) > 0.0;
    const int bLit = (p->jkFlags & JKFLAG_SABERON) && p->actorParams.pPlayer->curWeaponID == iosSaberFold_SaberBin();
    int bCaught = 0;
    for (int k = 0; k < 2; k++) {
        iosSaberFoldOut* o = &iosSaberFold_s.aOut[k];
        if (!o->bOut) continue;
        if (now < o->releaseMs || now - o->releaseMs > IOSSABERFOLD_OUT_MAX_MS) { o->bOut = 0; continue; }
        if (o->bBack) {
            if (!bLit) continue;
            o->bOut = 0;
            bCaught = 1;
            if (iosSaberFold_s.bAuto && o->hilt == IOSSABER_HILT_FOLD) iosSaberFold_s.lastSwingMs = now; // (AUTO's 5 s from when it is lit in hand)
            continue;
        }
        if (!(o->bByInventory ? bHad : (mask & (1 << k)))) o->bSeenAway = 1;
        else if (o->bSeenAway) {
            iosSaberFold_Caught(p, k, now);
            if (bLit) o->bOut = 0;
            bCaught = 1;
            continue;
        }
        if (o->hilt == IOSSABER_HILT_FOLD && !o->bOpenAtRelease && !o->bClicked && now >= o->releaseMs + IOSSABERFOLD_THROWN_SNAP_MS) {
            o->bClicked = 1;
            iosSaberFold_Click(iosSaberFold_OutThing(o));
        }
    }

    // lit afresh (not caught): folded with AUTO on
    const int bOn = iosSaberFold_LitFold(p);
    if (bOn && !iosSaberFold_s.bWasOn && !bCaught && !iosSaberFold_s.aOut[0].bOut && !iosSaberFold_s.aOut[1].bOut) iosSaberFold_Fresh();
    iosSaberFold_s.bWasOn = bOn;
    if (!bOn) return;

    // each: folded by hand, AUTO opens it again once 5 s have gone by without a swing; AUTO, open, swung,
    // and 5 s without a swing (nor anything else going on): folded (both together)
    int fold = 0;
    for (int k = 0; k < 2; k++) {
        if (iosSaberFold_Flying(k)) continue;
        const uint32_t calmFrom = iosSaberFold_s.aHeldFolded[k] && (int32_t)(iosSaberFold_s.aHeldMs[k] - iosSaberFold_s.lastSwingMs) > 0
                                  ? iosSaberFold_s.aHeldMs[k] : iosSaberFold_s.lastSwingMs;
        if (iosSaberFold_s.aHeldFolded[k] && now - calmFrom >= IOSSABERFOLD_CALM_MS) iosSaberFold_s.aHeldFolded[k] = 0;
        if (iosSaberFold_s.bAuto && iosSaberFold_s.aTarget[k] && iosSaberFold_s.aSwung[k] && now - iosSaberFold_s.lastSwingMs >= IOSSABERFOLD_CALM_MS)
            fold |= 1 << k;
    }
    if (fold && !iosSaberStaff_IsBusy(p)) {
        iosSaberFold_Clicked(iosSaberFold_Target(fold, 0));
        for (int k = 0; k < 2; k++)
            if (fold & (1 << k)) iosSaberFold_s.aSwung[k] = 0;
    }
}

// ------------------------------------------------------------ other players' (multiplayer)

// (each player's own, by their number, as the looks are kept: jkPlayerLook)
#define IOSSABERFOLD_SLOTS JKPLAYER_NUM_INFOS
typedef struct {
    SithThing* pThing; // (NULL: none)
    uint32_t signature;
    int bits;
    iosSaberFoldSnap aSaber[2];
} iosSaberFoldSlot;
static iosSaberFoldSlot iosSaberFold_aSlot[IOSSABERFOLD_SLOTS];

// (the player's slot, by their number; NULL if none)
static iosSaberFoldSlot* iosSaberFold_SlotOf(SithThing* pThing)
{
    if (!pThing || pThing->type != SITH_THING_PLAYER || !pThing->actorParams.pPlayer) return NULL;
    const uintptr_t at = (uintptr_t)pThing->actorParams.pPlayer, first = (uintptr_t)&jkPlayer_playerInfos[0];
    if (at < first || (at - first) % sizeof(jkPlayer_playerInfos[0])) return NULL;
    const uintptr_t i = (at - first) / sizeof(jkPlayer_playerInfos[0]);
    return i < IOSSABERFOLD_SLOTS ? &iosSaberFold_aSlot[i] : NULL;
}

// (the slot, if it is this player's now)
static iosSaberFoldSlot* iosSaberFold_Find(SithThing* pThing)
{
    iosSaberFoldSlot* s = iosSaberFold_SlotOf(pThing);
    return (s && s->pThing == pThing && s->signature == pThing->signature) ? s : NULL;
}

void iosSaberFold_SetOpenBits(SithThing* pThing, int bits)
{
    if (!pThing || pThing == sithPlayer_g_pLocalPlayerThing) return;
    iosSaberFoldSlot* s = iosSaberFold_SlotOf(pThing);
    if (!s) return;
    bits &= 3;
    if (s->pThing != pThing || s->signature != pThing->signature) {
        // (first heard, or a FOLD new to the player: as it is, at once; no snap, no click)
        s->pThing = pThing;
        s->signature = pThing->signature;
        s->bits = bits;
        for (int k = 0; k < 2; k++) iosSaberFold_SnapSet(&s->aSaber[k], (bits >> k) & 1);
        return;
    }
    if (bits == s->bits) return;
    const uint32_t now = iosSaberFold_Now();
    int bMoved = 0;
    for (int k = 0; k < 2; k++) bMoved |= iosSaberFold_SnapTo(&s->aSaber[k], (bits >> k) & 1, now);
    s->bits = bits;
    if (bMoved && (pThing->jkFlags & JKFLAG_SABERON) && (iosSaberStaff_HiltOf(pThing, 0) == IOSSABER_HILT_FOLD || iosSaberStaff_HiltOf(pThing, 1) == IOSSABER_HILT_FOLD))
        iosSaberFold_Click(pThing);
}

// (a player without a FOLD now, or not heard: forgotten, so one picked later starts as it is heard)
static void iosSaberFold_Forget(SithThing* pThing)
{
    iosSaberFoldSlot* s = iosSaberFold_Find(pThing);
    if (s) s->pThing = NULL;
}

// Each other player's FOLD as their game sent it (the look's flags, jkPlayerLook)
static void iosSaberFold_TickOthers(void)
{
#ifdef QOL_IMPROVEMENTS
    if (!sithNet_isMulti) return;
    for (uint32_t i = 0; i < jkPlayer_maxPlayers; i++) {
        SithThing* t = jkPlayer_playerInfos[i].pLocalPlayer;
        if (!(jkPlayer_playerInfos[i].flags & 1) || !t || t == sithPlayer_g_pLocalPlayerThing || t->type != SITH_THING_PLAYER) continue;
        const jkPlayerLook* pLook = jkPlayerLook_ForThing(t);
        if (!pLook || (pLook->aHilt[0] != IOSSABER_HILT_FOLD && pLook->aHilt[1] != IOSSABER_HILT_FOLD)) {
            iosSaberFold_Forget(t);
            continue;
        }
        int bits = 0;
        for (int k = 0; k < 2; k++)
            if (pLook->flags & JKPLAYERLOOK_FLAG_FOLD_OPEN(k)) bits |= 1 << k;
        iosSaberFold_SetOpenBits(t, bits);
    }
#endif
}

int iosSaberFold_LocalOpen(int hand)
{
    return iosSaberFold_IsOpen(sithPlayer_g_pLocalPlayerThing, hand);
}

int iosSaberFold_OpenBits(SithThing* pThing)
{
    if (!pThing) return 0;
    if (pThing == sithPlayer_g_pLocalPlayerThing) {
        int bits = 0;
        for (int k = 0; k < 2; k++)
            if (iosSaberStaff_SaberHilt(pThing, k) == IOSSABER_HILT_FOLD && iosSaberFold_s.aSaber[k].target) bits |= 1 << k;
        return bits;
    }
    const iosSaberFoldSlot* s = iosSaberFold_Find(pThing);
    return s ? s->bits : 0;
}

// ------------------------------------------------------------ drawing

float iosSaberFold_OpenFraction(SithThing* pThing, int saber)
{
    if (!pThing || saber < 0 || saber > 1) return 0.0f;
    const uint32_t now = iosSaberFold_Now();
    if (pThing == sithPlayer_g_pLocalPlayerThing) {
        if (iosSaberFold_Flying(saber)) return iosSaberFold_OutFraction(&iosSaberFold_s.aOut[saber], now);
        return iosSaberFold_SnapNow(&iosSaberFold_s.aSaber[saber], now);
    }
    const iosSaberFoldSlot* s = iosSaberFold_Find(pThing);
    return s ? iosSaberFold_SnapNow(&s->aSaber[saber], now) : 0.0f;
}

int iosSaberFold_IsOpen(SithThing* pThing, int hand)
{
    if (!pThing || hand < 0 || hand > 1 || iosSaberStaff_HiltOf(pThing, hand) != IOSSABER_HILT_FOLD) return 0;
    const int saber = iosSaberStaff_SaberInHand(pThing, hand);
    if (pThing == sithPlayer_g_pLocalPlayerThing) return iosSaberFold_s.aSaber[saber].target;
    const iosSaberFoldSlot* s = iosSaberFold_Find(pThing);
    return s ? s->aSaber[saber].target : 0;
}

#endif // JK_ENHANCED
