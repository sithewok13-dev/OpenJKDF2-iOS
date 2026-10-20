// Added: entire file. See iosSaberWheel.h.

#include "Enhanced/iosSaberWheel.h"

#ifdef JK_ENHANCED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Enhanced/iosSaber.h"
#include "Enhanced/iosSaberFold.h"

static struct {
    int bSameColour, bSameHilt;
    int pickHand;  // the card outlined
    int pickBlade; // its dot ringed, or -1: the whole hand
} iosSaberWheel_s = { 0, 0, IOSSABER_HAND_RIGHT, -1 };

static int iosSaberWheel_ValidHand(int hand)
{
    return hand >= 0 && hand < IOSSABER_NUM_HANDS;
}

static int iosSaberWheel_Blades(int hand)
{
    return iosSaber_HiltNumBlades(iosSaber_GetHandHilt(hand));
}

// (the hands in use: the right, and the left with two sabers)
static int iosSaberWheel_InUse(int hand)
{
    return hand == IOSSABER_HAND_RIGHT || (hand == IOSSABER_HAND_LEFT && iosSaberWheel_NumSabers() > 1);
}

// (SAME COLOUR and SAME HILT act only with two sabers; with one, as off, each
// switch kept as set for when the second comes back)
static int iosSaberWheel_SameColourActs(void)
{
    return iosSaberWheel_s.bSameColour && iosSaberWheel_NumSabers() > 1;
}

static int iosSaberWheel_SameHiltActs(void)
{
    return iosSaberWheel_s.bSameHilt && iosSaberWheel_NumSabers() > 1;
}

static void iosSaberWheel_Settle(int* pHand, int* pBlade);

// ------------------------------------------------------------ one saber or two

int iosSaberWheel_NumSabers(void)
{
    return iosSaber_StyleNumBlades(iosSaber_GetStyle()) > 1 ? 2 : 1;
}

void iosSaberWheel_AddSecond(void)
{
    if (!iosSaber_StyleAvailable(IOSSABER_STYLE_DUAL)) return;
    iosSaber_SetStyle(IOSSABER_STYLE_DUAL);
    iosSaberWheel_PickHand(IOSSABER_HAND_LEFT);
}

void iosSaberWheel_RemoveSecond(void)
{
    iosSaber_SetStyle(IOSSABER_STYLE_SINGLE);
    if (iosSaberWheel_s.pickHand != IOSSABER_HAND_RIGHT) iosSaberWheel_PickHand(IOSSABER_HAND_RIGHT);
}

// ------------------------------------------------------------ hilts

int iosSaberWheel_GetHilt(int hand)
{
    return iosSaber_GetHandHilt(hand);
}

void iosSaberWheel_SetHilt(int hand, int hilt)
{
    if (!iosSaberWheel_ValidHand(hand) || !iosSaber_HiltAvailable(hilt)) return;
    const int bBoth = iosSaberWheel_SameHiltActs();
    for (int h = 0; h < IOSSABER_NUM_HANDS; h++)
        if (h == hand || bBoth) iosSaber_SetHandHilt(h, hilt);
    iosSaberWheel_Settle(NULL, NULL); // (a blade picked on a hand now with one: its whole hand, for good)
}

int iosSaberWheel_CycleHilt(int hand)
{
    if (!iosSaberWheel_ValidHand(hand)) return IOSSABER_HILT_NORMAL;
    iosSaberWheel_SetHilt(hand, iosSaber_NextHilt(iosSaber_GetHandHilt(hand)));
    return iosSaber_GetHandHilt(hand);
}

// ------------------------------------------------------------ SAME COLOUR, SAME HILT

int iosSaberWheel_GetSameColour(void)
{
    return iosSaberWheel_SameColourActs();
}

void iosSaberWheel_SetSameColour(int bOn)
{
    iosSaberWheel_s.bSameColour = bOn ? 1 : 0;
}

int iosSaberWheel_GetSameHilt(void)
{
    return iosSaberWheel_SameHiltActs();
}

void iosSaberWheel_SetSameHilt(int bOn)
{
    iosSaberWheel_s.bSameHilt = bOn ? 1 : 0;
}

// ------------------------------------------------------------ what the ring paints

void iosSaberWheel_PickHand(int hand)
{
    if (!iosSaberWheel_ValidHand(hand) || !iosSaberWheel_InUse(hand)) hand = IOSSABER_HAND_RIGHT;
    iosSaberWheel_s.pickHand = hand;
    iosSaberWheel_s.pickBlade = -1;
}

int iosSaberWheel_BladesPickable(int hand)
{
    return iosSaberWheel_ValidHand(hand) && iosSaberWheel_InUse(hand) && iosSaberWheel_Blades(hand) > 1 && !iosSaberWheel_SameColourActs();
}

void iosSaberWheel_PickBlade(int hand, int blade)
{
    iosSaberWheel_PickHand(hand);
    hand = iosSaberWheel_s.pickHand;
    // (only picked: a linked pair stays linked until blade 2 is given a
    // colour of its own, iosSaberWheel_ApplyColour. SAME COLOUR on, the dots
    // are their card's: the ring paints every blade)
    if (blade < 0 || blade >= iosSaberWheel_Blades(hand) || !iosSaberWheel_BladesPickable(hand)) return;
    iosSaberWheel_s.pickBlade = blade;
}

// (the pick as it stands now, and kept so: a hand put away, or a hilt with
// fewer blades since, is back to the right hand's or that hand's whole --
// for good, so a STAFF back again starts whole)
static void iosSaberWheel_Settle(int* pHand, int* pBlade)
{
    int hand = iosSaberWheel_s.pickHand, blade = iosSaberWheel_s.pickBlade;
    if (!iosSaberWheel_ValidHand(hand) || !iosSaberWheel_InUse(hand)) {
        hand = IOSSABER_HAND_RIGHT;
        blade = -1;
    }
    if (blade >= iosSaberWheel_Blades(hand)) blade = -1;
    iosSaberWheel_s.pickHand = hand;
    iosSaberWheel_s.pickBlade = blade;
    if (pHand) *pHand = hand;
    if (pBlade) *pBlade = blade;
}

// (what the ring paints: SAME COLOUR on, every blade, so no blade of its own;
// the blade picked before is back when SAME COLOUR goes off)
static void iosSaberWheel_Pick(int* pHand, int* pBlade)
{
    iosSaberWheel_Settle(pHand, pBlade);
    if (iosSaberWheel_SameColourActs()) *pBlade = -1;
}

int iosSaberWheel_GetPickedHand(void)
{
    int hand, blade;
    iosSaberWheel_Pick(&hand, &blade);
    return hand;
}

int iosSaberWheel_GetPickedBlade(void)
{
    int hand, blade;
    iosSaberWheel_Pick(&hand, &blade);
    return blade;
}

int iosSaberWheel_PaintKind(void)
{
    if (iosSaberWheel_SameColourActs()) return IOSSABERWHEEL_PAINT_ALL;
    return iosSaberWheel_GetPickedBlade() >= 0 ? IOSSABERWHEEL_PAINT_BLADE : IOSSABERWHEEL_PAINT_HAND;
}

// (a whole hand: both its blades, linked again)
static void iosSaberWheel_PaintHand(int hand, int id)
{
    iosSaber_SetBladeColour(hand, 0, id);
    iosSaber_SetBladeColour(hand, 1, id);
    iosSaber_SetBladeLinked(hand, 1);
}

void iosSaberWheel_ApplyColour(int id)
{
    if (id < 0 || id >= iosSaber_NumColours()) return;
    int hand, blade;
    iosSaberWheel_Settle(&hand, &blade);
    if (iosSaberWheel_SameColourActs()) {
        // (every blade of both hands, every pair linked again: a blade picked
        // before is its whole hand now)
        for (int h = 0; h < IOSSABER_NUM_HANDS; h++) iosSaberWheel_PaintHand(h, id);
        iosSaberWheel_s.pickBlade = -1;
    }
    else if (blade < 0) {
        iosSaberWheel_PaintHand(hand, id);
    }
    else if (blade == 1 && iosSaber_GetBladeLinked(hand)) {
        // (the second of a linked pair: split from the first only for a
        // colour other than the first's, as shown; the first keeps its own)
        if (id == iosSaber_GetShownBladeColour(hand, 0)) return;
        iosSaber_SetBladeColour(hand, 1, id);
        iosSaber_SetBladeLinked(hand, 0);
    }
    else {
        iosSaber_SetBladeColour(hand, blade, id); // (the first of a linked pair: the second follows)
    }
}

int iosSaberWheel_PaintColour(void)
{
    int hand, blade;
    iosSaberWheel_Pick(&hand, &blade);
    return iosSaber_GetShownBladeColour(hand, blade < 0 ? 0 : blade);
}

const char* iosSaberWheel_HubText(int bTapMode)
{
    static char text[48];
    int hand, blade;
    iosSaberWheel_Pick(&hand, &blade);
    const char* side = hand == IOSSABER_HAND_LEFT ? "left" : "right";
    if (blade >= 0) {
        snprintf(text, sizeof(text), "%s saber, blade %d", side, blade + 1);
        return text;
    }
    if (iosSaberWheel_NumSabers() < 2) return bTapMode ? "tap a colour" : "slide toward a colour";
    if (iosSaberWheel_SameColourActs()) return "colour for both sabers";
    snprintf(text, sizeof(text), "colour for the %s saber", side);
    return text;
}

// ------------------------------------------------------------ the cards

void iosSaberWheel_GetCard(int hand, iosSaberWheelCard* pOut)
{
    memset(pOut, 0, sizeof(*pOut));
    pOut->pickedBlade = -1;
    if (!iosSaberWheel_ValidHand(hand)) return;
    int pickHand, pickBlade;
    iosSaberWheel_Pick(&pickHand, &pickBlade);
    pOut->bShown = iosSaberWheel_InUse(hand);
    pOut->hilt = iosSaber_GetHandHilt(hand);
    pOut->numBlades = iosSaberWheel_Blades(hand);
    pOut->bLinked = pOut->numBlades < 2 || iosSaber_GetBladeLinked(hand);
    pOut->aColour[0] = iosSaber_GetShownBladeColour(hand, 0);
    pOut->aColour[1] = pOut->bLinked ? pOut->aColour[0] : iosSaber_GetShownBladeColour(hand, 1);
    pOut->bPicked = pOut->bShown && pickHand == hand;
    if (pOut->bPicked) pOut->pickedBlade = pickBlade;
}

// ------------------------------------------------------------ FILM, AUTO

int iosSaberWheel_ShowAuto(void)
{
    for (int h = 0; h < IOSSABER_NUM_HANDS; h++)
        if (iosSaberWheel_InUse(h) && iosSaber_GetHandHilt(h) == IOSSABER_HILT_FOLD) return 1;
    return 0;
}

int iosSaberWheel_GetAuto(void)
{
    return iosSaberFold_GetAuto();
}

void iosSaberWheel_SetAuto(int bOn)
{
    iosSaberFold_SetAuto(bOn);
}

// ------------------------------------------------------------ RANDOM

static unsigned int iosSaberWheel_dice; // (0: not seeded yet)

void iosSaberWheel_SeedRandom(unsigned int seed)
{
    iosSaberWheel_dice = seed ? seed : 0x9E3779B9u; // (xorshift never leaves 0)
}

// (0 .. n - 1, n > 0: xorshift32, seeded from the clock at the first throw)
static int iosSaberWheel_Roll(int n)
{
    if (!iosSaberWheel_dice) iosSaberWheel_SeedRandom((unsigned int)time(NULL) * 2654435761u ^ (unsigned int)clock());
    unsigned int x = iosSaberWheel_dice;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    iosSaberWheel_dice = x;
    return (int)(x % (unsigned int)n);
}

// (two colours that look the same on the wheel: the same one, or the same
// swatch -- ORIGINAL and the game's own blade's colour)
static int iosSaberWheel_LooksSame(int a, int b)
{
    if (a == b) return 1;
    unsigned char ra, ga, ba, rb, gb, bb;
    iosSaber_ColourRGB(a, &ra, &ga, &ba);
    iosSaber_ColourRGB(b, &rb, &gb, &bb);
    return ra == rb && ga == gb && ba == bb;
}

// A colour on this game's wheel in this look that looks like none of the n
// in aNot, at random; -1 if there is none
static int iosSaberWheel_RandomColour(const int* aNot, int n)
{
    int aOk[64], k = 0;
    for (int id = 0; id < iosSaber_NumColours() && k < 64; id++) {
        if (!iosSaber_ColourOnWheel(id)) continue;
        int bOut = 0;
        for (int i = 0; i < n && !bOut; i++) bOut = iosSaberWheel_LooksSame(id, aNot[i]);
        if (!bOut) aOk[k++] = id;
    }
    return k ? aOk[iosSaberWheel_Roll(k)] : -1;
}

// An available hilt none of the n in aNot is, at random; -1 if there is none
static int iosSaberWheel_RandomHilt(const int* aNot, int n)
{
    int aOk[IOSSABER_NUM_HILTS], k = 0;
    for (int h = 0; h < IOSSABER_NUM_HILTS; h++) {
        if (!iosSaber_HiltAvailable(h)) continue;
        int bOut = 0;
        for (int i = 0; i < n; i++) bOut |= h == aNot[i];
        if (!bOut) aOk[k++] = h;
    }
    return k ? aOk[iosSaberWheel_Roll(k)] : -1;
}

// (the colours a hand shows now: its first blade's, and its second's when
// it has one of its own; how many)
static int iosSaberWheel_ShownColours(int hand, int* aOut)
{
    aOut[0] = iosSaber_GetShownBladeColour(hand, 0);
    if (iosSaberWheel_Blades(hand) < 2 || iosSaber_GetBladeLinked(hand)) return 1;
    aOut[1] = iosSaber_GetShownBladeColour(hand, 1);
    return 2;
}

int iosSaberWheel_Random(int bColoursOnly)
{
    const int numHands = iosSaberWheel_NumSabers();
    // (what every saber held shows now, before its hilt changes)
    int aWas[IOSSABER_NUM_HANDS][2], aNumWas[IOSSABER_NUM_HANDS];
    for (int h = 0; h < numHands; h++) aNumWas[h] = iosSaberWheel_ShownColours(h, aWas[h]);
    int bChanged = 0;
    if (!bColoursOnly) {
        // a hilt other than its own, as its HILT sets it (SAME HILT: both
        // hands to one, neither's own -- or, with too few hilts for that,
        // the right's not)
        int aHilt[IOSSABER_NUM_HANDS];
        for (int h = 0; h < numHands; h++) aHilt[h] = iosSaber_GetHandHilt(h);
        if (iosSaberWheel_SameHiltActs()) {
            int hilt = iosSaberWheel_RandomHilt(aHilt, numHands);
            if (hilt < 0) hilt = iosSaberWheel_RandomHilt(aHilt, 1);
            if (hilt >= 0) iosSaberWheel_SetHilt(IOSSABER_HAND_RIGHT, hilt);
            bChanged |= hilt >= 0;
        }
        else {
            for (int h = 0; h < numHands; h++) {
                const int hilt = iosSaberWheel_RandomHilt(&aHilt[h], 1);
                if (hilt >= 0) iosSaberWheel_SetHilt(h, hilt);
                bChanged |= hilt >= 0;
            }
        }
    }
    if (iosSaberWheel_SameColourActs()) {
        // every blade one colour no blade shows now, every pair linked
        // again, as a colour tapped with SAME COLOUR on
        int aNot[IOSSABER_NUM_HANDS * 2], n = 0;
        for (int h = 0; h < numHands; h++)
            for (int i = 0; i < aNumWas[h]; i++) aNot[n++] = aWas[h][i];
        const int id = iosSaberWheel_RandomColour(aNot, n);
        if (id >= 0) {
            for (int h = 0; h < IOSSABER_NUM_HANDS; h++) iosSaberWheel_PaintHand(h, id);
            iosSaberWheel_s.pickBlade = -1;
            bChanged = 1;
        }
    }
    else {
        for (int h = 0; h < numHands; h++) {
            const int id = iosSaberWheel_RandomColour(aWas[h], aNumWas[h]);
            if (id < 0) continue;
            bChanged = 1;
            if (iosSaberWheel_Blades(h) < 2 || iosSaber_GetBladeLinked(h)) {
                // one blade, or two joined: one colour (as a colour tapped for the whole hand)
                iosSaberWheel_PaintHand(h, id);
                continue;
            }
            // split: two, neither one the hand shows now, nor each other's
            int aNot[3] = { aWas[h][0], aWas[h][aNumWas[h] - 1], id };
            const int id2 = iosSaberWheel_RandomColour(aNot, 3);
            iosSaber_SetBladeColour(h, 0, id);
            iosSaber_SetBladeColour(h, 1, id2 >= 0 ? id2 : id);
        }
    }
    iosSaberWheel_Settle(NULL, NULL);
    return bChanged;
}

// ------------------------------------------------------------ the SABER button

void iosSaberWheel_GetIcon(iosSaberWheelIcon* pOut)
{
    memset(pOut, 0, sizeof(*pOut));
    pOut->numSabers = iosSaberWheel_NumSabers();
    for (int h = 0; h < IOSSABER_NUM_HANDS && h < pOut->numSabers; h++) {
        pOut->aHand[h].hilt = iosSaber_GetHandHilt(h);
        pOut->aHand[h].bOpen = pOut->aHand[h].hilt == IOSSABER_HILT_FOLD && iosSaberFold_LocalOpen(h);
        for (int b = 0; b < 2; b++) pOut->aHand[h].aColour[b] = iosSaber_GetShownBladeColour(h, b);
    }
}

int iosSaberWheel_TapFlips(void)
{
    return iosSaberFold_CanFlip();
}

int iosSaberWheel_Flip(void)
{
    return iosSaberFold_Flip();
}

int iosSaberWheel_TapFlip(int bAgain)
{
    return iosSaberFold_TapFlip(bAgain);
}

int iosSaberWheel_KeyFlip(void)
{
    return iosSaberFold_KeyFlip();
}

// ------------------------------------------------------------ kept

enum {
    KEPT_STYLE, KEPT_HILT_R, KEPT_HILT_L, KEPT_COLOUR_R1, KEPT_COLOUR_R2, KEPT_COLOUR_L1, KEPT_COLOUR_L2,
    KEPT_LINK_R, KEPT_LINK_L, KEPT_SAME_COLOUR, KEPT_SAME_HILT, KEPT_FOLD_AUTO, KEPT_FOLD_OPEN,
    KEPT_FOLD_OPEN_R, KEPT_FOLD_OPEN_L, KEPT_NUM
};
static const char* const iosSaberWheel_aKept[KEPT_NUM] = {
    "Style", "HiltRight", "HiltLeft", "ColourRight1", "ColourRight2", "ColourLeft1", "ColourLeft2",
    "LinkRight", "LinkLeft", "SameColour", "SameHilt", "FoldAuto", "FoldOpen",
    "FoldOpenRight", "FoldOpenLeft",
};
// (what builds before kept: the shared hilt, and each style's colours under two spellings)
enum { OLD_HILT, OLD_SINGLE, OLD_DUAL_R, OLD_DUAL_L, OLD_CROSS, OLD_NUM_KINDS };
static const struct { const char* name; int kind; } iosSaberWheel_aOld[] = {
    { "Hilt", OLD_HILT },
    { "Colour.SINGLE.0", OLD_SINGLE }, { "Colour.DUAL.0", OLD_DUAL_R }, { "Colour.DUAL.1", OLD_DUAL_L }, { "Colour.CROSS.0", OLD_CROSS },
    { "ColourSingle", OLD_SINGLE }, { "ColourDualRight", OLD_DUAL_R }, { "ColourDualLeft", OLD_DUAL_L }, { "ColourCross", OLD_CROSS },
};
#define IOSSABERWHEEL_NUM_OLD ((int)(sizeof(iosSaberWheel_aOld) / sizeof(iosSaberWheel_aOld[0])))

static struct {
    int bLoading;
    int aPresent[KEPT_NUM];
    char aOld[OLD_NUM_KINDS][IOSSABERWHEEL_KEPT_LEN]; // ("" none)
    int movedHilt;                                    // (Style CROSS: one saber, this hilt in the right hand)
    int foldAuto, foldOpen, aFoldOpen[2];             // (FOLD's, put on at the end: AUTO first)
    int aLink[IOSSABER_NUM_HANDS];                    // (put on at the end: after the hilts, whatever their order)
} iosSaberWheel_load;

int iosSaberWheel_NumKept(void)
{
    return KEPT_NUM;
}

const char* iosSaberWheel_KeptName(int i)
{
    return (i >= 0 && i < KEPT_NUM) ? iosSaberWheel_aKept[i] : NULL;
}

int iosSaberWheel_NumOldKept(void)
{
    return IOSSABERWHEEL_NUM_OLD;
}

const char* iosSaberWheel_OldKeptName(int i)
{
    return (i >= 0 && i < IOSSABERWHEEL_NUM_OLD) ? iosSaberWheel_aOld[i].name : NULL;
}

static void iosSaberWheel_ColourKey(int id, char* pOut, int len)
{
    const char* key = iosSaber_ColourKey(id);
    snprintf(pOut, len, "%s", key ? key : "game");
}

void iosSaberWheel_GetKept(int i, char* pOut, int len)
{
    if (!pOut || len <= 0) return;
    pOut[0] = 0;
    switch (i) {
    case KEPT_STYLE: snprintf(pOut, len, "%s", iosSaber_StyleName(iosSaber_GetStyle())); break;
    case KEPT_HILT_R: case KEPT_HILT_L: snprintf(pOut, len, "%s", iosSaber_HiltName(iosSaber_GetHandHilt(i - KEPT_HILT_R))); break;
    case KEPT_COLOUR_R1: case KEPT_COLOUR_R2: case KEPT_COLOUR_L1: case KEPT_COLOUR_L2: {
        const int k = i - KEPT_COLOUR_R1;
        iosSaberWheel_ColourKey(iosSaber_GetBladeColour(k / 2, k % 2), pOut, len);
        break;
    }
    case KEPT_LINK_R: case KEPT_LINK_L: snprintf(pOut, len, "%d", iosSaber_GetBladeLinked(i - KEPT_LINK_R)); break;
    case KEPT_SAME_COLOUR: snprintf(pOut, len, "%d", iosSaberWheel_s.bSameColour); break;
    case KEPT_SAME_HILT: snprintf(pOut, len, "%d", iosSaberWheel_s.bSameHilt); break;
    case KEPT_FOLD_AUTO: snprintf(pOut, len, "%d", iosSaberFold_GetAuto() ? 1 : 0); break;
    // (with AUTO on, it opens and folds by itself: not a choice to keep)
    // (FoldOpen, as builds before kept it, one for both: the right hand's)
    case KEPT_FOLD_OPEN: snprintf(pOut, len, "%d", (!iosSaberFold_GetAuto() && iosSaberFold_GetSaberHeldOpen(0)) ? 1 : 0); break;
    case KEPT_FOLD_OPEN_R: case KEPT_FOLD_OPEN_L:
        snprintf(pOut, len, "%d", (!iosSaberFold_GetAuto() && iosSaberFold_GetSaberHeldOpen(i - KEPT_FOLD_OPEN_R)) ? 1 : 0);
        break;
    default: break;
    }
}

static int iosSaberWheel_Bool(const char* v)
{
    return v && (atoi(v) != 0 || !strcmp(v, "true") || !strcmp(v, "YES"));
}

// One kept choice put on (bLoad: as part of a load); 0 if it is no such name
static int iosSaberWheel_Put(const char* name, const char* v, int bLoad)
{
    int i = 0;
    while (i < KEPT_NUM && strcmp(name, iosSaberWheel_aKept[i])) i++;
    if (i >= KEPT_NUM) return 0;
    if (bLoad) iosSaberWheel_load.aPresent[i] = 1;
    switch (i) {
    case KEPT_STYLE: {
        int moved = IOSSABER_HILT_NORMAL;
        const int st = iosSaber_StyleByName(v, &moved);
        if (st >= 0) iosSaber_SetStyle(st);
        if (bLoad && moved != IOSSABER_HILT_NORMAL) iosSaberWheel_load.movedHilt = moved;
        break;
    }
    case KEPT_HILT_R: case KEPT_HILT_L: {
        const int h = iosSaber_HiltByName(v); // (one this build hasn't, or not available: NORMAL)
        iosSaber_SetHandHilt(i - KEPT_HILT_R, iosSaber_HiltAvailable(h) ? h : IOSSABER_HILT_NORMAL);
        break;
    }
    case KEPT_COLOUR_R1: case KEPT_COLOUR_R2: case KEPT_COLOUR_L1: case KEPT_COLOUR_L2: {
        const int k = i - KEPT_COLOUR_R1, id = iosSaber_ColourByKey(v);
        iosSaber_SetBladeColour(k / 2, k % 2, id >= 0 ? id : 0); // (a colour no more: the game's own)
        break;
    }
    case KEPT_LINK_R: case KEPT_LINK_L:
        if (bLoad) iosSaberWheel_load.aLink[i - KEPT_LINK_R] = iosSaberWheel_Bool(v);
        else iosSaber_SetBladeLinked(i - KEPT_LINK_R, iosSaberWheel_Bool(v));
        break;
    case KEPT_SAME_COLOUR: iosSaberWheel_s.bSameColour = iosSaberWheel_Bool(v); break;
    case KEPT_SAME_HILT: iosSaberWheel_s.bSameHilt = iosSaberWheel_Bool(v); break;
    case KEPT_FOLD_AUTO:
        if (bLoad) iosSaberWheel_load.foldAuto = iosSaberWheel_Bool(v);
        else iosSaberFold_SetAuto(iosSaberWheel_Bool(v));
        break;
    case KEPT_FOLD_OPEN:
        if (bLoad) iosSaberWheel_load.foldOpen = iosSaberWheel_Bool(v);
        else iosSaberFold_SetHeldOpen(iosSaberWheel_Bool(v)); // (both)
        break;
    case KEPT_FOLD_OPEN_R: case KEPT_FOLD_OPEN_L:
        if (bLoad) iosSaberWheel_load.aFoldOpen[i - KEPT_FOLD_OPEN_R] = iosSaberWheel_Bool(v);
        else iosSaberFold_SetSaberHeldOpen(i - KEPT_FOLD_OPEN_R, iosSaberWheel_Bool(v));
        break;
    default: break;
    }
    return 1;
}

void iosSaberWheel_BeginLoad(void)
{
    memset(&iosSaberWheel_load, 0, sizeof(iosSaberWheel_load));
    iosSaberWheel_load.bLoading = 1;
    iosSaberWheel_load.foldAuto = 1;
    iosSaberWheel_load.movedHilt = IOSSABER_HILT_NORMAL;
    // everything to its start
    iosSaber_SetStyle(IOSSABER_STYLE_SINGLE);
    for (int h = 0; h < IOSSABER_NUM_HANDS; h++) {
        iosSaber_SetHandHilt(h, IOSSABER_HILT_NORMAL);
        iosSaberWheel_PaintHand(h, 0);
    }
    iosSaberWheel_s.bSameColour = iosSaberWheel_s.bSameHilt = 0;
    iosSaberWheel_PickHand(IOSSABER_HAND_RIGHT);
}

void iosSaberWheel_LoadKept(const char* name, const char* value)
{
    if (!name || !value) return;
    if (!iosSaberWheel_load.bLoading) {
        iosSaberWheel_SetKept(name, value);
        return;
    }
    if (iosSaberWheel_Put(name, value, 1)) return;
    for (int i = 0; i < IOSSABERWHEEL_NUM_OLD; i++)
        if (!strcmp(name, iosSaberWheel_aOld[i].name) && value[0])
            snprintf(iosSaberWheel_load.aOld[iosSaberWheel_aOld[i].kind], IOSSABERWHEEL_KEPT_LEN, "%s", value);
}

// (an old colour kept, by its key; -1 none)
static int iosSaberWheel_OldColour(int kind)
{
    return iosSaberWheel_load.aOld[kind][0] ? iosSaber_ColourByKey(iosSaberWheel_load.aOld[kind]) : -1;
}

void iosSaberWheel_EndLoad(void)
{
    if (!iosSaberWheel_load.bLoading) return;
    const int* p = iosSaberWheel_load.aPresent;
    // the hilts not kept by hand: the shared one (both hands'), or the right hand's CROSS kept as a style
    if (!p[KEPT_HILT_R] && !p[KEPT_HILT_L]) {
        const int shared = iosSaberWheel_load.aOld[OLD_HILT][0] ? iosSaber_HiltByName(iosSaberWheel_load.aOld[OLD_HILT]) : -1;
        if (iosSaber_HiltAvailable(shared)) iosSaber_SetHilt(shared);
        else if (iosSaberWheel_load.movedHilt != IOSSABER_HILT_NORMAL) iosSaber_SetHandHilt(IOSSABER_HAND_RIGHT, iosSaberWheel_load.movedHilt);
    }
    // the colours not kept by blade: each style's, the right hand the style's in use, the left DUAL's left
    if (!p[KEPT_COLOUR_R1] && !p[KEPT_COLOUR_R2] && !p[KEPT_COLOUR_L1] && !p[KEPT_COLOUR_L2]) {
        // (a CROSS kept as a style: its own colour; none kept for it (a build without CROSS wrote the file last,
        // keeping the style as it was), one saber's; neither, the game's)
        int right = -1;
        if (iosSaber_GetStyle() == IOSSABER_STYLE_DUAL) right = iosSaberWheel_OldColour(OLD_DUAL_R);
        else if (iosSaberWheel_load.movedHilt != IOSSABER_HILT_NORMAL)
            right = iosSaberWheel_OldColour(OLD_CROSS) >= 0 ? iosSaberWheel_OldColour(OLD_CROSS) : (iosSaberWheel_OldColour(OLD_SINGLE) >= 0 ? iosSaberWheel_OldColour(OLD_SINGLE) : 0);
        else right = iosSaberWheel_OldColour(OLD_SINGLE);
        const int left = iosSaberWheel_OldColour(OLD_DUAL_L);
        if (right >= 0) iosSaberWheel_PaintHand(IOSSABER_HAND_RIGHT, right);
        if (left >= 0) iosSaberWheel_PaintHand(IOSSABER_HAND_LEFT, left);
    }
    for (int h = 0; h < IOSSABER_NUM_HANDS; h++)
        if (p[KEPT_LINK_R + h]) iosSaber_SetBladeLinked(h, iosSaberWheel_load.aLink[h]);
    // FOLD: AUTO, then how each was left with AUTO off: each hand's own, or FoldOpen for both (kept by builds
    // before; or changed by one since this build kept the hands, which writes FoldOpen as the right hand's)
    iosSaberFold_SetAuto(iosSaberWheel_load.foldAuto);
    const int bOldSince = p[KEPT_FOLD_OPEN] && p[KEPT_FOLD_OPEN_R] && iosSaberWheel_load.foldOpen != iosSaberWheel_load.aFoldOpen[0];
    for (int k = 0; k < 2 && !iosSaberWheel_load.foldAuto; k++)
        iosSaberFold_SetSaberHeldOpen(k, (p[KEPT_FOLD_OPEN_R + k] && !bOldSince) ? iosSaberWheel_load.aFoldOpen[k] : iosSaberWheel_load.foldOpen);
    iosSaberWheel_load.bLoading = 0;
    iosSaberWheel_PickHand(IOSSABER_HAND_RIGHT);
}

int iosSaberWheel_SetKept(const char* name, const char* value)
{
    if (!name || !value) return 0;
    const int bTaken = iosSaberWheel_Put(name, value, 0);
    iosSaberWheel_Settle(NULL, NULL); // (a hilt or the style changed: the pick as it can stand)
    return bTaken;
}

#endif // JK_ENHANCED
