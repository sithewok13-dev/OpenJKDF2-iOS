// Added: entire file. Tests for the touch overlay's core (touchUi.c), with
// scripted, timed touches and a fake game: one or more cases per rule of the
// touch controls standard. Plain C, no GL, no SDL.
//
//   cc -std=c11 -DTOUCHUI_ENABLED -DJK_ENHANCED -I src src/Platform/Touch/tests/touchUi_test.c
//      src/Platform/Touch/touchUi.c src/Platform/Touch/touchUiDraw.c
//      src/Platform/Input/inputWheel.c src/Enhanced/iosSaberWheel.c -lm && ./a.out
//
// The wheels are inputWheel.c's, which asks the game itself (iosGame.h,
// iosSaber.h) and ticks in the hand itself (platServices_Haptic): those are
// the fake game here too, below.

#include "Platform/Touch/touchUi.h"
#include "Platform/Input/inputWheel.h"
#include "Enhanced/iosSaberWheel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------ the fake game

static struct {
    int bMots;
    int aPowerLevel[64]; // >0: learned, at that level (1..4); -1: learned at 0
    int curPower;
    const char* forceName; // (FORCE's power's name; NULL: SPEED)
    int aWeaponOwned[160], aWeaponAmmo[160], aWeaponSel[160];
    int curWeapon;
    int aItem[64];
    int bChat, chatToggles;
    int bHold, holdSets;
    int quickLoads;
    int selectedPower, selectedWeapon, selectedItem, weaponPicksCancelled;
    int bAlwaysRun;
    int bCheat, cheatToggles, bCanFly, bFlying, flightToggles, bCanNoclip, noclipToggles;
    int numLevels, levelLoaded;
    int bThird;
    int jumpStyle;
    char lastMsg[128];
    int bGauge;
    // the saber
    int bSaberBtn, saberStyle, bDualOk;
    // each hand's saber, as iosSaber keeps it: [hand][blade] colours, the
    // second blade linked to the first (to start), each hand's hilt
    int aBladeColour[2][2], aUnlinked[2];
    int aHilt[2], bCrossOk, bStaffOk, bFoldOk; // (0 NORMAL, 1 CROSS, 2 STAFF, 3 FOLD; CROSS on, STAFF and FOLD off as setup leaves them)
    int bFoldAuto, bFoldOpen, bFoldOpenL, bFoldLit, foldFlips; // (iosSaberFold's: AUTO, held open (the right hand's, the left's), a FOLD lit in hand)
    int saberLook; // (0 CLASSIC, the default, as setup leaves it; 1 REFORGED)
    int bRealNames; // (the colours' names as the game's: for how they fit)
    int saberShape; // (0 SPIKE, the default; 1 FILM)
    int bThrow, bThrowHidden; // THROW (Jedi Knight's Saber Throw, iosSaberThrowJk); not offered (multiplayer)
    // the camera
    int bCaptureFull, captures, lastKinds, lastMode, captureCancels, batch, batchSets;
    int selfieResult, selfieShots, bSelfie, selfieStops;
    int bFreezeOk, bFreeze, freezeEnds, bSmooth;
    float orbitYaw, orbitPitch, panX, panY, zoom;
    int cinemaResult, bCinema, cinemaStops;
} fg;
// (a style's blade's colour as tests before the hands had their own asked: the hand's first blade)
#define SC(st, b) fg.aBladeColour[(b) & 1][0]

static int F_IsMots(void) { return fg.bMots; }
static const char* F_GetForcePowerName(void) { return fg.curPower >= 0 ? (fg.forceName ? fg.forceName : "SPEED") : NULL; }
static int F_IsPowerAvailable(int bin) { return bin >= 0 && bin < 64 && fg.aPowerLevel[bin] != 0; }
static int F_GetPowerLevel(int bin) { return (bin >= 0 && bin < 64 && fg.aPowerLevel[bin] > 0) ? fg.aPowerLevel[bin] : 0; }
static const char* F_GetPowerName(int bin)
{
    static char b[64][16];
    if (bin < 0 || bin >= 64) return NULL;
    snprintf(b[bin], 16, "POWER %d", bin);
    return b[bin];
}
static int F_GetCurPower(void) { return fg.curPower; }
static void F_SelectPower(int bin) { fg.selectedPower = bin; fg.curPower = bin; }
static const char* F_GetWeaponName(int bin)
{
    static char b[160][24];
    if (bin < 0 || bin >= 160) return NULL;
    snprintf(b[bin], 24, bin == 3 ? "STORMTROOPER RIFLE" : "WPN %d", bin);
    return b[bin];
}
static int F_GetWeapon(int bin, int* pAmmo, int* pSel)
{
    *pAmmo = fg.aWeaponAmmo[bin];
    *pSel = fg.aWeaponSel[bin];
    return fg.aWeaponOwned[bin];
}
static int F_GetCurWeapon(void) { return fg.curWeapon; }
static int F_SelectWeapon(int bin) { fg.selectedWeapon = bin; return 1; }
static void F_CancelWeaponPick(void) { fg.weaponPicksCancelled++; }
static int F_GetItem(int bin, int* pAmount, int* pActive) { *pAmount = fg.aItem[bin]; *pActive = 0; return fg.aItem[bin] > 0; }
static void F_SelectItem(int bin) { fg.selectedItem = bin; }
static int F_QuickLoad(void) { fg.quickLoads++; return 1; }
static void F_SetHold(int b) { fg.bHold = b; fg.holdSets++; }
static int F_IsHolding(void) { return fg.bHold; }
static int F_IsChatOpen(void) { return fg.bChat; }
static void F_ToggleChat(void) { fg.bChat = !fg.bChat; fg.chatToggles++; }
static float F_GetForceMana(int* pbFull) { *pbFull = 0; return 0.5f; }
static unsigned int F_GetFrameCount(void) { return 0; }
static int F_IsAlwaysRun(void) { return fg.bAlwaysRun; }
static void F_GetMouseLookDegrees(float* pTurn, float* pPitch) { *pTurn = 0.1f; *pPitch = 0.1f; }
static int F_CanFly(void) { return fg.bCanFly; }
static int F_ToggleFlight(void) { fg.bFlying = !fg.bFlying; fg.flightToggles++; return fg.bFlying; }
static int F_IsFlying(void) { return fg.bFlying; }
static int F_CanNoclip(void) { return fg.bCanNoclip; }
static int F_ToggleNoclip(void) { fg.noclipToggles++; return 1; }
static int F_NumPickableLevels(void) { return fg.numLevels; }
static int F_GoToLevel(int l) { fg.levelLoaded = l; return 1; }
static int F_IsSithEwokOn(void) { return fg.bCheat; }
static int F_ToggleSithEwok(void) { fg.bCheat = !fg.bCheat; fg.cheatToggles++; return fg.bCheat; }
static int F_TakeSithEwokTypedOff(void) { return 0; }
static void F_SetCheatShown(int b) { (void)b; }
static void F_SetThirdPerson(int b) { fg.bThird = b; }
static int F_IsThirdPerson(void) { return fg.bThird; }
static void F_SetForceJumpStyle(int s) { fg.jumpStyle = s; }
static int F_GetForceJumpRank(void) { return 0; }
static void F_ShowMessage(const char* m) { snprintf(fg.lastMsg, sizeof(fg.lastMsg), "%s", m); }
static int F_GetGaugeRect(int bLeft, float* x0, float* y0, float* x1, float* y1)
{
    if (!fg.bGauge) return 0;
    // an iPhone-sized HUD gauge in the corner
    if (bLeft) { *x0 = 0; *y0 = 393 - 21 - 60; *x1 = 62; *y1 = 393 - 21; }
    else { *x0 = 852 - 62; *y0 = 393 - 21 - 60; *x1 = 852; *y1 = 393 - 21; }
    return 1;
}

static int F_IsSaberButtonShown(void) { return fg.bSaberBtn; }
static int F_SaberNumColours(void) { return 17; }
static const char* F_SaberColourName(int id)
{
    static char b[17][16];
    if (id < 0 || id >= 17) return NULL;
    static const char* const aReal[17] = { "ORIGINAL", "RED", "CRIMSON", "ORANGE", "AMBER", "YELLOW", "LIME", "GREEN", "TEAL", "CYAN",
                                           "LIGHT BLUE", "BLUE", "PURPLE", "MAGENTA", "PINK", "WHITE", "BLACK" }; // (iosSaber.c's)
    if (fg.bRealNames) return aReal[id];
    snprintf(b[id], 16, id ? "COLOUR %d" : "GAME", id);
    return b[id];
}
static const char* F_SaberColourKey(int id)
{
    static char b[17][8];
    if (id < 0 || id >= 17) return NULL;
    snprintf(b[id], 8, "c%d", id);
    return b[id];
}
static int F_SaberColourByKey(const char* k) { int n = -1; return (k[0] == 'c' && sscanf(k + 1, "%d", &n) == 1 && n >= 0 && n < 17) ? n : -1; }
static void F_SaberColourRGB(int id, unsigned char* r, unsigned char* g, unsigned char* b)
{
    // (with the game's names, its colours too: iosSaber.c's Jedi Knight swatches)
    static const unsigned char aReal[17][3] = { { 150, 150, 150 }, { 230, 45, 75 }, { 165, 10, 30 }, { 255, 95, 20 }, { 255, 158, 18 }, { 255, 230, 40 },
        { 150, 240, 40 }, { 50, 225, 75 }, { 15, 185, 150 }, { 20, 225, 240 }, { 115, 160, 240 }, { 50, 110, 255 }, { 190, 67, 240 }, { 245, 35, 195 },
        { 255, 115, 185 }, { 215, 225, 255 }, { 245, 248, 255 } };
    if (fg.bRealNames && id >= 0 && id < 17) { *r = aReal[id][0]; *g = aReal[id][1]; *b = aReal[id][2]; return; }
    *r = (unsigned char)(id * 15); *g = 100; *b = 200;
}
static int F_SaberStyleAvailable(int st) { return st == 0 || (st == 1 && fg.bDualOk); }
static const char* F_SaberStyleName(int st) { return st == 0 ? "SINGLE" : st == 1 ? "DUAL" : NULL; }
static int F_SaberStyleNumBlades(int st) { return st == 1 ? 2 : st == 0 ? 1 : 0; }
static int F_SaberGetStyle(void) { return fg.saberStyle; }
static void F_SaberSetStyle(int st) { if (F_SaberStyleAvailable(st)) fg.saberStyle = st; }
// (as iosSaber's: a style's blade is a hand, its first blade's colour; setting it colours the whole hand, linked)
static int F_SaberGetColour(int st, int b) { return (st >= 0 && st <= 1) ? fg.aBladeColour[b & 1][0] : 0; }
static void F_SaberSetColour(int st, int b, int id) { if (st >= 0 && st <= 1) { fg.aBladeColour[b & 1][0] = fg.aBladeColour[b & 1][1] = id; fg.aUnlinked[b & 1] = 0; } }
static const char* F_SaberHiltName(int h) { return h == 0 ? "NORMAL" : h == 1 ? "CROSS" : h == 2 ? "STAFF" : h == 3 ? "FOLD" : NULL; }
static int F_SaberHiltAvailable(int h) { return h == 0 || (h == 1 && fg.bCrossOk) || (h == 2 && fg.bStaffOk) || (h == 3 && fg.bFoldOk); }
static int F_SaberGetHilt(void) { return fg.aHilt[0]; }
static void F_SaberSetHilt(int h) { if (F_SaberHiltAvailable(h)) fg.aHilt[0] = fg.aHilt[1] = h; }
static const char* F_SaberLookName(int lk) { return lk == 0 ? "CLASSIC" : lk == 1 ? "REFORGED" : NULL; }
static int F_SaberGetLook(void) { return fg.saberLook; }
static void F_SaberSetLook(int lk) { if (lk == 0 || lk == 1) fg.saberLook = lk; }
static const char* F_SaberShapeName(int sh) { return sh == 0 ? "SPIKE" : sh == 1 ? "BEAM" : NULL; }
static int F_SaberGetShape(void) { return fg.saberShape; }
static void F_SaberSetShape(int sh) { if (sh == 0 || sh == 1) fg.saberShape = sh; }
static int F_SaberGetThrow(void) { return fg.bThrow; }
static void F_SaberSetThrow(int b) { fg.bThrow = b != 0; }
static int F_CaptureRequest(int kinds, int mode)
{
    if (fg.bCaptureFull) return 0;
    fg.captures++;
    fg.lastKinds = kinds;
    fg.lastMode = mode;
    return 1;
}
static void F_CaptureCancel(void) { fg.captureCancels++; }
static void F_SetPhotoBatch(int b) { fg.batch = b; fg.batchSets++; }
static int F_SelfieStart(int n) { fg.selfieShots = n; if (fg.selfieResult == 1) fg.bSelfie = 1; return fg.selfieResult; }
static void F_SelfieStop(void) { fg.bSelfie = 0; fg.selfieStops++; }
static int F_SelfieIsActive(void) { return fg.bSelfie; }
static int F_FreezeBegin(void) { if (fg.bFreezeOk) fg.bFreeze = 1; return fg.bFreezeOk; }
static void F_FreezeEnd(void) { fg.bFreeze = 0; fg.freezeEnds++; }
static int F_FreezeIsActive(void) { return fg.bFreeze; }
static void F_FreezeSetSmooth(int b) { fg.bSmooth = b; }
static int F_FreezeOrbit(float y, float p) { fg.orbitYaw += y; fg.orbitPitch += p; return 1; }
static void F_FreezePan(float x, float y) { fg.panX += x; fg.panY += y; }
static void F_FreezeZoom(float f) { fg.zoom *= f; }
static int F_CinemaStart(void) { if (fg.cinemaResult == 1) fg.bCinema = 1; return fg.cinemaResult; }
static void F_CinemaStop(void) { fg.bCinema = 0; fg.cinemaStops++; }
static int F_CinemaIsActive(void) { return fg.bCinema; }

static const touchUiGame fakeGame = {
    F_IsMots, F_GetForcePowerName, F_IsPowerAvailable, F_GetPowerLevel, F_GetPowerName, F_GetCurPower, F_SelectPower,
    F_GetWeaponName, F_GetWeapon, F_GetCurWeapon, F_SelectWeapon, F_CancelWeaponPick, F_GetItem, F_SelectItem,
    F_QuickLoad, F_SetHold, F_IsHolding, F_IsChatOpen, F_ToggleChat, F_GetForceMana, F_GetFrameCount, F_IsAlwaysRun,
    F_GetMouseLookDegrees, F_CanFly, F_ToggleFlight, F_IsFlying, F_CanNoclip, F_ToggleNoclip, F_NumPickableLevels,
    F_GoToLevel, F_IsSithEwokOn, F_ToggleSithEwok, F_TakeSithEwokTypedOff, F_SetCheatShown, F_SetThirdPerson,
    F_IsThirdPerson, F_SetForceJumpStyle, F_GetForceJumpRank, F_ShowMessage, F_ShowMessage, F_GetGaugeRect,
    F_IsSaberButtonShown, F_SaberNumColours, F_SaberColourName, F_SaberColourKey, F_SaberColourByKey, F_SaberColourRGB,
    F_SaberStyleAvailable, F_SaberStyleName, F_SaberStyleNumBlades, F_SaberGetStyle, F_SaberSetStyle, F_SaberGetColour,
    F_SaberSetColour, F_SaberLookName, F_SaberGetLook, F_SaberSetLook, F_SaberShapeName, F_SaberGetShape, F_SaberSetShape, F_SaberGetThrow, F_SaberSetThrow,
    F_SaberHiltName, F_SaberHiltAvailable, F_SaberGetHilt, F_SaberSetHilt, F_CaptureRequest, F_CaptureCancel, F_SetPhotoBatch, F_SelfieStart, F_SelfieStop, F_SelfieIsActive,
    F_FreezeBegin, F_FreezeEnd, F_FreezeIsActive, F_FreezeSetSmooth, F_FreezeOrbit, F_FreezePan, F_FreezeZoom,
    F_CinemaStart, F_CinemaStop, F_CinemaIsActive,
};

// The same fake game as the engine's calls, for the wheels (inputWheel.c)
int iosGame_IsMots(void) { return F_IsMots(); }
int iosGame_IsPowerAvailable(int bin) { return F_IsPowerAvailable(bin); }
int iosGame_GetPowerLevel(int bin) { return F_GetPowerLevel(bin); }
const char* iosGame_GetPowerName(int bin) { return F_GetPowerName(bin); }
int iosGame_GetCurPower(void) { return F_GetCurPower(); }
void iosGame_SelectPower(int bin) { F_SelectPower(bin); }
const char* iosGame_GetWeaponName(int bin) { return F_GetWeaponName(bin); }
int iosGame_GetWeapon(int bin, int* pAmmo, int* pSel) { return F_GetWeapon(bin, pAmmo, pSel); }
int iosGame_GetCurWeapon(void) { return F_GetCurWeapon(); }
int iosGame_SelectWeapon(int bin) { return F_SelectWeapon(bin); }
void iosGame_SetHold(int b) { F_SetHold(b); }
int iosGame_IsSaberButtonShown(void) { return F_IsSaberButtonShown(); }
int iosSaber_NumColours(void) { return F_SaberNumColours(); }
// (as iosSaber.c: one colour, 7 here as GREEN is there, is off Jedi Knight's wheel in
// the REFORGED look; each look's swatches its own, here by their green)
int iosSaber_ColourOnWheel(int id) { return id >= 0 && id < F_SaberNumColours() && !(id == 7 && !F_IsMots() && fg.saberLook == 1); }
int iosSaber_GetLook(void) { return F_SaberGetLook(); }
void iosSaber_SetLook(int lk) { F_SaberSetLook(lk); }
void iosSaber_ToggleLook(void) { F_SaberSetLook(fg.saberLook ? 0 : 1); }
const char* iosSaber_LookName(int lk) { return F_SaberLookName(lk); }
// (iosSaberShape, as the shared wheel asks it)
int iosSaberShape_Get(void) { return F_SaberGetShape(); }
void iosSaberShape_Toggle(void) { F_SaberSetShape(fg.saberShape ? 0 : 1); }
// (iosSaberThrowJk: THROW, offered in Jedi Knight's single player; Saber Throw on the
// Force wheel with it or with the cheat)
int iosSaberThrowJk_Get(void) { return F_SaberGetThrow(); }
void iosSaberThrowJk_Toggle(void) { F_SaberSetThrow(!fg.bThrow); }
int iosSaberThrowJk_Offered(void) { return !F_IsMots() && !fg.bThrowHidden; }
int iosSaberThrowJk_OnWheel(void) { return iosSaberThrowJk_Offered() && (fg.bThrow || fg.bCheat); }
int iosSaberThrowJk_CheatHeld(void) { return iosSaberThrowJk_Offered() && fg.bCheat; }
int iosSaber_GetShownColour(int st, int b) { const int id = F_SaberGetColour(st, b); return iosSaber_ColourOnWheel(id) ? id : 0; }
const char* iosSaber_ColourName(int id) { return F_SaberColourName(id); }
void iosSaber_ColourRGB(int id, unsigned char* r, unsigned char* g, unsigned char* b) { F_SaberColourRGB(id, r, g, b); if (!fg.saberLook && !fg.bRealNames) *g = 50; }
int iosSaber_StyleAvailable(int st) { return F_SaberStyleAvailable(st); }
int iosSaber_StyleNumBlades(int st) { return F_SaberStyleNumBlades(st); }
// (the hilts as iosSaber.c has them: HILT goes round the available ones; CROSS's blades keep their own shape)
int iosSaber_HiltAvailable(int h) { return F_SaberHiltAvailable(h); }
const char* iosSaber_HiltName(int h) { return F_SaberHiltName(h); }
int iosSaber_HiltKeepsShape(int h) { return h == 1; }
int iosSaber_GetHilt(void) { return F_SaberGetHilt(); }
void iosSaber_SetHilt(int h) { F_SaberSetHilt(h); }
int iosSaber_NextHilt(int h) { for (int k = 1; k < 4; k++) if (F_SaberHiltAvailable((h + k) % 4)) return (h + k) % 4; return h; }
int iosSaber_GetStyle(void) { return F_SaberGetStyle(); }
void iosSaber_SetStyle(int st) { F_SaberSetStyle(st); }
int iosSaber_GetColour(int st, int b) { return F_SaberGetColour(st, b); }
void iosSaber_SetColour(int st, int b, int id) { F_SaberSetColour(st, b, id); }
// (each hand's saber, as iosSaber.c keeps it; for iosSaberWheel)
const char* iosSaber_StyleName(int st) { return F_SaberStyleName(st); }
int iosSaber_StyleByName(const char* n, int* pHilt)
{
    if (!n) return -1;
    if (!strcmp(n, "SINGLE")) return 0;
    if (!strcmp(n, "DUAL")) return 1;
    if (!strcmp(n, "CROSS")) { if (pHilt) *pHilt = 1; return 0; }
    return -1;
}
int iosSaber_HiltByName(const char* n) { for (int h = 0; n && h < 4; h++) if (!strcmp(n, F_SaberHiltName(h))) return h; return -1; }
int iosSaber_HiltNumBlades(int h) { return (h == 2 || h == 3) ? 2 : 1; }
int iosSaber_GetHandHilt(int hand) { return (hand == 0 || hand == 1) ? fg.aHilt[hand] : 0; }
void iosSaber_SetHandHilt(int hand, int h)
{
    if ((hand != 0 && hand != 1) || !F_SaberHiltAvailable(h)) return;
    if (iosSaber_HiltNumBlades(h) > 1 && iosSaber_HiltNumBlades(fg.aHilt[hand]) < 2) fg.aUnlinked[hand] = 0;
    fg.aHilt[hand] = h;
}
int iosSaber_GetBladeColour(int hand, int b) { if ((hand != 0 && hand != 1) || (b != 0 && b != 1)) return 0; return fg.aBladeColour[hand][fg.aUnlinked[hand] ? b : 0]; }
void iosSaber_SetBladeColour(int hand, int b, int id) { if ((hand == 0 || hand == 1) && (b == 0 || b == 1) && id >= 0 && id < 17) fg.aBladeColour[hand][b] = id; }
int iosSaber_GetBladeLinked(int hand) { return (hand == 0 || hand == 1) ? !fg.aUnlinked[hand] : 1; }
void iosSaber_SetBladeLinked(int hand, int b) { if (hand == 0 || hand == 1) fg.aUnlinked[hand] = !b; }
int iosSaber_GetShownBladeColour(int hand, int b) { const int id = iosSaber_GetBladeColour(hand, b); return iosSaber_ColourOnWheel(id) ? id : 0; }
const char* iosSaber_ColourKey(int id) { return F_SaberColourKey(id); }
int iosSaber_ColourByKey(const char* k) { return k ? F_SaberColourByKey(k) : -1; }
// (iosSaberFold's: AUTO, how each hand's was left, the SABER tap's flips: with a FOLD in each hand the
// right one, or the left one as the quick second tap; one in hand, that one -- a FOLD lit with no FOLD
// hilt set here: the right hand's)
int iosSaberFold_GetAuto(void) { return fg.bFoldAuto; }
void iosSaberFold_SetAuto(int b) { fg.bFoldAuto = b != 0; }
int iosSaberFold_GetSaberHeldOpen(int k) { return k == 0 ? fg.bFoldOpen : (k == 1 ? fg.bFoldOpenL : 0); }
void iosSaberFold_SetSaberHeldOpen(int k, int b) { if (fg.bFoldAuto) return; if (k == 0) fg.bFoldOpen = b != 0; else if (k == 1) fg.bFoldOpenL = b != 0; }
int iosSaberFold_GetHeldOpen(void) { return fg.bFoldOpen; }
void iosSaberFold_SetHeldOpen(int b) { iosSaberFold_SetSaberHeldOpen(0, b); iosSaberFold_SetSaberHeldOpen(1, b); }
int iosSaberFold_CanFlip(void) { return fg.bFoldLit; }
static int fgFoldHands(void)
{
    if (!fg.bFoldLit) return 0;
    const int m = (fg.aHilt[0] == 3 ? 1 : 0) | ((fg.saberStyle == 1 && fg.aHilt[1] == 3) ? 2 : 0);
    return m ? m : 1;
}
static void fgFlipHands(int m) { if (m & 1) fg.bFoldOpen = !fg.bFoldOpen; if (m & 2) fg.bFoldOpenL = !fg.bFoldOpenL; fg.foldFlips++; }
int iosSaberFold_Flip(void) { const int m = fgFoldHands(); if (!m) return 0; fgFlipHands(m); return 1; }
int iosSaberFold_TapFlip(int bAgain)
{
    const int m = fgFoldHands();
    if (!m) return 0;
    if (m != 3) { fgFlipHands(m); return 1; }
    fgFlipHands(bAgain ? 2 : 1);
    return bAgain ? 1 : 2;
}
int iosSaberFold_KeyFlip(void) { return iosSaberFold_TapFlip(0) != 0; }
int iosSaberFold_LocalOpen(int hand) { return (hand == 0 || hand == 1) && fg.aHilt[hand] == 3 && fg.bFoldLit && (hand ? fg.bFoldOpenL : fg.bFoldOpen); }
static int g_platHaptics = 0, g_platTicks = 0;
void platServices_Haptic(int kind) { g_platHaptics |= kind == 0 ? TOUCHUI_HAPTIC_TICK : TOUCHUI_HAPTIC_TAP; if (kind == 0) g_platTicks++; }

// What the hand felt since last asked: the overlay's own, and the wheel's
static int takeHaptics(void)
{
    const int h = touchUi_TakeHaptics() | g_platHaptics;
    g_platHaptics = 0;
    return h;
}

// ------------------------------------------------------------ the harness

static int g_fails = 0, g_checks = 0;
static const char* g_case = "";
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fails++; printf("FAIL [%s] line %d: %s\n", g_case, __LINE__, #cond); } } while (0)
#define CHECKF(a, b, eps) do { g_checks++; if (fabs((double)(a) - (double)(b)) > (eps)) { g_fails++; printf("FAIL [%s] line %d: %s = %g, want %g\n", g_case, __LINE__, #a, (double)(a), (double)(b)); } } while (0)

enum { SC_A = 4, SC_C = 6, SC_D = 7, SC_E = 8, SC_G = 10, SC_S = 22, SC_W = 26, SC_X = 27, SC_RETURN = 40, SC_F9 = 66, SC_LCTRL = 224, SC_LSHIFT = 225 };

static const char* g_settings = "/tmp/touchUi_test_settings.cfg";
static double T = 0.0;

static void setup_screen(float w, float h, float l, float t, float r, float b)
{
    touchUi_SetScreen(w, h, l, t, r, b);
}

static void setup(void)
{
    memset(&fg, 0, sizeof(fg));
    fg.saberLook = 0; // (CLASSIC, the default)
    fg.bFoldAuto = 1;
    fg.curPower = -1;
    fg.curWeapon = 2;
    // Jedi Knight: every power learned, at levels 1..4 round
    for (int bin = 21; bin <= 34; bin++) fg.aPowerLevel[bin] = 1 + (bin % 4);
    fg.aWeaponOwned[1] = fg.aWeaponSel[1] = 1; fg.aWeaponAmmo[1] = -1;
    fg.aWeaponOwned[2] = fg.aWeaponSel[2] = 1; fg.aWeaponAmmo[2] = 120;
    fg.aWeaponOwned[3] = 1; fg.aWeaponSel[3] = 0; fg.aWeaponAmmo[3] = 0; // no ammo
    fg.bDualOk = 1;
    fg.bCrossOk = 1;
    fg.selfieResult = fg.cinemaResult = 1;
    fg.bFreezeOk = 1;
    fg.zoom = 1.0f;
    remove(g_settings);
    if (inputWheel_IsOpen()) inputWheel_Close(-1); // (a case that ended with one open)
    touchUi_Init(&fakeGame, g_settings);
    touchUi_SetGyroAvailable(1);
    setup_screen(852, 393, 59, 0, 59, 21); // an iPhone-sized screen, as the standard was made on
    touchUi_SetShown(1);
    T = 100.0;
    touchUi_Tick(T, T);
    takeHaptics();
}

static void bxy(int btn, float* x, float* y)
{
    float r;
    touchUi_GetButton(btn, x, y, &r);
}

static void down(int id, float x, float y) { touchUi_Finger(id, TOUCHUI_PHASE_DOWN, x, y, T); }
static void move(int id, float x, float y) { touchUi_Finger(id, TOUCHUI_PHASE_MOVE, x, y, T); }
static void up(int id, float x, float y) { touchUi_Finger(id, TOUCHUI_PHASE_UP, x, y, T); }
static void cancel(int id) { touchUi_Finger(id, TOUCHUI_PHASE_CANCEL, 0, 0, T); }
static void tick(void) { touchUi_Tick(T, T); }
// time goes on, the frames ticking every 1/60 s
static void wait(double secs)
{
    double end = T + secs;
    while (T + 1.0 / 60.0 < end - 1e-9) {
        T += 1.0 / 60.0;
        tick();
    }
    T = end;
    tick();
}

// The game reading the keyboard n times: how many separate presses of sc it sees
static int presses(int sc, int n)
{
    int count = 0, was = 0;
    for (int i = 0; i < n; i++) {
        int d = touchUi_IsScancodeDown(sc);
        if (d && !was) count++;
        was = d;
    }
    return count;
}

static void downOn(int id, int btn) { float x, y; bxy(btn, &x, &y); down(id, x, y); }
static void upOn(int id, int btn) { float x, y; bxy(btn, &x, &y); up(id, x, y); }

// a point at deg (counter-clockwise from right) and dist from (x, y)
static void polar(float x, float y, float deg, float dist, float* px, float* py)
{
    *px = x + dist * cosf(deg * 3.14159265f / 180.0f);
    *py = y - dist * sinf(deg * 3.14159265f / 180.0f);
}

// ------------------------------------------------------------ the cases

static void test_stick(void)
{
    g_case = "stick: walks where the left thumb lands";
    setup();
    down(1, 150, 300);
    CHECK(touchUi_IsStickActive());
    move(1, 150, 290); // inside the dead zone (30% of 60)
    CHECK(!touchUi_IsKeyHeld(SC_W));
    move(1, 150, 230); // up 70 points: pushed to the edge
    CHECK(touchUi_IsKeyHeld(SC_W) && !touchUi_IsKeyHeld(SC_S));
    move(1, 220, 300);
    CHECK(touchUi_IsKeyHeld(SC_D) && !touchUi_IsKeyHeld(SC_W));
    move(1, 80, 340);
    CHECK(touchUi_IsKeyHeld(SC_A) && touchUi_IsKeyHeld(SC_S));
    up(1, 80, 340);
    CHECK(!touchUi_IsStickActive() && !touchUi_IsKeyHeld(SC_A) && !touchUi_IsKeyHeld(SC_S));

    g_case = "stick: only in the left 42% of the screen";
    down(2, 852 * 0.43f, 300);
    CHECK(!touchUi_IsStickActive());
    up(2, 852 * 0.43f, 300);

    g_case = "stick: run marker (Always Run off): push up past the ring onto it";
    down(1, 200, 330);
    move(1, 200, 254); // 76 up, straight: on the marker (100 above, run from 70)
    CHECK(touchUi_IsStickRunning());
    CHECK(touchUi_IsKeyHeld(SC_W) && touchUi_IsKeyHeld(SC_LSHIFT));
    CHECK(takeHaptics() & TOUCHUI_HAPTIC_TICK);
    move(1, 200, 268); // 62: still running (stops under 60)
    CHECK(touchUi_IsStickRunning());
    move(1, 200, 272); // 58: walking again
    CHECK(!touchUi_IsStickRunning() && touchUi_IsKeyHeld(SC_W) && !touchUi_IsKeyHeld(SC_LSHIFT));
    float px, py;
    polar(200, 330, 90 + 35, 80, &px, &py); // 35 degrees off: no
    move(1, px, py);
    CHECK(!touchUi_IsStickRunning());
    up(1, px, py);

    g_case = "stick: Always Run on: no marker";
    fg.bAlwaysRun = 1;
    tick();
    down(1, 200, 330);
    move(1, 200, 240);
    CHECK(!touchUi_IsStickRunning() && !touchUi_IsKeyHeld(SC_LSHIFT));
    up(1, 200, 240);
}

static void test_look(void)
{
    g_case = "look: a drag elsewhere turns the view (1.6 / 1.3 per point)";
    setup();
    int dx, dy;
    touchUi_TakeLook(&dx, &dy);
    down(1, 500, 200);
    move(1, 510, 210);
    touchUi_TakeLook(&dx, &dy);
    CHECK(dx == 16 && dy == 13);
    move(1, 500, 200);
    touchUi_TakeLook(&dx, &dy);
    CHECK(dx == -16 && dy == -13);
    up(1, 500, 200);

    g_case = "look: a drag on FIRE fires and looks";
    float fx, fy;
    bxy(TOUCHUI_BTN_FIRE, &fx, &fy);
    down(2, fx, fy);
    CHECK(touchUi_IsKeyHeld(SC_LCTRL));
    move(2, fx - 20, fy);
    touchUi_TakeLook(&dx, &dy);
    CHECK(dx == -32);
    CHECK(touchUi_IsKeyHeld(SC_LCTRL));
    up(2, fx - 20, fy);
    CHECK(!touchUi_IsKeyHeld(SC_LCTRL));
}

static void test_buttons(void)
{
    static const struct { int btn; int sc; } aCase[] = {
        { TOUCHUI_BTN_FIRE, SC_LCTRL }, { TOUCHUI_BTN_ALT, 29 }, { TOUCHUI_BTN_DUCK, SC_C },
        { TOUCHUI_BTN_ACT, 44 }, { TOUCHUI_BTN_JUMP, SC_X }, { TOUCHUI_BTN_FORCE, 9 },
    };
    setup();
    for (int i = 0; i < (int)(sizeof(aCase) / sizeof(aCase[0])); i++) {
        g_case = "cluster buttons hold their keys";
        downOn(1, aCase[i].btn);
        CHECK(touchUi_IsKeyHeld(aCase[i].sc));
        CHECK(touchUi_IsScancodeDown(aCase[i].sc));
        upOn(1, aCase[i].btn);
        CHECK(!touchUi_IsKeyHeld(aCase[i].sc));
    }
    g_case = "a tap the game never read still counts once";
    downOn(1, TOUCHUI_BTN_JUMP);
    upOn(1, TOUCHUI_BTN_JUMP);
    CHECK(presses(SC_X, 6) == 1);
}

static void test_quicksave(void)
{
    float qx, qy, lx, ly;
    setup();
    bxy(TOUCHUI_BTN_QUICKSAVE, &qx, &qy);
    bxy(TOUCHUI_BTN_QUICKLOAD, &lx, &ly);

    g_case = "QUICK SAVE: lifted at 0.29 s does nothing";
    down(1, qx, qy);
    wait(0.29);
    CHECKF(touchUi_GetRingProgress(TOUCHUI_BTN_QUICKSAVE), 0.29 / 0.3, 0.02);
    up(1, qx, qy);
    tick();
    CHECK(presses(SC_F9, 8) == 0);
    CHECK(touchUi_GetRingProgress(TOUCHUI_BTN_QUICKSAVE) == 0.0f);

    g_case = "QUICK SAVE: a 0.3 s hold saves once, the finger still down";
    down(1, qx, qy);
    wait(0.15);
    CHECKF(touchUi_GetRingProgress(TOUCHUI_BTN_QUICKSAVE), 0.5, 0.02);
    wait(0.16);
    CHECK(presses(SC_F9, 8) == 1);
    wait(1.0);
    CHECK(presses(SC_F9, 8) == 0);
    up(1, qx, qy);

    g_case = "QUICK SAVE: slid off and back on: nothing";
    down(1, qx, qy);
    wait(0.1);
    move(1, qx + 40, qy);
    move(1, qx, qy);
    wait(0.5);
    CHECK(presses(SC_F9, 8) == 0);
    up(1, qx, qy);

    g_case = "QUICK SAVE: two fingers save once";
    down(1, qx - 3, qy);
    wait(0.1);
    down(2, qx + 3, qy);
    wait(0.5);
    CHECK(presses(SC_F9, 12) == 1);
    up(1, qx, qy);
    up(2, qx, qy);

    g_case = "QUICK LOAD: a 0.3 s hold loads once";
    down(1, lx, ly);
    wait(0.29);
    CHECK(fg.quickLoads == 0);
    wait(0.02);
    CHECK(fg.quickLoads == 1);
    wait(0.5);
    CHECK(fg.quickLoads == 1);
    up(1, lx, ly);

    g_case = "QUICK SAVE and QUICK LOAD together: QUICK SAVE wins a tie";
    fg.quickLoads = 0;
    down(1, lx, ly);
    down(2, qx, qy);
    T += 0.4; // one long frame covers both
    tick();
    CHECK(presses(SC_F9, 8) == 1);
    CHECK(fg.quickLoads == 0);
    up(1, lx, ly);
    up(2, qx, qy);

    g_case = "QUICK SAVE taken by the system: nothing";
    down(1, qx, qy);
    wait(0.1);
    cancel(1);
    wait(0.5);
    CHECK(presses(SC_F9, 8) == 0);

    g_case = "QUICK SAVE while typing: nothing";
    fg.bChat = 1;
    tick();
    down(1, qx, qy);
    wait(0.5);
    CHECK(presses(SC_F9, 8) == 0);
    up(1, qx, qy);
    fg.bChat = 0;
}

static void test_menu(void)
{
    float mx, my;
    setup();
    bxy(TOUCHUI_BTN_MENU, &mx, &my);

    g_case = "MENU: a tap is the game menu (Escape, two frames)";
    down(1, mx, my);
    wait(0.1);
    up(1, mx, my);
    CHECK(touchUi_TakeEscape() == 1);
    CHECK(touchUi_TakeEscape() == 1);
    CHECK(touchUi_TakeEscape() == 0);
    CHECK(!touchUi_IsTrayOpen());

    g_case = "MENU: held 0.44 s: no tray yet";
    down(1, mx, my);
    wait(0.44);
    CHECK(!touchUi_IsTrayOpen());
    g_case = "MENU: held 0.45 s: the tray, and no menu";
    wait(0.02);
    CHECK(touchUi_IsTrayOpen());
    up(1, mx, my);
    CHECK(touchUi_TakeEscape() == 0);

    g_case = "tray: FPS shows the readout and closes the tray";
    CHECK(!touchUi_IsFpsShown());
    downOn(2, TOUCHUI_BTN_TRAYFPS);
    upOn(2, TOUCHUI_BTN_TRAYFPS);
    CHECK(touchUi_IsFpsShown());
    CHECK(!touchUi_IsTrayOpen());

    g_case = "tray: a touch off it closes it";
    down(1, mx, my); wait(0.5); up(1, mx, my);
    CHECK(touchUi_IsTrayOpen());
    down(2, 400, 250);
    CHECK(!touchUi_IsTrayOpen());
    up(2, 400, 250);

    g_case = "tray: the keyboard opens the typing line";
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYKEYS);
    upOn(2, TOUCHUI_BTN_TRAYKEYS);
    CHECK(fg.chatToggles == 1 && fg.bChat);
    fg.bChat = 0;

    g_case = "tray: slide from MENU's hold onto a tray button and lift";
    down(1, mx, my);
    wait(0.5);
    float fx, fy;
    bxy(TOUCHUI_BTN_TRAYFPS, &fx, &fy);
    move(1, fx, fy);
    up(1, fx, fy);
    CHECK(!touchUi_IsFpsShown()); // (switched back off)
    CHECK(touchUi_TakeEscape() == 0);
}

static void test_gyro_settings(void)
{
    float mx, my;
    setup();
    bxy(TOUCHUI_BTN_MENU, &mx, &my);

    g_case = "GYRO: OFF to start, SENS 1.5";
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_OFF);
    CHECKF(touchUi_GetGyroSens(), 1.5, 1e-6);

    g_case = "GYRO: a tap switches it on (TOUCH), the tray stays";
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYGYRO);
    wait(0.1);
    upOn(2, TOUCHUI_BTN_TRAYGYRO);
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_TOUCH);
    CHECK(touchUi_IsTrayOpen());

    g_case = "GYRO held 0.45 s: its options (MODE, SENS)";
    downOn(2, TOUCHUI_BTN_TRAYGYRO);
    wait(0.5);
    CHECK(touchUi_IsGyroRowOpen());
    upOn(2, TOUCHUI_BTN_TRAYGYRO);
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_TOUCH); // (the hold's lift doesn't switch it)

    g_case = "SENS: 1.5 -> 2.0 -> 3.0 -> 1.0 -> 1.5";
    static const float aWant[] = { 2.0f, 3.0f, 1.0f, 1.5f };
    for (int i = 0; i < 4; i++) {
        downOn(3, TOUCHUI_BTN_GYROOPT_SENS);
        upOn(3, TOUCHUI_BTN_GYROOPT_SENS);
        CHECKF(touchUi_GetGyroSens(), aWant[i], 1e-6);
        CHECK(touchUi_IsGyroRowOpen());
    }
    g_case = "MODE: TOUCH -> ALWAYS";
    downOn(3, TOUCHUI_BTN_GYROOPT_MODE);
    upOn(3, TOUCHUI_BTN_GYROOPT_MODE);
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_ALWAYS);
    downOn(3, TOUCHUI_BTN_GYROOPT_SENS);
    upOn(3, TOUCHUI_BTN_GYROOPT_SENS); // 2.0

    g_case = "settings remembered (mode, SENS, FPS)";
    touchUi_Init(&fakeGame, g_settings);
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_ALWAYS);
    CHECKF(touchUi_GetGyroSens(), 2.0, 1e-6);
}

static void gyro_frames(int n, double rate, int* pSum)
{
    *pSum = 0;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < 4; k++) {
            T += 0.004;
            // turning left about up, held upright landscape
            touchUi_GyroSample(0.0, rate, 0.0, 0.0, 1.0, 0.0, T);
        }
        tick();
        int dx, dy;
        touchUi_GyroFrame(T, &dx, &dy);
        *pSum += dx;
    }
}

static void test_gyro_aim(void)
{
    int sum;
    setup();
    touchUi_GyroSetSide(1, T - 10.0);

    g_case = "GYRO OFF: never aims";
    gyro_frames(30, 1.0, &sum);
    CHECK(sum == 0);

    g_case = "GYRO TOUCH: aims while a thumb is down";
    float mx, my;
    bxy(TOUCHUI_BTN_MENU, &mx, &my);
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYGYRO); upOn(2, TOUCHUI_BTN_TRAYGYRO);
    downOn(2, TOUCHUI_BTN_FIRE); upOn(2, TOUCHUI_BTN_FIRE); // (closes the tray, touching off it)
    CHECK(touchUi_GetGyroMode() == TOUCHUI_GYRO_TOUCH && !touchUi_IsTrayOpen());
    gyro_frames(30, 1.0, &sum);
    CHECK(sum == 0); // no thumb down
    down(3, 500, 200); // on the look area
    gyro_frames(30, 1.0, &sum);
    // 1 rad/s for 0.48 s at 1.5x = 41 degrees, at 0.1 degrees a count: about -410 (turning left)
    CHECK(sum < -300);
    g_case = "GYRO TOUCH: lifting both thumbs freezes the view";
    up(3, 500, 200);
    gyro_frames(30, 1.0, &sum);
    CHECK(sum == 0);
    g_case = "GYRO TOUCH: the stick counts as a thumb";
    down(4, 150, 300);
    gyro_frames(30, 1.0, &sum);
    CHECK(sum < -300);
    g_case = "GYRO: not while a wheel is open";
    downOn(5, TOUCHUI_BTN_WHEEL);
    wait(0.35);
    CHECK(touchUi_IsWheelOpen());
    gyro_frames(30, 1.0, &sum);
    CHECK(sum == 0);
    up(5, 0, 0);
    up(4, 150, 300);
}

static void test_wheels(void)
{
    float wx, wy, cx, cy, r0, r1, deg;
    setup();
    bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);

    g_case = "FORCE WHEEL: a tap is the next power (E)";
    down(1, wx, wy);
    wait(0.1);
    up(1, wx, wy);
    CHECK(!touchUi_IsWheelOpen());
    CHECK(presses(SC_E, 6) == 1);

    g_case = "FORCE WHEEL: still 0.29 s, lifted: a tap";
    down(1, wx, wy);
    T += 0.29; // (lifted between two frames)
    up(1, wx, wy);
    CHECK(!touchUi_IsWheelOpen());
    CHECK(presses(SC_E, 6) == 1);

    g_case = "FORCE WHEEL: a 9.5 pt slide doesn't open it";
    down(1, wx, wy);
    move(1, wx + 9.5f, wy);
    CHECK(!touchUi_IsWheelOpen());
    up(1, wx + 9.5f, wy);
    presses(SC_E, 6);

    g_case = "FORCE WHEEL: a 10 pt slide opens it at once; the game holds still";
    down(1, wx, wy);
    move(1, wx + 10.0f, wy);
    CHECK(touchUi_IsWheelOpen() && !touchUi_IsWheelTapMode());
    CHECK(fg.bHold == 1);
    CHECK(touchUi_GetWheelKind() == TOUCHUI_WHEEL_FORCE);
    g_case = "FORCE WHEEL: slide toward a power and lift: selected";
    int slice = touchUi_GetWheelSlice(32, &deg); // (Lightning)
    CHECK(slice >= 0);
    float px, py;
    polar(wx, wy, deg, 40, &px, &py);
    move(1, px, py);
    CHECK(touchUi_GetWheelHot() == slice);
    up(1, px, py);
    CHECK(!touchUi_IsWheelOpen());
    CHECK(fg.selectedPower == 32);
    CHECK(fg.bHold == 0);
    CHECK(presses(SC_E, 6) == 0);

    g_case = "FORCE WHEEL: a haptic tick per slice crossed";
    down(1, wx, wy);
    move(1, wx + 10.0f, wy);
    takeHaptics();
    int ticks = 0;
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    for (float a = 240.0f; a >= -60.0f; a -= 2.0f) { // round the ring on the wheel itself
        polar(cx, cy, a, (r0 + r1) * 0.5f, &px, &py);
        move(1, px, py);
        if (takeHaptics() & TOUCHUI_HAPTIC_TICK) ticks++;
    }
    CHECK(ticks == 14); // Jedi Knight's 14 powers, one tick each
    g_case = "FORCE WHEEL: the gap at the bottom cancels";
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    move(1, px, py);
    CHECK(touchUi_GetWheelHot() == -2);
    fg.selectedPower = 0;
    up(1, px, py);
    CHECK(!touchUi_IsWheelOpen() && fg.selectedPower == 0 && fg.bHold == 0);

    g_case = "FORCE WHEEL: lifted back where it started: no change";
    down(1, wx, wy);
    move(1, wx + 30.0f, wy);
    move(1, wx + 2.0f, wy);
    up(1, wx + 2.0f, wy);
    CHECK(!touchUi_IsWheelOpen() && fg.selectedPower == 0);

    g_case = "FORCE WHEEL: held 0.3 s opens it to tap";
    down(1, wx, wy);
    wait(0.28);
    CHECK(!touchUi_IsWheelOpen());
    CHECKF(touchUi_GetRingProgress(TOUCHUI_BTN_WHEEL), 0.28 / 0.3, 0.03);
    wait(0.03);
    CHECK(touchUi_IsWheelOpen() && touchUi_IsWheelTapMode());
    up(1, wx, wy);
    CHECK(touchUi_IsWheelOpen()); // stays open to tap
    slice = touchUi_GetWheelSlice(25, &deg); // (Healing)
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py);
    up(2, px, py);
    CHECK(!touchUi_IsWheelOpen() && fg.selectedPower == 25);

    g_case = "FORCE WHEEL: a tap in the middle closes it, no change";
    down(1, wx, wy); wait(0.35); up(1, wx, wy);
    fg.selectedPower = 0;
    down(2, cx, cy); up(2, cx, cy);
    CHECK(!touchUi_IsWheelOpen() && fg.selectedPower == 0);

    g_case = "FORCE WHEEL: four gold stars a power, filled to its level";
    down(1, wx, wy); wait(0.35); up(1, wx, wy);
    const touchUiDrawList* l = touchUi_Draw(T);
    int filledEdges = 0, emptyEdges = 0;
    for (int i = 0; i < l->numItems; i++) {
        const touchUiDrawItem* it = &l->aItems[i];
        if (it->type != TOUCHUI_DRAW_LINE) continue;
        if (it->stroke.r > 0.99f && it->stroke.g > 0.79f && it->stroke.g < 0.81f && it->stroke.a < 0.5f) emptyEdges++; // an empty star's outline (gold)
        if (it->stroke.r == 0.0f && it->stroke.a > 0.77f && it->stroke.a < 0.79f) filledEdges++;                    // a filled one's dark edge
    }
    int want = 0;
    for (int bin = 21; bin <= 34; bin++) want += fg.aPowerLevel[bin];
    CHECK(filledEdges == want * 10);
    CHECK(emptyEdges == (14 * 4 - want) * 10);
    down(2, cx, cy); up(2, cx, cy);

    g_case = "NEXT WPN: a tap is the next weapon (G)";
    float nx, ny;
    bxy(TOUCHUI_BTN_NEXTWPN, &nx, &ny);
    down(1, nx, ny); wait(0.05); up(1, nx, ny);
    CHECK(presses(SC_G, 6) == 1);
    g_case = "NEXT WPN: slide to a weapon and lift: selected";
    down(1, nx, ny);
    move(1, nx + 12, ny);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelKind() == TOUCHUI_WHEEL_WEAPONS);
    touchUi_GetWheelSlice(1, &deg); // fists
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    move(1, px, py);
    up(1, px, py);
    CHECK(fg.selectedWeapon == 1);
    g_case = "NEXT WPN: a weapon with no ammo can't be picked (the wheel stays)";
    down(1, nx, ny); wait(0.35); up(1, nx, ny);
    touchUi_GetWheelSlice(3, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    fg.selectedWeapon = 0;
    down(2, px, py); up(2, px, py);
    CHECK(touchUi_IsWheelOpen() && fg.selectedWeapon == 0);
    down(2, cx, cy); up(2, cx, cy);
    CHECK(!touchUi_IsWheelOpen());

    g_case = "wheel button taken by the system: nothing";
    down(1, wx, wy); wait(0.1); cancel(1);
    CHECK(presses(SC_E, 6) == 0 && !touchUi_IsWheelOpen());
}

static void test_items_cheats(void)
{
    setup();
    g_case = "BACTA: shows with a tank; a 0.3 s hold uses one";
    float bx, by;
    CHECK(!touchUi_GetButton(TOUCHUI_BTN_BACTA, &bx, &by, NULL));
    fg.aItem[40] = 2;
    tick();
    CHECK(touchUi_GetButton(TOUCHUI_BTN_BACTA, &bx, &by, NULL));
    down(1, bx, by);
    wait(0.2);
    up(1, bx, by);
    CHECK(presses(SC_RETURN, 6) == 0);
    down(1, bx, by);
    wait(0.31);
    CHECK(presses(SC_RETURN, 6) == 1 && fg.selectedItem == 40);
    up(1, bx, by);

    g_case = "CHEAT: the tray's switch shows it and puts sithewok on";
    float mx, my;
    bxy(TOUCHUI_BTN_MENU, &mx, &my);
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYCHEAT); upOn(2, TOUCHUI_BTN_TRAYCHEAT);
    tick();
    CHECK(touchUi_IsCheatShown() && fg.bCheat);
    g_case = "CHEAT: a tap does nothing; held 0.5 s it switches the cheat";
    float cx, cy;
    CHECK(touchUi_GetButton(TOUCHUI_BTN_CHEAT, &cx, &cy, NULL));
    down(1, cx, cy); wait(0.2); up(1, cx, cy);
    CHECK(fg.bCheat);
    down(1, cx, cy); wait(0.49);
    CHECK(fg.bCheat);
    wait(0.02);
    CHECK(!fg.bCheat);
    up(1, cx, cy);

    g_case = "JUMP held 0.5 s (sithewok): flight on; a double tap lands";
    fg.bCheat = 1;
    fg.bCanFly = 1;
    tick();
    downOn(1, TOUCHUI_BTN_JUMP); wait(0.51); upOn(1, TOUCHUI_BTN_JUMP);
    CHECK(fg.bFlying && fg.flightToggles == 1);
    downOn(1, TOUCHUI_BTN_JUMP); wait(0.1); upOn(1, TOUCHUI_BTN_JUMP);
    wait(0.1);
    downOn(1, TOUCHUI_BTN_JUMP); wait(0.1); upOn(1, TOUCHUI_BTN_JUMP);
    CHECK(!fg.bFlying && fg.flightToggles == 2);

    g_case = "VIEW: the tray's switch shows it; a tap is third person";
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYVIEW); upOn(2, TOUCHUI_BTN_TRAYVIEW);
    CHECK(touchUi_IsViewShown());
    downOn(2, TOUCHUI_BTN_VIEW); upOn(2, TOUCHUI_BTN_VIEW);
    CHECK(fg.bThird == 1);
}

// sithewok's level pick: the tray's LEVEL (right of its CHEAT switch, only
// while the cheat is on); DUCK only ever crouches
static void openTrayT(void) { float mx, my; bxy(TOUCHUI_BTN_MENU, &mx, &my); down(1, mx, my); wait(0.5); up(1, mx, my); }
static void closeTrayT(void) { if (touchUi_IsTrayOpen()) { down(9, 400, 250); up(9, 400, 250); tick(); } }
static void check_level_tray_layout(const char* name, float W, float H, float l, float t, float r, float b, int bSwap)
{
    g_case = name;
    setup();
    touchUi_SetScreen(W, H, l, t, r, b);
    if (bSwap) { openTrayT(); downOn(2, TOUCHUI_BTN_TRAYCTRL); upOn(2, TOUCHUI_BTN_TRAYCTRL); closeTrayT(); }
    float aOff[TOUCHUI_BTN_COUNT], x, y, rr, fx0, fy0;
    bxy(TOUCHUI_BTN_FORCE, &fx0, &fy0);
    openTrayT();
    CHECK(!touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &x, &y, &rr));
    for (int i = TOUCHUI_BTN_TRAYGYRO; i <= TOUCHUI_BTN_TRAYKEYS; i++) { touchUi_GetButton(i, &aOff[i], &y, &rr); }
    closeTrayT();
    fg.bCheat = 1; tick();
    float fx1, fy1; bxy(TOUCHUI_BTN_FORCE, &fx1, &fy1);
    CHECK(fx0 == fx1 && fy0 == fy1); // (FORCE never moves with the cheat)
    openTrayT();
    float lx, ly, cx, vx;
    CHECK(touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &lx, &ly, &rr));
    bxy(TOUCHUI_BTN_TRAYCHEAT, &cx, &y); bxy(TOUCHUI_BTN_TRAYVIEW, &vx, &y);
    CHECKF(lx - cx, 50.0, 0.5);
    CHECKF(vx - lx, 50.0, 0.5);
    float gx, kx, ky; bxy(TOUCHUI_BTN_TRAYGYRO, &gx, &y); bxy(TOUCHUI_BTN_TRAYKEYS, &kx, &ky);
    // its left end inside the safe area; its right end where it was without
    // LEVEL (the row grows to the left only)
    CHECK(gx - 26 >= (l > 8 ? l : 0) - 0.5f && ky - 26 >= t - 0.5f);
    CHECKF(kx, aOff[TOUCHUI_BTN_TRAYKEYS], 0.5);
    printf("  [%s] tray with LEVEL %.0f..%.0f (off: %.0f..%.0f)\n", name, gx - 26, kx + 26, aOff[TOUCHUI_BTN_TRAYGYRO] - 26, aOff[TOUCHUI_BTN_TRAYKEYS] + 26);
    // the moved message line with the tray and each row open: clear of every
    // button shown, down 70 pt (stacked lines), at least 120 pt wide
    for (int st = 0; st < 4; st++) {
        closeTrayT(); openTrayT();
        if (st == 1) { downOn(2, TOUCHUI_BTN_TRAYGYRO); wait(0.55); upOn(2, TOUCHUI_BTN_TRAYGYRO); }
        if (st >= 2) { downOn(2, TOUCHUI_BTN_TRAYCAM); wait(0.55); upOn(2, TOUCHUI_BTN_TRAYCAM); }
        if (st == 3) { downOn(2, TOUCHUI_BTN_CAMROW_PHOTO); upOn(2, TOUCHUI_BTN_CAMROW_PHOTO); }
        tick();
        float tt, tl, tr2;
        touchUi_GetTopTextRect(&tt, &tl, &tr2);
        float bx, by, br;
        const int aRowBtn[4] = { -1, TOUCHUI_BTN_GYROOPT_MODE, TOUCHUI_BTN_CAMROW_PHOTO, TOUCHUI_BTN_CAMOPT_CLEAN };
        if (st > 0) CHECK(touchUi_GetButton(aRowBtn[st], &bx, &by, &br));
        CHECK(tr2 - tl >= 120.0f);
        for (int i = 0; i < TOUCHUI_BTN_COUNT; i++) {
            if (!touchUi_GetButton(i, &bx, &by, &br)) continue;
            const int bOver = bx + br > tl + 0.5f && bx - br < tr2 - 0.5f && by + br > tt + 0.5f && by - br < tt + 70.0f - 0.5f;
            if (bOver) printf("  [%s] message line (state %d) over button %d\n", name, st, i);
            CHECK(!bOver);
        }
        printf("  [%s] message line, state %d: top %.0f, x %.0f..%.0f\n", name, st, tt, tl, tr2);
    }
    closeTrayT();
}

static void test_level_tray(void)
{
    setup();
    float x, y, r;
    g_case = "level pick: DUCK held still with sithewok on only crouches";
    fg.bCheat = 1; fg.numLevels = 21; tick();
    downOn(1, TOUCHUI_BTN_DUCK); wait(3.0);
    CHECK(touchUi_IsKeyHeld(SC_C));
    upOn(1, TOUCHUI_BTN_DUCK); wait(0.1);
    CHECK(!touchUi_IsKeyHeld(SC_C) && fg.levelLoaded == 0 && fg.lastMsg[0] == 0);

    g_case = "level pick: no LEVEL in the tray with sithewok off";
    fg.bCheat = 0; tick();
    openTrayT();
    CHECK(touchUi_IsTrayOpen() && !touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &x, &y, &r) && touchUi_GetButton(TOUCHUI_BTN_TRAYVIEW, &x, &y, &r));
    float cx, vx; bxy(TOUCHUI_BTN_TRAYCHEAT, &cx, &y); bxy(TOUCHUI_BTN_TRAYVIEW, &vx, &y);
    CHECKF(vx - cx, 50.0, 0.5);
    closeTrayT();

    g_case = "level pick: LEVEL right of CHEAT with sithewok on; a tap only says how";
    fg.bCheat = 1; tick();
    openTrayT();
    CHECK(touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &x, &y, &r));
    downOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.1); upOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.1);
    CHECK(fg.levelLoaded == 0 && !strcmp(fg.lastMsg, "Hold LEVEL to count levels") && touchUi_IsTrayOpen());

    g_case = "level pick: the message line under the open tray, back up when it closes";
    { float t0, l0, r0, t1, l1, r1, gx2, gy2; touchUi_GetTopTextRect(&t1, &l1, &r1); bxy(TOUCHUI_BTN_TRAYGYRO, &gx2, &gy2);
      closeTrayT(); touchUi_GetTopTextRect(&t0, &l0, &r0); openTrayT();
      CHECK(t1 >= gy2 + 26 && t0 < gy2 - 20 && r1 - l1 >= 120); }
    g_case = "level pick: LEVEL held counts 1, 2, 3; lifted, level 3 loads and the tray closes";
    downOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.45);
    CHECK(fg.lastMsg[0] == 'H');
    wait(0.1); CHECK(!strcmp(fg.lastMsg, "Level 1 of 21"));
    CHECK(takeHaptics() & TOUCHUI_HAPTIC_TAP);
    wait(0.4); CHECK(!strcmp(fg.lastMsg, "Level 2 of 21"));
    wait(0.4); CHECK(!strcmp(fg.lastMsg, "Level 3 of 21"));
    upOn(2, TOUCHUI_BTN_TRAYLEVEL);
    CHECK(fg.levelLoaded == 0 && !strcmp(fg.lastMsg, "Loading level 3"));
    wait(0.05);
    CHECK(fg.levelLoaded == 3 && !touchUi_IsTrayOpen());

    g_case = "level pick: round past the last";
    fg.levelLoaded = 0; openTrayT();
    downOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(9.1); CHECK(!strcmp(fg.lastMsg, "Level 1 of 21"));
    upOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.05);
    CHECK(fg.levelLoaded == 1);

    g_case = "level pick: slid off while counting: no level";
    fg.levelLoaded = 0; openTrayT();
    float lx, ly; bxy(TOUCHUI_BTN_TRAYLEVEL, &lx, &ly);
    down(2, lx, ly); wait(1.0); move(2, lx + 20, ly); wait(0.1);
    CHECK(!strcmp(fg.lastMsg, "No level picked"));
    move(2, lx, ly); wait(0.5); up(2, lx, ly); wait(0.1);
    CHECK(fg.levelLoaded == 0 && !strcmp(fg.lastMsg, "No level picked"));

    g_case = "level pick: the tray closed while counting: no level";
    closeTrayT(); openTrayT();
    down(2, lx, ly); wait(1.0); down(3, 400, 250); up(3, 400, 250); wait(0.1);
    CHECK(!touchUi_IsTrayOpen() && !strcmp(fg.lastMsg, "No level picked"));
    wait(0.5); up(2, lx, ly); wait(0.1);
    CHECK(fg.levelLoaded == 0);

    g_case = "level pick: the touch cancelled while counting: no level";
    openTrayT(); down(2, lx, ly); wait(1.0); cancel(2); wait(0.1);
    CHECK(fg.levelLoaded == 0);

    g_case = "level pick: sithewok off with the tray open: LEVEL goes, no level";
    closeTrayT(); openTrayT(); down(2, lx, ly); wait(1.0); fg.bCheat = 0; wait(0.1);
    CHECK(!touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &x, &y, &r) && touchUi_IsTrayOpen());
    up(2, lx, ly); wait(0.1);
    CHECK(fg.levelLoaded == 0);
    fg.bCheat = 1; wait(0.1);
    CHECK(touchUi_GetButton(TOUCHUI_BTN_TRAYLEVEL, &x, &y, &r));

    g_case = "level pick: no level to pick (multiplayer guest): never counts, says so";
    fg.numLevels = 0; fg.lastMsg[0] = 0;
    downOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(2.0); CHECK(fg.lastMsg[0] == 0); upOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.1);
    CHECK(fg.levelLoaded == 0);
    downOn(2, TOUCHUI_BTN_TRAYLEVEL); upOn(2, TOUCHUI_BTN_TRAYLEVEL); wait(0.1);
    CHECK(!strcmp(fg.lastMsg, "No level to pick here"));
    fg.numLevels = 21;
    closeTrayT();

    check_level_tray_layout("level tray: iPhone-sized", 852, 393, 59, 0, 59, 21, 0);
    check_level_tray_layout("level tray: iPhone-sized, SWAPPED", 852, 393, 59, 0, 59, 21, 1);
    check_level_tray_layout("level tray: Android phone (732x360)", 732, 360, 0, 0, 0, 0, 0);
    check_level_tray_layout("level tray: small phone (640x360)", 640, 360, 0, 0, 0, 0, 0);
    check_level_tray_layout("level tray: small phone (640x360), SWAPPED", 640, 360, 0, 0, 0, 0, 1);
    check_level_tray_layout("level tray: Android phone (873x393, cutout on the right)", 873, 393, 0, 0, 32, 0, 0);
    check_level_tray_layout("level tray: iPhone SE, Display Zoom (568x320)", 568, 320, 0, 0, 0, 0, 0);
    check_level_tray_layout("level tray: iPhone mini, Display Zoom (693x320)", 693, 320, 44, 0, 44, 21, 0);
    check_level_tray_layout("level tray: iPhone mini (780x360)", 780, 360, 50, 0, 50, 21, 0);
}

static void test_other_input(void)
{
    setup();
    g_case = "a controller in use hides the controls; a touch brings them back and does nothing else";
    touchUi_SetOtherInput(1);
    CHECK(touchUi_IsHiddenForOtherInput());
    CHECK(touchUi_Draw(T)->numItems == 0);
    downOn(1, TOUCHUI_BTN_FIRE);
    CHECK(!touchUi_IsHiddenForOtherInput() && !touchUi_IsKeyHeld(SC_LCTRL));
    upOn(1, TOUCHUI_BTN_FIRE);
    CHECK(touchUi_Draw(T)->numItems > 0);

    g_case = "hidden (a menu): every touch let go";
    downOn(1, TOUCHUI_BTN_FIRE);
    CHECK(touchUi_IsKeyHeld(SC_LCTRL));
    touchUi_SetShown(0);
    CHECK(!touchUi_IsKeyHeld(SC_LCTRL));
    touchUi_SetShown(1);
}

static void test_swapped(void)
{
    setup();
    g_case = "CONTROLS SWAPPED: FIRE on the left, the stick on the right";
    float mx, my, fx, fy;
    bxy(TOUCHUI_BTN_MENU, &mx, &my);
    down(1, mx, my); wait(0.5); up(1, mx, my);
    downOn(2, TOUCHUI_BTN_TRAYCTRL); upOn(2, TOUCHUI_BTN_TRAYCTRL);
    CHECK(touchUi_IsSwapped());
    bxy(TOUCHUI_BTN_FIRE, &fx, &fy);
    CHECK(fx < 852 / 2);
    down(3, 700, 300);
    CHECK(touchUi_IsStickActive());
    up(3, 700, 300);
}

// Every button showing inside the safe area, none on another
// ------------------------------------------------------------ CAMERA and SABER

static void openTray(void)
{
    float mx, my;
    bxy(TOUCHUI_BTN_MENU, &mx, &my);
    down(9, mx, my); wait(0.5); up(9, mx, my);
}

static void test_camera(void)
{
    float cx, cy;
    setup();
    bxy(TOUCHUI_BTN_CAMERA, &cx, &cy);

    g_case = "CAMERA: shown by default, in the top row";
    CHECK(touchUi_IsCameraShown());
    CHECK(touchUi_GetButton(TOUCHUI_BTN_CAMERA, NULL, NULL, NULL));

    g_case = "CAMERA: a tap takes the ticked photos (CLEAN), then the flash and a tap felt";
    down(1, cx, cy); wait(0.2); up(1, cx, cy);
    CHECK(fg.captures == 1 && fg.lastKinds == TOUCHUI_CAPTURE_CLEAN && fg.lastMode == TOUCHUI_CAPTURE_PHOTO);
    CHECK(!touchUi_IsSelfieUp());
    takeHaptics();
    touchUi_CameraFeedback(TOUCHUI_CAPTURE_PHOTO);
    CHECK(takeHaptics() & TOUCHUI_HAPTIC_TAP);
    {
        const touchUiDrawList* l = touchUi_Draw(T);
        const touchUiDrawItem* it = &l->aItems[l->numItems - 1];
        CHECK(it->type == TOUCHUI_DRAW_RRECT && it->x0 == 0.0f && it->fill.r == 1.0f && it->fill.a > 0.5f); // (the flash, over everything)
        l = touchUi_Draw(T + 0.31);
        it = &l->aItems[l->numItems - 1];
        CHECK(!(it->type == TOUCHUI_DRAW_RRECT && it->x0 == 0.0f && it->fill.r == 1.0f && it->fill.g == 1.0f)); // (gone after 0.3 s)
    }

    g_case = "CAMERA: a photo no frame takes in 0.5 s is dropped";
    down(1, cx, cy); wait(0.1); up(1, cx, cy);
    CHECK(fg.captures == 2);
    wait(0.45);
    CHECK(fg.captureCancels == 0);
    wait(0.1);
    CHECK(fg.captureCancels == 1);

    g_case = "CAMERA: the last photos still being saved: a tap takes none";
    fg.bCaptureFull = 1;
    down(1, cx, cy); wait(0.1); up(1, cx, cy);
    CHECK(fg.captures == 2);
    fg.bCaptureFull = 0;

    g_case = "CAMERA: held 0.49 s, lifted: a photo, no selfie";
    down(1, cx, cy); T += 0.49; up(1, cx, cy); tick();
    CHECK(fg.captures == 3 && !touchUi_IsSelfieUp());

    g_case = "CAMERA: held still 0.5 s: the selfie (SELFIES: 3); the lift takes no photo";
    down(1, cx, cy);
    wait(0.25);
    CHECK(touchUi_GetRingProgress(TOUCHUI_BTN_CAMERA) > 0.4f && !touchUi_IsSelfieUp());
    wait(0.26);
    CHECK(touchUi_IsSelfieUp() && fg.bSelfie && fg.selfieShots == 3);
    up(1, cx, cy);
    CHECK(fg.captures == 3);

    g_case = "selfie: every touch ignored, nothing drawn but the flash";
    {
        float fx, fy;
        bxy(TOUCHUI_BTN_FIRE, &fx, &fy);
        down(2, fx, fy); tick();
        CHECK(!touchUi_IsKeyHeld(SC_LCTRL));
        up(2, fx, fy);
        const touchUiDrawList* l = touchUi_Draw(T);
        CHECK(l->numItems == 0);
    }
    g_case = "selfie: over (the engine says so): all back";
    touchUi_PhotoModeEnded(TOUCHUI_MODE_SELFIE);
    CHECK(!touchUi_IsSelfieUp());
    CHECK(touchUi_Draw(T)->numItems > 10);
    g_case = "selfie: ended by the engine without saying: put back at the next frame";
    down(1, cx, cy); wait(0.55); up(1, cx, cy);
    CHECK(touchUi_IsSelfieUp());
    fg.bSelfie = 0;
    tick();
    CHECK(!touchUi_IsSelfieUp());

    g_case = "selfie: can't (multiplayer, a cutscene): a message, nothing hidden";
    fg.selfieResult = TOUCHUI_PHOTOMODE_CANT;
    down(1, cx, cy); wait(0.55); up(1, cx, cy);
    CHECK(!touchUi_IsSelfieUp() && !strcmp(fg.lastMsg, "Can't take a selfie here"));
    fg.selfieResult = TOUCHUI_PHOTOMODE_BUSY;
    down(1, cx, cy); wait(0.55); up(1, cx, cy);
    CHECK(!touchUi_IsSelfieUp() && !strcmp(fg.lastMsg, "Still saving the last photo"));
    fg.selfieResult = 1;

    g_case = "CAMERA half held, a wheel opened by another finger: no half-filled ring left, no selfie";
    {
        float wx, wy;
        bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
        down(5, cx, cy); wait(0.25);
        CHECK(touchUi_GetRingProgress(TOUCHUI_BTN_CAMERA) > 0.4f);
        down(6, wx, wy); move(6, wx + 12.0f, wy);
        CHECK(touchUi_IsWheelOpen() && touchUi_GetRingProgress(TOUCHUI_BTN_CAMERA) == 0.0f);
        wait(0.4);
        CHECK(!touchUi_IsSelfieUp() && touchUi_GetRingProgress(TOUCHUI_BTN_CAMERA) == 0.0f);
        up(5, cx, cy); up(6, wx + 12.0f, wy);
        float pcx, pcy, r0, r1;
        touchUi_GetWheelGeometry(&pcx, &pcy, &r0, &r1);
        if (touchUi_IsWheelOpen()) { down(7, pcx, pcy + (r0 + r1) * 0.5f); up(7, pcx, pcy + (r0 + r1) * 0.5f); }
        CHECK(!touchUi_IsWheelOpen());
    }

    g_case = "CAMERA: slid 10 pt sideways: a burst, a clean shot every 0.2 s while the finger stays";
    int c0 = fg.captures;
    down(1, cx, cy);
    move(1, cx + 9.5f, cy);
    CHECK(touchUi_GetBurstShots() < 0);
    move(1, cx + 10.5f, cy);
    CHECK(touchUi_GetBurstShots() == 0 && fg.batch == 1);
    wait(1.0 / 60.0);
    CHECK(fg.captures == c0 + 1 && fg.lastMode == TOUCHUI_CAPTURE_BURST && fg.lastKinds == TOUCHUI_CAPTURE_CLEAN);
    wait(0.5);
    CHECK(fg.captures == c0 + 3);
    wait(1.0);
    CHECK(!touchUi_IsSelfieUp()); // (a slide is never a hold)
    CHECK(touchUi_GetBurstShots() == 8);
    wait(2.0);
    CHECK(fg.captures == c0 + 10); // (at most 10)
    up(1, cx + 10.5f, cy);
    CHECK(touchUi_GetBurstShots() < 0 && fg.batch == 0);
    CHECK(fg.captures == c0 + 10); // (the lift takes none)

    g_case = "CAMERA: slid down 20 pt (FREEZE on): on its way, no burst, no photo";
    c0 = fg.captures;
    down(1, cx, cy); move(1, cx, cy + 20.0f); wait(0.1);
    CHECK(touchUi_GetBurstShots() < 0 && !touchUi_IsFreezing());
    up(1, cx, cy + 20.0f);
    CHECK(fg.captures == c0);

    g_case = "CAMERA: slid down 36 pt: Action Freeze; only CAMERA and the X";
    down(1, cx, cy); move(1, cx + 5.0f, cy + 20.0f); move(1, cx + 5.0f, cy + 36.0f);
    CHECK(touchUi_IsFreezing() && fg.bFreeze && fg.bSmooth);
    up(1, cx + 5.0f, cy + 36.0f);
    CHECK(touchUi_IsFreezing());
    g_case = "Action Freeze: one finger turns the camera round, 0.3 degrees a point, smoothly";
    down(2, 300, 200);
    move(2, 400, 200);
    wait(1.0 / 60.0);
    CHECK(fg.orbitYaw < 0.0f && fg.orbitYaw > -30.0f);
    wait(1.0);
    CHECKF(fg.orbitYaw, -30.0, 0.05);
    CHECKF(fg.orbitPitch, 0.0, 0.001);
    g_case = "Action Freeze: a second finger: the two pinch to zoom and move it";
    down(3, 500, 200);
    move(3, 600, 200); // (spread from 100 to 200: twice as close)
    wait(1.0);
    CHECKF(fg.zoom, 2.0, 0.02);
    CHECK(fg.panX > 0.0f);
    up(2, 400, 200); up(3, 600, 200);
    g_case = "Action Freeze: CAMERA tapped: one clean photo";
    c0 = fg.captures;
    down(4, cx, cy); wait(0.1); up(4, cx, cy);
    CHECK(fg.captures == c0 + 1 && fg.lastKinds == TOUCHUI_CAPTURE_CLEAN && fg.lastMode == TOUCHUI_CAPTURE_PHOTO);
    CHECK(touchUi_IsFreezing());
    g_case = "Action Freeze: the X: back to the game, as it was";
    {
        float xx, xy;
        touchUi_GetFreezeClose(&xx, &xy);
        down(5, xx, xy); wait(0.05);
        CHECK(touchUi_IsFreezing());
        up(5, xx, xy);
        CHECK(!touchUi_IsFreezing() && !fg.bFreeze && fg.freezeEnds == 1);
    }

    g_case = "CAMERA: slid up (to near the top edge): the cinematic burst; over: all back";
    down(1, cx, cy); move(1, cx, 4.0f);
    CHECK(touchUi_IsCinemaUp() && fg.bCinema);
    up(1, cx, 4.0f);
    CHECK(touchUi_Draw(T)->numItems == 0);
    touchUi_PhotoModeEnded(TOUCHUI_MODE_CINEMA);
    CHECK(!touchUi_IsCinemaUp());

    g_case = "Action Freeze can't (multiplayer): a message";
    fg.bFreezeOk = 0;
    down(1, cx, cy); move(1, cx, cy + 40.0f);
    CHECK(!touchUi_IsFreezing() && !strcmp(fg.lastMsg, "Can't freeze here"));
    up(1, cx, cy + 40.0f);
    fg.bFreezeOk = 1;

    g_case = "the overlay going (a menu) ends Action Freeze";
    down(1, cx, cy); move(1, cx, cy + 40.0f); up(1, cx, cy + 40.0f);
    CHECK(touchUi_IsFreezing());
    touchUi_SetShown(0);
    CHECK(!touchUi_IsFreezing() && !fg.bFreeze);
    touchUi_SetShown(1);
    tick();

    g_case = "tray: the camera switch held 0.45 s: the camera options";
    openTray();
    CHECK(touchUi_IsTrayOpen());
    downOn(2, TOUCHUI_BTN_TRAYCAM); wait(0.44);
    CHECK(touchUi_GetOptRow() == 0);
    wait(0.02);
    CHECK(touchUi_GetOptRow() == 2);
    upOn(2, TOUCHUI_BTN_TRAYCAM);
    CHECK(touchUi_IsCameraShown()); // (the lift doesn't switch it)
    g_case = "camera options: SELFIES 3, 5, 1; FREEZE off; PHOTO opens the kinds";
    downOn(2, TOUCHUI_BTN_CAMROW_SELFIES); upOn(2, TOUCHUI_BTN_CAMROW_SELFIES);
    CHECK(touchUi_GetSelfieShots() == 5);
    downOn(2, TOUCHUI_BTN_CAMROW_SELFIES); upOn(2, TOUCHUI_BTN_CAMROW_SELFIES);
    CHECK(touchUi_GetSelfieShots() == 1);
    downOn(2, TOUCHUI_BTN_CAMROW_FREEZE); upOn(2, TOUCHUI_BTN_CAMROW_FREEZE);
    CHECK(!touchUi_IsFreezeOn());
    downOn(2, TOUCHUI_BTN_CAMROW_PHOTO); upOn(2, TOUCHUI_BTN_CAMROW_PHOTO);
    CHECK(touchUi_IsKindsRowOpen() && touchUi_GetOptRow() == 2);
    g_case = "photo kinds: HUD and HUD+BTNS ticked, CLEAN off; the last one stays";
    downOn(2, TOUCHUI_BTN_CAMOPT_HUD); upOn(2, TOUCHUI_BTN_CAMOPT_HUD);
    downOn(2, TOUCHUI_BTN_CAMOPT_BTNS); upOn(2, TOUCHUI_BTN_CAMOPT_BTNS);
    downOn(2, TOUCHUI_BTN_CAMOPT_CLEAN); upOn(2, TOUCHUI_BTN_CAMOPT_CLEAN);
    CHECK(touchUi_GetCameraKinds() == (TOUCHUI_CAPTURE_HUD | TOUCHUI_CAPTURE_HUDBTNS));
    downOn(2, TOUCHUI_BTN_CAMOPT_HUD); upOn(2, TOUCHUI_BTN_CAMOPT_HUD);
    downOn(2, TOUCHUI_BTN_CAMOPT_BTNS); upOn(2, TOUCHUI_BTN_CAMOPT_BTNS);
    CHECK(touchUi_GetCameraKinds() == TOUCHUI_CAPTURE_HUDBTNS);
    downOn(2, TOUCHUI_BTN_CAMOPT_HUD); upOn(2, TOUCHUI_BTN_CAMOPT_HUD);
    g_case = "camera options: a touch elsewhere closes them";
    down(3, 400, 300); up(3, 400, 300);
    CHECK(touchUi_GetOptRow() == 0 && !touchUi_IsKindsRowOpen());
    g_case = "CAMERA: a tap takes the kinds ticked";
    down(1, cx, cy); wait(0.1); up(1, cx, cy);
    CHECK(fg.lastKinds == (TOUCHUI_CAPTURE_HUD | TOUCHUI_CAPTURE_HUDBTNS) && fg.lastMode == TOUCHUI_CAPTURE_PHOTO);
    g_case = "FREEZE off: slid down is a burst";
    down(1, cx, cy); move(1, cx, cy + 40.0f);
    CHECK(!touchUi_IsFreezing() && touchUi_GetBurstShots() == 0);
    up(1, cx, cy + 40.0f);
    g_case = "tray: the camera switch tapped hides CAMERA";
    openTray();
    downOn(2, TOUCHUI_BTN_TRAYCAM); wait(0.1); upOn(2, TOUCHUI_BTN_TRAYCAM);
    CHECK(!touchUi_IsCameraShown() && !touchUi_GetButton(TOUCHUI_BTN_CAMERA, NULL, NULL, NULL) && !touchUi_IsTrayOpen());
    g_case = "camera settings remembered";
    touchUi_Init(&fakeGame, g_settings);
    CHECK(!touchUi_IsCameraShown() && touchUi_GetSelfieShots() == 1 && !touchUi_IsFreezeOn()
          && touchUi_GetCameraKinds() == (TOUCHUI_CAPTURE_HUD | TOUCHUI_CAPTURE_HUDBTNS));
}

static int drawnText(const char* t);
static int settingsHave(const char* line);
// ---- the saber wheel's choices, tapped
// A tap on one (its middle; finger 9): 0 if it isn't shown
static int tapChip(int chip)
{
    float x, y;
    if (!touchUi_GetSaberChip(chip, &x, &y)) return 0;
    down(9, x, y); up(9, x, y);
    return 1;
}
// A tap on a colour's slice (the middle of the band), or in the gap at the bottom (bin -1)
static void tapColour(int bin)
{
    float cx, cy, r0, r1, deg = 270.0f, px, py;
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    if (bin >= 0) touchUi_GetWheelSlice(bin, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(9, px, py); up(9, px, py);
}
static void tapSaber(void) { float sx, sy; bxy(TOUCHUI_BTN_SABER, &sx, &sy); down(1, sx, sy); wait(0.1); up(1, sx, sy); }
#define CARD(h) (TOUCHUI_CHIP_CARD + (h))
#define HILT(h) (TOUCHUI_CHIP_HILT + (h))
#define DOT(h, b) (TOUCHUI_CHIP_DOT + 2 * (h) + (b))

static void test_saber(void)
{
    float sx, sy, cx, cy, r0, r1, deg;
    setup();
    g_case = "SABER: hidden until the saber is had (or sithewok is on)";
    CHECK(!touchUi_IsSaberShown());
    fg.bSaberBtn = 1;
    tick();
    CHECK(touchUi_IsSaberShown() && touchUi_GetButton(TOUCHUI_BTN_SABER, &sx, &sy, NULL));

    g_case = "SABER: a tap opens its wheel to tap a colour; the game holds still";
    fg.saberLook = 1; // (REFORGED, picked: these cases were made with it)
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelKind() == TOUCHUI_WHEEL_SABER && touchUi_IsWheelTapMode() && fg.bHold);
    // (Jedi Knight: 16, colour 7 -- GREEN -- left out; each slice keeps its colour's id)
    CHECK(touchUi_GetWheelSliceBin(15) == 16 && touchUi_GetWheelSliceBin(16) == -1
          && touchUi_GetWheelSliceBin(6) == 6 && touchUi_GetWheelSliceBin(7) == 8);
    g_case = "saber wheel, one saber: the RIGHT card (its HILT), + 2ND SABER on the left, FILM, EQUIP; no LEFT card, x, SAME switches or AUTO";
    CHECK(touchUi_GetSaberChip(CARD(0), NULL, NULL) && touchUi_GetSaberChip(HILT(0), NULL, NULL) && touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, NULL, NULL)
          && touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, NULL, NULL) && touchUi_GetSaberChip(TOUCHUI_CHIP_EQUIP, NULL, NULL));
    CHECK(!touchUi_GetSaberChip(CARD(1), NULL, NULL) && !touchUi_GetSaberChip(HILT(1), NULL, NULL) && !touchUi_GetSaberChip(TOUCHUI_CHIP_REMOVE, NULL, NULL)
          && !touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_COLOUR, NULL, NULL) && !touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_HILT, NULL, NULL)
          && !touchUi_GetSaberChip(TOUCHUI_CHIP_AUTO, NULL, NULL) && !touchUi_GetSaberChip(DOT(0, 0), NULL, NULL));
    {
        float ax, ay, bx, by;
        touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
        touchUi_GetSaberChip(CARD(0), &ax, &ay);
        touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, &bx, &by);
        CHECK(ax > cx + r1 && bx < cx - r1 && fabsf(ay - by) < 0.01f && drawnText("RIGHT") && drawnText("2ND SABER") && drawnText("tap a colour"));
    }
    g_case = "saber wheel: a colour tapped is set, the wheel stays open, a tick felt";
    float px, py;
    touchUi_GetWheelSlice(5, &deg);
    takeHaptics();
    tapColour(5);
    CHECK(SC(0, 0) == 5 && touchUi_IsWheelOpen());
    CHECK(takeHaptics() & TOUCHUI_HAPTIC_TICK);
    g_case = "saber wheel: + 2ND SABER: two sabers, the LEFT card picked (gold); a colour goes to the left saber only";
    g_platTicks = 0;
    CHECK(tapChip(TOUCHUI_CHIP_ADD));
    CHECK(fg.saberStyle == TOUCHUI_SABER_DUAL && touchUi_IsWheelOpen() && g_platTicks == 1 && iosSaberWheel_GetPickedHand() == 1);
    CHECK(touchUi_GetSaberChip(CARD(1), NULL, NULL) && touchUi_GetSaberChip(TOUCHUI_CHIP_REMOVE, NULL, NULL) && touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_COLOUR, NULL, NULL)
          && touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_HILT, NULL, NULL) && !touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, NULL, NULL) && drawnText("colour for the left saber"));
    CHECK(touchUi_GetWheelSlice(7, NULL) < 0); // (GREEN: not on Jedi Knight's wheel)
    tapColour(8);
    CHECK(SC(1, 1) == 8 && SC(1, 0) == 5); // (the right hand's saber as it was with one)
    g_case = "saber wheel: SAME COLOUR: a colour goes to both sabers";
    CHECK(tapChip(TOUCHUI_CHIP_SAME_COLOUR) && iosSaberWheel_GetSameColour() && drawnText("colour for both sabers"));
    tapColour(12);
    CHECK(SC(1, 1) == 12 && SC(1, 0) == 12);
    CHECK(SC(0, 0) == 12); // (the right hand's saber: the same one with one saber or two)
    g_case = "saber wheel: a tap in the gap closes it; the game goes on";
    tapColour(-1);
    CHECK(!touchUi_IsWheelOpen() && !fg.bHold);
    g_case = "saber wheel in Mysteries of the Sith: all 17, GREEN (7) on it and picked";
    fg.bMots = 1;
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelSliceBin(16) == 16 && touchUi_GetWheelSliceBin(17) == -1 && touchUi_GetWheelSlice(7, &deg) == 7);
    tapColour(7);
    CHECK(SC(1, 1) == 7 && SC(1, 0) == 7 && touchUi_IsWheelOpen());
    tapColour(-1);
    CHECK(!touchUi_IsWheelOpen());
    fg.bMots = 0;

    g_case = "SABER: a 10 pt slide opens it to slide; lifted on a colour: set, and open to tap";
    down(1, sx, sy);
    move(1, sx, sy + 10.5f);
    CHECK(touchUi_IsWheelOpen() && !touchUi_IsWheelTapMode());
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    touchUi_GetWheelSlice(3, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    move(1, px, py);
    CHECK(touchUi_GetWheelHot() == 3);
    up(1, px, py);
    CHECK(SC(1, 0) == 3 && SC(1, 1) == 3 && touchUi_IsWheelOpen() && touchUi_IsWheelTapMode());
    g_case = "saber wheel: EQUIP without a saber does nothing";
    float qx, qy;
    touchUi_GetSaberChip(TOUCHUI_CHIP_EQUIP, &qx, &qy);
    down(2, qx, qy); up(2, qx, qy);
    CHECK(touchUi_IsWheelOpen());
    g_case = "saber wheel: EQUIP with the saber: in hand, the wheel closed";
    fg.aWeaponOwned[10] = fg.aWeaponSel[10] = 1;
    down(2, qx, qy); up(2, qx, qy);
    CHECK(!touchUi_IsWheelOpen() && fg.selectedWeapon == 10);

    g_case = "SABER: held still 0.3 s: open to tap";
    down(1, sx, sy); wait(0.29);
    CHECK(!touchUi_IsWheelOpen() && touchUi_GetRingProgress(TOUCHUI_BTN_SABER) > 0.8f);
    wait(0.02);
    CHECK(touchUi_IsWheelOpen() && touchUi_IsWheelTapMode());
    up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen());
    CHECK(tapChip(CARD(0)) && iosSaberWheel_GetPickedHand() == 0);
    tapColour(-1);
    CHECK(!touchUi_IsWheelOpen());

    g_case = "saber choices remembered (by key and name)";
    memset(fg.aBladeColour, 0, sizeof(fg.aBladeColour)); memset(fg.aUnlinked, 0, sizeof(fg.aUnlinked));
    fg.saberStyle = 0;
    iosSaberWheel_SetSameColour(0);
    touchUi_Init(&fakeGame, g_settings);
    CHECK(fg.saberStyle == TOUCHUI_SABER_DUAL && SC(0, 0) == 3 && SC(1, 0) == 3 && SC(1, 1) == 3 && iosSaberWheel_GetSameColour());
    CHECK(iosSaberWheel_GetPickedHand() == 0 && settingsHave("saberSameColour=1\n") && !settingsHave("saberTarget"));

    g_case = "+ 2ND SABER not offered where two can't be had";
    setup();
    fg.bSaberBtn = 1; fg.bDualOk = 0;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, NULL, NULL) && touchUi_GetSaberChip(CARD(0), NULL, NULL));
    inputWheel_SaberChip(INPUTWHEEL_CHIP_ADD);
    CHECK(fg.saberStyle == TOUCHUI_SABER_SINGLE);
}

// The saber wheel's look switch (CLASSIC / REFORGED), in its middle
static void test_saber_look(void)
{
    float sx, sy, cx, cy, r0, r1, deg, px, py, lx, ly, qx, qy;
    setup();
    fg.bSaberBtn = 1;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    g_case = "look: CLASSIC to start (nothing kept): all 17 slices in Jedi Knight";
    CHECK(fg.saberLook == 0);
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelSliceBin(16) == 16 && touchUi_GetWheelSlice(7, NULL) == 7);
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen());
    fg.saberLook = 1; // (REFORGED picked: the cases below were made from it)
    g_case = "look: the switch in the saber wheel's middle, over the name; REFORGED picked, 16 slices (no GREEN) in Jedi Knight";
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_LOOK, &lx, &ly));
    CHECK(fabsf(lx - cx) < 0.01f && ly < cy - 20.0f && hypotf(lx - cx, ly - cy) < r0 - 10.0f);
    CHECK(fg.saberLook == 1 && touchUi_GetWheelSliceBin(15) == 16 && touchUi_GetWheelSliceBin(16) == -1 && touchUi_GetWheelSlice(7, NULL) < 0);
    g_case = "look: tapped: CLASSIC, a tick, the wheel open (to tap) with all 17, the game still held";
    g_platTicks = 0;
    down(2, lx, ly); up(2, lx, ly);
    CHECK(fg.saberLook == 0 && g_platTicks == 1 && touchUi_IsWheelOpen() && touchUi_IsWheelTapMode() && fg.bHold);
    CHECK(touchUi_GetWheelSliceBin(16) == 16 && touchUi_GetWheelSliceBin(17) == -1 && touchUi_GetWheelSlice(7, &deg) == 7);
    g_case = "look: CLASSIC, Jedi Knight: GREEN tapped: set; back to REFORGED: 16 again, GREEN kept (shown as ORIGINAL)";
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(SC(0, 0) == 7 && touchUi_IsWheelOpen());
    down(2, lx, ly); up(2, lx, ly);
    CHECK(fg.saberLook == 1 && SC(0, 0) == 7 && iosSaber_GetShownColour(0, 0) == 0 && touchUi_GetWheelSlice(7, NULL) < 0 && touchUi_IsWheelOpen());
    g_case = "look: slid from SABER in CLASSIC: one tick per slice (17); the gap cancels";
    down(2, lx, ly); up(2, lx, ly); // (CLASSIC)
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen() && !fg.bHold && fg.saberLook == 0);
    down(1, sx, sy);
    move(1, sx, sy + 10.5f);
    CHECK(touchUi_IsWheelOpen() && !touchUi_IsWheelTapMode());
    g_platTicks = 0;
    for (int k = 0; k < 17; k++) {
        touchUi_GetWheelSlice(touchUi_GetWheelSliceBin(k), &deg);
        polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
        move(1, px, py);
    }
    const int nTicks = g_platTicks;
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    move(1, px, py); up(1, px, py);
    CHECK(nTicks == 17 && !touchUi_IsWheelOpen() && SC(0, 0) == 7);
    g_case = "look: a slide lifted on the switch: nothing picked, closed, the look unchanged";
    down(1, sx, sy);
    move(1, sx, sy + 10.5f);
    touchUi_GetWheelSlice(9, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    move(1, px, py); move(1, lx, ly); up(1, lx, ly);
    CHECK(!touchUi_IsWheelOpen() && fg.saberLook == 0 && SC(0, 0) == 7);
    g_case = "look: held 0.3 s, open to tap: the switch works";
    down(1, sx, sy); wait(0.31); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && touchUi_IsWheelTapMode());
    down(2, lx, ly); up(2, lx, ly);
    CHECK(fg.saberLook == 1 && touchUi_IsWheelOpen() && touchUi_GetWheelSliceBin(16) == -1);
    g_case = "look: the ring's top slice just past the switch: that slice, not the switch";
    {
        const int k = 8; // (TEAL: the top in REFORGED's 16 is between TEAL and CYAN; TEAL just left of it)
        touchUi_GetWheelSlice(k, &deg);
        polar(cx, cy, deg, r0 + 6.0f, &px, &py);
        down(2, px, py); up(2, px, py);
        CHECK(fg.saberLook == 1 && SC(0, 0) == k && touchUi_IsWheelOpen());
    }
    g_case = "look: two sabers in both looks: LEFT gets GREEN in CLASSIC, RIGHT gets BLUE in REFORGED";
    touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, &qx, &qy);
    down(2, qx, qy); up(2, qx, qy);
    down(2, lx, ly); up(2, lx, ly); // CLASSIC
    touchUi_GetSaberChip(TOUCHUI_CHIP_CARD + 1, &qx, &qy);
    down(2, qx, qy); up(2, qx, qy);
    touchUi_GetWheelSlice(7, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(fg.saberStyle == TOUCHUI_SABER_DUAL && SC(1, 1) == 7 && SC(1, 0) == 8); // (the right hand's as it was with one saber)
    down(2, lx, ly); up(2, lx, ly); // REFORGED
    touchUi_GetSaberChip(TOUCHUI_CHIP_CARD, &qx, &qy);
    down(2, qx, qy); up(2, qx, qy);
    touchUi_GetWheelSlice(11, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(fg.saberLook == 1 && SC(1, 0) == 11 && SC(1, 1) == 7 && iosSaber_GetShownColour(1, 1) == 0 && touchUi_IsWheelOpen());
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen());
    g_case = "look: Mysteries of the Sith: 17 in both looks";
    fg.bMots = 1;
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    const int n0 = touchUi_GetWheelSliceBin(16) == 16 && touchUi_GetWheelSlice(7, NULL) == 7;
    down(2, lx, ly); up(2, lx, ly);
    CHECK(n0 && fg.saberLook == 0 && touchUi_GetWheelSliceBin(16) == 16 && touchUi_GetWheelSliceBin(17) == -1 && touchUi_GetWheelSlice(7, NULL) == 7);
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    fg.bMots = 0;
    g_case = "look: 2 pt inside the ring's inner edge across the top: the slice there, never the switch";
    fg.saberStyle = 0;
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    if (fg.saberLook != 0) { down(2, lx, ly); up(2, lx, ly); } // (CLASSIC: a slice centred on the top)
    {
        int bOk = 1;
        for (int d = 50; d <= 130; d += 5) {
            polar(cx, cy, (float)d, r0 - 2.0f, &px, &py);
            const int k = inputWheel_SliceAt(px, py, -1);
            if (k < 0) continue;
            SC(0, 0) = 0;
            down(2, px, py); up(2, px, py);
            if (fg.saberLook != 0 || SC(0, 0) != touchUi_GetWheelSliceBin(k) || !touchUi_IsWheelOpen()) bOk = 0;
        }
        CHECK(bOk);
    }
    g_case = "look: a finger resting on a slice when the look flips points at nothing (the slices are others now)";
    touchUi_GetWheelSlice(10, &deg);
    polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
    down(3, px, py);
    const int hot0 = touchUi_GetWheelHot();
    down(2, lx, ly); up(2, lx, ly); // (REFORGED: slice 10 is BLUE now)
    printf("  resting finger: hot %d -> %d, look %d\n", hot0, touchUi_GetWheelHot(), fg.saberLook);
    CHECK(hot0 == 10 && fg.saberLook == 1 && touchUi_GetWheelHot() == -1);
    up(3, px, py);
    if (touchUi_IsWheelOpen()) { polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py); down(2, px, py); up(2, px, py); }
    SC(0, 0) = 8;
    g_case = "look: the shared wheel's own call (a pad's L3, the 4 key) switches it, the wheel open";
    fg.saberLook = 0;
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    inputWheel_SaberChip(INPUTWHEEL_CHIP_LOOK);
    CHECK(fg.saberLook == 1 && inputWheel_IsOpen() && inputWheel_GetOpenMap()->numSlices == 16);
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen());
    g_case = "look: no switch on the Force wheel";
    {
        float wx, wy;
        bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
        down(1, wx, wy); wait(0.35); up(1, wx, wy);
        CHECK(touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_LOOK, NULL, NULL) && inputWheel_ChipAt(cx, cy - 30.0f) < 0);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
    }
    g_case = "look: remembered (by name) over a restart";
    {
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        down(2, lx, ly); up(2, lx, ly);
        const int lk = fg.saberLook;
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
        fg.saberLook = !lk;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.saberLook == lk);
    }
    g_case = "a pad's or the keys' picks (look, style, colour) are remembered too, the overlay hidden";
    touchUi_SetShown(0);
    fg.bMots = 0; fg.saberStyle = 0; fg.saberLook = 0;
    tick();
    inputWheel_Open(INPUTWHEEL_SABER, 1);
    inputWheel_SaberChip(INPUTWHEEL_CHIP_LOOK); // (REFORGED)
    inputWheel_SaberChip(INPUTWHEEL_CHIP_ADD);
    inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR);
    inputWheel_Close(inputWheel_GetOpenMap()->numSlices - 1); // (BLACK, to both)
    inputWheel_Close(-1);
    tick();
    printf("  pad picks: look %d style %d colours %d/%d\n", fg.saberLook, fg.saberStyle, SC(1, 0), SC(1, 1));
    CHECK(fg.saberLook == 1 && fg.saberStyle == 1 && SC(1, 0) == 16 && SC(1, 1) == 16);
    fg.saberLook = 0; fg.saberStyle = 0; SC(1, 0) = SC(1, 1) = 0;
    touchUi_Init(&fakeGame, g_settings);
    CHECK(fg.saberLook == 1 && fg.saberStyle == 1 && SC(1, 0) == 16 && SC(1, 1) == 16);
}

static int drawnText(const char* t);
static int drawnTextAt(const char* t, float* pX, float* pY, float* pSize);
// The saber wheel's choices as laid out now (on a screen W x H, insets l t r
// b; bTouch: under the touch overlay's top row, clear of its buttons): each
// shown inside the safe area; the cards and switches clear of the popped
// ring by 14 pt; none within 6 pt of another (THROW 8); each HILT, dot and x
// inside its card; clear of every button showing (the action buttons are
// hidden). FILM shown.
static float g_ringGap = 14.0f;
static int chipRect(int i, float* r) { return inputWheel_GetChipRect(i, &r[0], &r[1], &r[2], &r[3]) && inputWheel_IsOpen(); }
static float rectGap(const float* a, const float* b)
{
    const float ex = fmaxf(fmaxf(a[0] - (b[0] + b[2]), b[0] - (a[0] + a[2])), 0.0f), ey = fmaxf(fmaxf(a[1] - (b[1] + b[3]), b[1] - (a[1] + a[3])), 0.0f);
    return hypotf(ex, ey);
}
static int layoutClear(float W, float H, float l, float t, float r, float b, int bTouch, const char* what)
{
    float cx, cy, r0, r1;
    inputWheel_GetGeometry(&cx, &cy, &r0, &r1);
    const float top = fmaxf(t, 8.0f) + (bTouch ? 52.0f : 0.0f);
    static const int aOuter[] = { TOUCHUI_CHIP_CARD, TOUCHUI_CHIP_CARD + 1, TOUCHUI_CHIP_ADD, TOUCHUI_CHIP_SAME_COLOUR, TOUCHUI_CHIP_SAME_HILT,
                                  TOUCHUI_CHIP_SHAPE, TOUCHUI_CHIP_AUTO, TOUCHUI_CHIP_THROW, TOUCHUI_CHIP_EQUIP };
    const int nOuter = (int)(sizeof(aOuter) / sizeof(aOuter[0]));
    int bOk = 1, n = 0;
    float rc[4], ro[4];
    for (int k = 0; k < nOuter; k++) {
        const int i = aOuter[k];
        if (!chipRect(i, rc)) continue;
        n++;
        if (rc[0] < fmaxf(l, 8.0f) - 0.01f || rc[0] + rc[2] > W - fmaxf(r, 8.0f) + 0.01f || (i != TOUCHUI_CHIP_EQUIP && rc[1] < top - 0.01f)
            || rc[1] + rc[3] > H - fmaxf(b, 8.0f) + 0.01f) { printf("  %s: choice %d off the safe area (%.0f,%.0f %.0fx%.0f)\n", what, i, rc[0], rc[1], rc[2], rc[3]); bOk = 0; }
        if (i != TOUCHUI_CHIP_EQUIP) {
            const float nx = fminf(fmaxf(cx, rc[0]), rc[0] + rc[2]), ny = fminf(fmaxf(cy, rc[1]), rc[1] + rc[3]);
            const float d = hypotf(nx - cx, ny - cy) - r1 - 10.0f;
            if (d < (i == TOUCHUI_CHIP_THROW ? 6.0f : g_ringGap) - 0.01f) { printf("  %s: choice %d %.1f pt from the popped ring\n", what, i, d); bOk = 0; }
        }
        for (int m = 0; m < k; m++) {
            if (!chipRect(aOuter[m], ro)) continue;
            const float g = rectGap(rc, ro), need = (i == TOUCHUI_CHIP_THROW || aOuter[m] == TOUCHUI_CHIP_THROW) ? 8.0f : 6.0f;
            if (g < need - 0.01f) { printf("  %s: choices %d and %d %.1f pt apart\n", what, aOuter[m], i, g); bOk = 0; }
        }
        for (int kb = 0; bTouch && kb < TOUCHUI_BTN_TRAYGYRO; kb++) {
            float bx, by, br;
            if (kb == TOUCHUI_BTN_SABER || !touchUi_GetButton(kb, &bx, &by, &br)) continue;
            if (kb <= TOUCHUI_BTN_FORCE && touchUi_AreActionsHidden()) continue;
            const float ex = fmaxf(fmaxf(rc[0] - bx, bx - (rc[0] + rc[2])), 0.0f), ey = fmaxf(fmaxf(rc[1] - by, by - (rc[1] + rc[3])), 0.0f);
            if (hypotf(ex, ey) < br + 4.0f) { printf("  %s: choice %d on button %d\n", what, i, kb); bOk = 0; }
        }
    }
    // the x, each HILT and dot inside its card
    for (int i = TOUCHUI_CHIP_HILT; i <= TOUCHUI_CHIP_REMOVE; i++) {
        if (i == TOUCHUI_CHIP_ADD || !chipRect(i, rc)) continue;
        const int hand = i == TOUCHUI_CHIP_REMOVE ? 1 : i < TOUCHUI_CHIP_DOT ? i - TOUCHUI_CHIP_HILT : (i - TOUCHUI_CHIP_DOT) / 2;
        if (!chipRect(TOUCHUI_CHIP_CARD + hand, ro) || rc[0] < ro[0] - 0.01f || rc[1] < ro[1] - 0.01f || rc[0] + rc[2] > ro[0] + ro[2] + 0.01f
            || rc[1] + rc[3] > ro[1] + ro[3] + 0.01f) { printf("  %s: %d outside its card\n", what, i); bOk = 0; }
    }
    chipRect(TOUCHUI_CHIP_CARD, rc);
    printf("  %s: %d choices beside the ring, the RIGHT card at %.0f,%.0f (wheel %.0f,%.0f r1 %.0f)\n", what, n, rc[0], rc[1], cx, cy, r1);
    return bOk && chipRect(TOUCHUI_CHIP_SHAPE, rc);
}
static int chipsClear(float W, float H, float l, float t, float r, float b, const char* what) { return layoutClear(W, H, l, t, r, b, 1, what); }

// The saber wheel's state for a layout case: 0 one saber; 1 two; 2 one
// CROSS; 3 two CROSS; 4 two, the right STAFF (its blades each their own),
// the left FOLD (linked); 5 one FOLD
static void saberState(int st)
{
    fg.bStaffOk = fg.bFoldOk = 1;
    fg.saberStyle = st == 1 || st == 3 || st == 4;
    fg.aHilt[0] = st == 2 || st == 3 ? 1 : st == 4 ? 2 : st == 5 ? 3 : 0;
    fg.aHilt[1] = st == 3 ? 1 : st == 4 ? 3 : 0;
    fg.aUnlinked[0] = st == 4;
    fg.aUnlinked[1] = 0;
    fg.aBladeColour[0][0] = 10; fg.aBladeColour[0][1] = 13; fg.aBladeColour[1][0] = 2; fg.aBladeColour[1][1] = 2;
}
static const char* const g_aStateName[6] = { "one", "two sabers", "CROSS", "two CROSS", "STAFF + FOLD", "one FOLD" };

static void test_saber_shape(void)
{
    float sx, sy, cx, cy, r0, r1, deg, px, py, hx, hy, dx, dy, lx, ly;
    setup();
    fg.bSaberBtn = 1;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    g_case = "shape: FILM under the RIGHT card, right of the ring; SPIKE (off) to start; drawn as FILM with a switch";
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &hx, &hy) && touchUi_GetSaberChip(TOUCHUI_CHIP_CARD, &dx, &dy));
    CHECK(fabsf(hx - dx) < 0.01f && hy > dy + 39.0f + 10.0f && hx > cx + r1 && fg.saberShape == 0 && drawnText("FILM"));
    g_case = "shape: tapped: BEAM, one tick, the wheel stays open (to tap), the game still held; again: SPIKE";
    g_platTicks = 0;
    down(2, hx, hy); up(2, hx, hy);
    CHECK(fg.saberShape == 1 && g_platTicks == 1 && touchUi_IsWheelOpen() && touchUi_IsWheelTapMode() && fg.bHold);
    down(2, hx, hy); up(2, hx, hy);
    CHECK(fg.saberShape == 0 && touchUi_IsWheelOpen());
    g_case = "shape: the look's switch leaves it; it leaves the look (both looks)";
    touchUi_GetSaberChip(TOUCHUI_CHIP_LOOK, &lx, &ly);
    {
        int bOk = 1;
        for (int k = 0; k < 4; k++) {
            const int lk0 = fg.saberLook, sh0 = fg.saberShape;
            down(2, hx, hy); up(2, hx, hy);
            if (fg.saberShape == sh0 || fg.saberLook != lk0) bOk = 0;
            down(2, lx, ly); up(2, lx, ly);
            if (fg.saberLook == lk0 || fg.saberShape == sh0) bOk = 0;
        }
        CHECK(bOk && touchUi_IsWheelOpen());
    }
    g_case = "shape: every slice still taps its colour, the shape unchanged (both looks)";
    {
        int bOk = 1;
        for (int lk = 0; lk < 2; lk++) {
            if (fg.saberLook != lk) { down(2, lx, ly); up(2, lx, ly); }
            const int sh0 = fg.saberShape;
            for (int k = 0; touchUi_GetWheelSliceBin(k) >= 0; k++) {
                touchUi_GetWheelSlice(touchUi_GetWheelSliceBin(k), &deg);
                // (the middle of the slice, and its outer edge too, nearest the switch)
                const float aR[2] = { (r0 + r1) * 0.5f, r1 - 2.0f };
                for (int j = 0; j < 2; j++) {
                    SC(0, 0) = -1;
                    polar(cx, cy, deg, aR[j], &px, &py);
                    down(2, px, py); up(2, px, py);
                    if (SC(0, 0) != touchUi_GetWheelSliceBin(k) || fg.saberShape != sh0 || !touchUi_IsWheelOpen()) bOk = 0;
                }
            }
        }
        CHECK(bOk);
    }
    g_case = "shape: a tap just off the switch, toward the ring: nothing changes";
    {
        const int sh0 = fg.saberShape;
        down(2, hx - 32.0f - 9.0f, hy); up(2, hx - 32.0f - 9.0f, hy);
        CHECK(fg.saberShape == sh0);
    }
    g_case = "shape: a slide from SABER: one tick per slice; lifted in the gap: cancelled, the shape unchanged";
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen() && !fg.bHold);
    {
        const int sh0 = fg.saberShape, n = fg.saberLook ? 16 : 17;
        down(1, sx, sy);
        move(1, sx, sy + 10.5f);
        g_platTicks = 0;
        for (int k = 0; k < n; k++) {
            touchUi_GetWheelSlice(touchUi_GetWheelSliceBin(k), &deg);
            polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
            move(1, px, py);
        }
        const int nTicks = g_platTicks;
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        move(1, px, py); up(1, px, py);
        CHECK(nTicks == n && !touchUi_IsWheelOpen() && fg.saberShape == sh0);
    }
    g_case = "shape: the shared wheel's own call (a pad's D-pad up, the 5 key) flips it, the wheel open";
    {
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        const int sh0 = fg.saberShape;
        inputWheel_SaberChip(INPUTWHEEL_CHIP_SHAPE);
        CHECK(fg.saberShape == !sh0 && inputWheel_IsOpen());
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
    }
    g_case = "shape: no switch on the Force wheel";
    {
        float wx, wy;
        bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
        down(1, wx, wy); wait(0.35); up(1, wx, wy);
        CHECK(touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, NULL, NULL) && inputWheel_ChipAt(hx, hy) < 0);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
    }
    g_case = "shape: remembered (by name) over a restart";
    {
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &hx, &hy);
        down(2, hx, hy); up(2, hx, hy);
        const int sh = fg.saberShape;
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
        fg.saberShape = !sh;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.saberShape == sh);
    }
    // where it goes, in every state, on phones and tablets
    static const struct { const char* name; float w, h, l, t, r, b; } aScr[] = {
        { "iPhone-sized (852x393, notch)", 852, 393, 59, 0, 59, 21 },
        { "Android phone (873x393, cutout left)", 873, 393, 32, 0, 0, 0 },
        { "Android phone (873x393, cutout right)", 873, 393, 0, 0, 32, 0 },
        { "Android phone (732x360)", 732, 360, 0, 0, 0, 0 },
        { "small phone (640x360)", 640, 360, 0, 0, 0, 0 },
        { "SE-sized (568x320)", 568, 320, 0, 0, 0, 0 },
        { "SE-sized (667x375)", 667, 375, 0, 0, 0, 0 },
        { "tablet (1067x667, nav bar)", 1067, 667, 0, 0, 0, 40 },
        { "iPhone mini, Display Zoom (693x320)", 693, 320, 44, 0, 44, 21 },
    };
    for (int k = 0; k < (int)(sizeof(aScr) / sizeof(aScr[0])); k++) {
        for (int st = 0; st < 6; st++) {
            setup();
            fg.bSaberBtn = 1;
            saberState(st);
            tick();
            touchUi_SetScreen(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b);
            tick();
            bxy(TOUCHUI_BTN_SABER, &sx, &sy);
            down(1, sx, sy); wait(0.1); up(1, sx, sy);
            static char name[160];
            snprintf(name, sizeof(name), "shape: %s, %s: the choices on screen, clear of the ring, each other and the buttons", aScr[k].name, g_aStateName[st]);
            g_case = name;
            CHECK(touchUi_IsWheelOpen() && chipsClear(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b, aScr[k].name));
            touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &hx, &hy);
            const int sh0 = fg.saberShape;
            down(2, hx, hy); up(2, hx, hy);
            CHECK(fg.saberShape == !sh0 && touchUi_IsWheelOpen());
            if (inputWheel_IsOpen()) inputWheel_Close(-1);
            tick();
        }
    }
}

// THROW (Jedi Knight's Saber Throw, iosSaberThrowJk): a switch at the top right
// of the saber wheel, beside the ring; clear of the buttons, the ring, the other
// choices (with one saber or two) and the screen's edges, at one spot whatever
// the style; a tap switches it (a tick, the wheel open), remembered, a pad's or
// the keys' too; not in Mysteries of the Sith or multiplayer. With it on (or the
// cheat), SABER THROW on the Force wheel: fifteen slices, it at the top between
// SPEED and SEEING; with it off, the fourteen as ever.
static int throwClear(float W, float H, float l, float t, float r, float b, const char* what, float* pX, float* pY)
{
    float cx, cy, r0, r1, x, y;
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    if (!touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, &x, &y)) { printf("  %s: no THROW\n", what); return 0; }
    *pX = x;
    *pY = y;
    const float x0 = x - 32.0f, y0 = y - 14.0f, x1 = x + 32.0f, y1 = y + 14.0f;
    int bOk = 1;
    const float top = fmaxf(t, 8.0f) + 52.0f;
    if (x0 < fmaxf(l, 8.0f) - 0.01f || x1 > W - fmaxf(r, 8.0f) + 0.01f || y0 < top - 0.01f || y1 > H - fmaxf(b, 8.0f) + 0.01f) { printf("  %s: THROW off the safe area\n", what); bOk = 0; }
    if (y1 >= cy || x0 < cx - 0.5f) { printf("  %s: THROW not at the top right\n", what); bOk = 0; }
    const float rx = fmaxf(fmaxf(x0 - cx, cx - x1), 0.0f), ry = fmaxf(fmaxf(y0 - cy, cy - y1), 0.0f);
    if (hypotf(rx, ry) < r1 + 10.0f + 6.0f) { printf("  %s: THROW on the popped slices\n", what); bOk = 0; }
    for (int i = 0; i < TOUCHUI_NUM_CHIPS; i++) {
        float rt[4] = { x0, y0, 64.0f, 28.0f }, ro[4];
        if (i == TOUCHUI_CHIP_THROW || i == TOUCHUI_CHIP_LOOK || !chipRect(i, ro)) continue;
        if (rectGap(rt, ro) < 8.0f - 0.01f) { printf("  %s: THROW %.1f pt from choice %d\n", what, rectGap(rt, ro), i); bOk = 0; }
    }
    for (int k = 0; k < TOUCHUI_BTN_TRAYGYRO; k++) {
        float bx, by, br;
        if (k == TOUCHUI_BTN_SABER || !touchUi_GetButton(k, &bx, &by, &br)) continue;
        const float ex = fmaxf(fmaxf(x0 - bx, bx - x1), 0.0f), ey = fmaxf(fmaxf(y0 - by, by - y1), 0.0f);
        if (hypotf(ex, ey) <= br + 4.0f) { printf("  %s: THROW %.1f pt from button %d\n", what, hypotf(ex, ey) - br, k); bOk = 0; }
    }
    return bOk;
}

static int settingsHave(const char* line)
{
    char buf[4096] = "";
    FILE* f = fopen(g_settings, "r");
    if (!f) return 0;
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    return strstr(buf, line) != NULL;
}

static void test_saber_throw(void)
{
    float sx, sy, cx, cy, r0, r1, px, py, tx, ty, wx, wy, deg;
    setup();
    fg.bSaberBtn = 1;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    g_case = "throw: SABER THROW at the top right of the saber wheel, off to start, drawn as a switch";
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, &tx, &ty) && tx > cx && ty < cy && fg.bThrow == 0 && drawnText("SABER\nTHROW") && drawnText("FILM"));
    g_case = "throw: the switches' names (SABER THROW on two lines, 8 pt further in, condensed and smaller; FILM) in the middle of the room left of the switch, short of the track, as on iOS";
    {
        float nx = 0.0f, ny = 0.0f, ns = 0.0f, nxs = 0.0f, nw = 0.0f, fx = 0.0f, fy = 0.0f, fsz = 0.0f, fw = 0.0f, fcx = 0.0f, fcy = 0.0f;
        int bName = drawnTextAt("FILM", &fx, &fy, &fsz) && touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &fcx, &fcy), bThrowName = 0;
        const touchUiDrawList* dl = touchUi_Draw(T);
        for (int i = 0; i < dl->numItems; i++)
            if (dl->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(dl->aPool + dl->aItems[i].text, "SABER\nTHROW")) {
                nx = dl->aItems[i].x0; ny = dl->aItems[i].y0; ns = dl->aItems[i].size; nxs = dl->aItems[i].xscale; bThrowName = 1;
            }
        touchUiDraw_MeasureText("SABER\nTHROW", ns, nxs, &nw, NULL);
        touchUiDraw_MeasureText("FILM", fsz, 1.0f, &fw, NULL);
        const float boxL = tx - 32.0f + 8.0f, boxR = tx + 32.0f - 7.0f - 20.0f - 2.0f; // (the room: 8 pt in, a name on two lines, 2 short of the track)
        // (its lines' corners inside the round end, 1.5 pt in from its border's inside: the lines 1.19 of the size apart)
        const float far = ns * (2.0f * 0.595f - 0.2496f), ex = (tx - 32.0f + 14.0f) - (nx - nw * 0.5f);
        const float corner = ex > 0.0f ? sqrtf(ex * ex + far * far) : 0.0f;
        printf("  SABER THROW's name: %.1f pt x%.2f, %.1f..%.1f in the room %.1f..%.1f (corner %.2f from the round end's middle); FILM's %.1f pt, its middle %.1f from its chip's\n",
               ns, nxs, nx - nw * 0.5f, nx + nw * 0.5f, boxL, boxR, corner, fsz, fx - fcx);
        CHECK(bName && bThrowName && ns >= 8.0f && ns <= 10.0f && nxs >= 0.82f - 0.001f && nx - nw * 0.5f >= boxL - 0.01f && nx + nw * 0.5f <= boxR + 0.01f
              && fabsf(nx - (boxL + boxR) * 0.5f) < 0.01f && fabsf(ny - ty) < 0.01f && corner <= 14.0f - 1.5f - 1.5f && fsz == 10.0f && fabsf(fx - (fcx - 12.5f)) < 0.01f);
    }
    g_case = "throw: tapped: on, one tick, the wheel stays open (to tap), the game still held, remembered; again: off";
    g_platTicks = 0;
    down(2, tx, ty); up(2, tx, ty);
    CHECK(fg.bThrow == 1 && g_platTicks == 1 && touchUi_IsWheelOpen() && touchUi_IsWheelTapMode() && fg.bHold && settingsHave("saberThrow=1"));
    down(2, tx + 34.0f, ty); up(2, tx + 34.0f, ty); // (its edge: a touch 6 pt round it)
    CHECK(fg.bThrow == 0 && g_platTicks == 2 && touchUi_IsWheelOpen() && settingsHave("saberThrow=0"));
    g_case = "throw: the shared wheel's own call (a pad's D-pad down, the 6 key) flips it, the wheel open";
    inputWheel_SaberChip(INPUTWHEEL_CHIP_THROW);
    CHECK(fg.bThrow == 1 && inputWheel_IsOpen());
    polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
    down(2, px, py); up(2, px, py);
    CHECK(!touchUi_IsWheelOpen());
    g_case = "throw: remembered over a restart, set from the overlay or from outside it (a pad, the keys)";
    tick(); // (a pad's or the keys' change: saved by the overlay's next frame)
    fg.bThrow = 0;
    touchUi_Init(&fakeGame, g_settings);
    CHECK(fg.bThrow == 1);
    tick();
    fg.bThrow = 0; // (a pad's D-pad down with the wheel opened by the pad: only the game's state changes)
    tick();
    fg.bThrow = 1;
    touchUi_Init(&fakeGame, g_settings);
    CHECK(fg.bThrow == 0 && settingsHave("saberThrow=0"));
    g_case = "throw: the Force wheel, THROW off: the 14 slices as ever, no SABER THROW";
    setup();
    fg.bSaberBtn = 1;
    tick();
    bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
    down(1, wx, wy); wait(0.35); up(1, wx, wy);
    {
        int n = 0, b37 = 0;
        for (int k = 0; touchUi_GetWheelSliceBin(k) >= 0; k++) { n++; if (touchUi_GetWheelSliceBin(k) == 37) b37 = 1; }
        CHECK(touchUi_IsWheelOpen() && n == 14 && !b37);
        touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
    }
    g_case = "throw: THROW on: 15 slices of 20.5 degrees, 5 between the sides (the gap at the bottom 42.5, at least the 14's 40), SABER THROW at the top (90) between SPEED and SEEING, the gap cancels, a tick a slice";
    fg.bThrow = 1;
    fg.aPowerLevel[37] = 3;
    down(1, wx, wy); wait(0.35); up(1, wx, wy);
    {
        int n = 0, i37 = -1, iSp = -1, iSe = -1;
        for (int k = 0; touchUi_GetWheelSliceBin(k) >= 0; k++) {
            const int bin = touchUi_GetWheelSliceBin(k);
            n++;
            if (bin == 37) i37 = k;
            if (bin == 22) iSp = k;
            if (bin == 23) iSe = k;
        }
        touchUi_GetWheelSlice(37, &deg);
        const inputWheelMap* m15 = inputWheel_GetOpenMap();
        const float gap15 = 360.0f - m15->numSlices * m15->sliceDeg - 2.0f * m15->gapDeg;
        CHECK(touchUi_IsWheelOpen() && n == 15 && i37 == iSp + 1 && i37 == iSe - 1 && fabsf(deg - 90.0f) < 0.01f && m15->sliceDeg == 20.5f && m15->gapDeg == 5.0f
              && gap15 >= 40.0f && m15->nameMin == 6.5f);
        CHECK(inputWheel_IsPickable(i37)); // (learned: its stars, 3 here)
        touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
        // a slide from FORCE WHEEL round all fifteen: a tick each; lifted in the gap: nothing picked
        down(1, wx, wy);
        move(1, wx, wy + 10.5f);
        g_platTicks = 0;
        for (int k = 0; k < 15; k++) {
            touchUi_GetWheelSlice(touchUi_GetWheelSliceBin(k), &deg);
            polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
            move(1, px, py);
        }
        const int nTicks = g_platTicks;
        const int cur0 = fg.curPower;
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        move(1, px, py); up(1, px, py);
        CHECK(nTicks == 15 && !touchUi_IsWheelOpen() && fg.curPower == cur0);
        // tapped: SABER THROW picked
        down(1, wx, wy); wait(0.35); up(1, wx, wy);
        touchUi_GetWheelSlice(37, &deg);
        polar(cx, cy, deg, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
        CHECK(!touchUi_IsWheelOpen() && fg.curPower == 37);
    }
    g_case = "throw: the cheat on, THROW off: SABER THROW on the Force wheel all the same";
    fg.bThrow = 0;
    fg.bCheat = 1;
    down(1, wx, wy); wait(0.35); up(1, wx, wy);
    {
        int n = 0;
        for (int k = 0; touchUi_GetWheelSliceBin(k) >= 0; k++) n++;
        CHECK(touchUi_IsWheelOpen() && n == 15);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        down(2, px, py); up(2, px, py);
    }
    fg.bCheat = 0;
    g_case = "throw: Mysteries of the Sith and multiplayer: no THROW (nor from a pad)";
    {
        setup();
        fg.bSaberBtn = 1;
        fg.bMots = 1;
        tick();
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        const int bMots = touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, NULL, NULL);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_THROW);
        const int bMotsPad = fg.bThrow == 0;
        if (inputWheel_IsOpen()) inputWheel_Close(-1);
        tick();
        setup();
        fg.bSaberBtn = 1;
        fg.bThrowHidden = 1;
        tick();
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        const int bMulti = touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, NULL, NULL);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_THROW);
        CHECK(bMots && bMotsPad && bMulti && fg.bThrow == 0);
        if (inputWheel_IsOpen()) inputWheel_Close(-1);
        tick();
    }
    g_case = "throw: no room for THROW (offered, but not shown): the pad's and keyboard's key do nothing either";
    {
        // (a screen with the right half all inset: THROW nowhere to go)
        setup();
        fg.bSaberBtn = 1;
        tick();
        touchUi_SetScreen(852, 393, 0, 0, 440, 21);
        tick();
        bxy(TOUCHUI_BTN_SABER, &sx, &sy);
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        const int bHidden = touchUi_IsWheelOpen() && !touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, NULL, NULL) && iosSaberThrowJk_Offered();
        inputWheel_SaberChip(INPUTWHEEL_CHIP_THROW);
        printf("  no room: wheel open %d, THROW shown %d, THROW after the key %d\n", touchUi_IsWheelOpen(), touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, NULL, NULL), fg.bThrow);
        CHECK(bHidden && fg.bThrow == 0);
        if (inputWheel_IsOpen()) inputWheel_Close(-1);
        touchUi_SetScreen(852, 393, 59, 0, 59, 21);
        tick();
    }
    // where it goes, on phones and tablets: one spot in every style, clear of everything
    static const struct { const char* name; float w, h, l, t, r, b; } aScr[] = {
        { "iPhone-sized (852x393, notch)", 852, 393, 59, 0, 59, 21 },
        { "Android phone (873x393, cutout left)", 873, 393, 32, 0, 0, 0 },
        { "Android phone (873x393, cutout right)", 873, 393, 0, 0, 32, 0 },
        { "Android phone (732x360)", 732, 360, 0, 0, 0, 0 },
        { "small phone (640x360)", 640, 360, 0, 0, 0, 0 },
        { "SE-sized (568x320)", 568, 320, 0, 0, 0, 0 },
        { "SE-sized (667x375)", 667, 375, 0, 0, 0, 0 },
        { "tablet (1067x667, nav bar)", 1067, 667, 0, 0, 0, 40 },
        { "iPhone mini, Display Zoom (693x320)", 693, 320, 44, 0, 44, 21 },
    };
    // (THROW's spot before the hand cards, each screen's: where it stays)
    static const float aWas[][2] = { { 575, 61 }, { 595, 61 }, { 571, 61 }, { 537, 135 }, { 541, 61 }, { 417, 61 }, { 586, 61 }, { 656, 153 }, { 475, 61 } };
    for (int k = 0; k < (int)(sizeof(aScr) / sizeof(aScr[0])); k++) {
        float x0 = -1.0f, y0 = -1.0f;
        int bSame = 1, bClear = 1;
        for (int st = 0; st < 6; st++) {
            setup();
            fg.bSaberBtn = 1;
            saberState(st);
            tick();
            touchUi_SetScreen(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b);
            tick();
            bxy(TOUCHUI_BTN_SABER, &sx, &sy);
            down(1, sx, sy); wait(0.1); up(1, sx, sy);
            float x, y;
            if (!touchUi_IsWheelOpen() || !throwClear(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b, aScr[k].name, &x, &y)) bClear = 0;
            if (st == 0) { x0 = x; y0 = y; }
            else if (fabsf(x - x0) > 0.01f || fabsf(y - y0) > 0.01f) bSame = 0;
            if (st == 1) {
                // (and it works there)
                const int b0 = fg.bThrow;
                down(2, x, y); up(2, x, y);
                if (fg.bThrow == b0 || !touchUi_IsWheelOpen()) bClear = 0;
            }
            if (inputWheel_IsOpen()) inputWheel_Close(-1);
            tick();
        }
        static char name[160];
        snprintf(name, sizeof(name), "throw: %s: THROW at one spot in every state (%.0f,%.0f, where it was before the cards), clear of the ring, the other choices, the buttons and the edges", aScr[k].name, x0 - 32.0f, y0 - 14.0f);
        g_case = name;
        printf("  %s: THROW at %.0f,%.0f (was %.0f,%.0f)\n", aScr[k].name, x0 - 32.0f, y0 - 14.0f, aWas[k][0], aWas[k][1]);
        CHECK(bSame && bClear && fabsf(x0 - 32.0f - aWas[k][0]) < 0.01f && fabsf(y0 - 14.0f - aWas[k][1]) < 0.01f);
    }
}

// HILT: each card's own button (NORMAL, CROSS, STAFF, FOLD: that hand's
// next available, round; SAME HILT: both hands'); its name drawn; remembered
// by name, set from outside too (a pad, the keys); a CROSS kept as a style by
// builds before the hilts moved to SINGLE with the CROSS hilt and its colour;
// on small screens each touch on a choice its own
static void test_saber_hilt(void)
{
    float sx, sy, cx, cy, r0, r1, xx, xy, dx, dy, hx, hy;
    setup();
    fg.bSaberBtn = 1;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    g_case = "hilt: the RIGHT card's HILT button, in the card under its colour; its hilt's name (NORMAL) drawn inside it, at 11 pt";
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(HILT(0), &xx, &xy) && touchUi_GetSaberChip(CARD(0), &dx, &dy)
          && touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &hx, &hy));
    {
        float tx = 0.0f, ty = 0.0f, ts = 0.0f, tw = 0.0f, hr[4];
        const int bName = drawnTextAt("NORMAL", &tx, &ty, &ts);
        touchUiDraw_MeasureText("NORMAL", ts, 1.0f, &tw, NULL);
        chipRect(HILT(0), hr);
        printf("  NORMAL drawn at %.1f pt, %.1f wide in the 88 pt HILT button\n", ts, tw);
        CHECK(fabsf(xx - dx) < 0.01f && xy > dy + 10.0f && hy > xy + 20.0f && xx > cx + r1 && bName && ts == 11.0f
              && tx - tw * 0.5f > hr[0] + 6.0f && tx + tw * 0.5f < hr[0] + hr[2] - 6.0f && fabsf(ty - xy) < 0.01f && drawnText("HILT"));
    }
    g_case = "hilt: tapped: CROSS (its name drawn), one saber kept, one tick, the wheel open to tap, the game held";
    g_platTicks = 0;
    CHECK(tapChip(HILT(0)));
    CHECK(fg.aHilt[0] == 1 && fg.aHilt[1] == 0 && fg.saberStyle == TOUCHUI_SABER_SINGLE && g_platTicks == 1 && touchUi_IsWheelOpen() && touchUi_IsWheelTapMode()
          && fg.bHold && drawnText("CROSS") && !drawnText("NORMAL"));
    CHECK(!touchUi_GetSaberChip(CARD(1), NULL, NULL) && !touchUi_GetSaberChip(HILT(1), NULL, NULL));
    g_case = "hilt: nothing else moved with it";
    {
        float ax, ay;
        int bSame = 1;
        bSame &= touchUi_GetSaberChip(CARD(0), &ax, &ay) && fabsf(ax - dx) < 0.01f && fabsf(ay - dy) < 0.01f;
        bSame &= touchUi_GetSaberChip(HILT(0), &ax, &ay) && fabsf(ax - xx) < 0.01f && fabsf(ay - xy) < 0.01f;
        bSame &= touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, &ax, &ay) && fabsf(ax - hx) < 0.01f && fabsf(ay - hy) < 0.01f;
        CHECK(bSame);
    }
    g_case = "hilt: a colour tapped with one CROSS saber: the right hand's (the left hand's kept)";
    SC(0, 0) = 5; SC(1, 1) = 3;
    tapColour(1);
    CHECK(SC(0, 0) == 1 && SC(1, 1) == 3 && fg.aHilt[0] == 1 && touchUi_IsWheelOpen());
    g_case = "hilt: FILM dim with every hilt in use CROSS, and it still flips, the hilt kept";
    {
        const int sh0 = fg.saberShape;
        CHECK(iosSaberWheel_FilmDimmed());
        down(2, hx, hy); up(2, hx, hy);
        CHECK(fg.saberShape == !sh0 && fg.aHilt[0] == 1 && touchUi_IsWheelOpen());
        down(2, hx, hy); up(2, hx, hy);
    }
    g_case = "hilt: two sabers with the right CROSS: the LEFT card its own hilt (NORMAL), FILM no longer dim, HILT where it was";
    CHECK(tapChip(TOUCHUI_CHIP_ADD));
    {
        float ax, ay;
        CHECK(fg.saberStyle == TOUCHUI_SABER_DUAL && fg.aHilt[0] == 1 && fg.aHilt[1] == 0 && touchUi_GetSaberChip(HILT(1), NULL, NULL)
              && touchUi_GetSaberChip(HILT(0), &ax, &ay) && fabsf(ax - xx) < 0.01f && fabsf(ay - xy) < 0.01f && drawnText("CROSS") && drawnText("NORMAL")
              && !iosSaberWheel_FilmDimmed());
    }
    g_case = "hilt: the LEFT card's HILT: only the left hand (CROSS), the right kept";
    CHECK(tapChip(HILT(1)) && fg.aHilt[1] == 1 && fg.aHilt[0] == 1 && iosSaberWheel_FilmDimmed());
    g_case = "hilt: SAME HILT on: a HILT tap changes both hands (NORMAL: round, only two available)";
    g_platTicks = 0;
    CHECK(tapChip(TOUCHUI_CHIP_SAME_HILT) && iosSaberWheel_GetSameHilt() && g_platTicks == 1 && touchUi_IsWheelOpen());
    CHECK(tapChip(HILT(1)) && fg.aHilt[0] == 0 && fg.aHilt[1] == 0 && fg.saberStyle == TOUCHUI_SABER_DUAL);
    CHECK(tapChip(HILT(0)) && fg.aHilt[0] == 1 && fg.aHilt[1] == 1);
    CHECK(tapChip(TOUCHUI_CHIP_SAME_HILT) && !iosSaberWheel_GetSameHilt());
    g_case = "hilt: with STAFF available too: NORMAL, CROSS, STAFF, round, past FOLD (not available)";
    {
        fg.bStaffOk = 1;
        int aSeen[4];
        for (int k = 0; k < 4; k++) { tapChip(HILT(0)); aSeen[k] = fg.aHilt[0]; }
        CHECK(aSeen[0] == 2 && aSeen[1] == 0 && aSeen[2] == 1 && aSeen[3] == 2 && fg.aHilt[1] == 1 && drawnText("STAFF"));
        fg.bStaffOk = 0;
        fg.aHilt[0] = 1;
    }
    g_case = "hilt: the shared wheel's own call (a pad's LB, the 7 key: the picked card's HILT) goes to that hand's next";
    iosSaberWheel_PickHand(0);
    inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT + iosSaberWheel_GetPickedHand());
    CHECK(fg.aHilt[0] == 0 && fg.aHilt[1] == 1 && inputWheel_IsOpen());
    inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT + iosSaberWheel_GetPickedHand());
    CHECK(fg.aHilt[0] == 1 && inputWheel_IsOpen());
    tick(); // (a frame: what a pad or the keys changed, saved)
    g_case = "hilt: the gap cancels; remembered (by name) over a restart, the style and colours too";
    tapColour(-1);
    CHECK(!touchUi_IsWheelOpen() && fg.aHilt[0] == 1 && fg.aHilt[1] == 1 && fg.saberStyle == TOUCHUI_SABER_DUAL);
    fg.aHilt[0] = fg.aHilt[1] = 0; fg.saberStyle = 0; SC(0, 0) = 0;
    touchUi_Init(&fakeGame, g_settings);
    CHECK(fg.aHilt[0] == 1 && fg.aHilt[1] == 1 && fg.saberStyle == TOUCHUI_SABER_DUAL && SC(0, 0) == 1 && settingsHave("saberHiltLeft=CROSS\n"));
    g_case = "hilt: a hilt and a colour set from outside the overlay (a pad, the keys): remembered over a restart";
    {
        setup();
        fg.aHilt[0] = 1;
        tick(); tick(); // (the hilt saved)
        SC(0, 0) = 7; // (then only SINGLE's colour: picked with a pad's A, a click)
        tick();
        fg.aHilt[0] = 0; SC(0, 0) = 0;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.aHilt[0] == 1 && SC(0, 0) == 7);
        tick();
        fg.aHilt[0] = 0; // (then only the hilt: a pad's LB, the 7 key)
        tick();
        fg.aHilt[0] = 1;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.aHilt[0] == 0 && SC(0, 0) == 7);
    }
    g_case = "hilt: a CROSS kept as a style (builds before the hilts): SINGLE, the right hand CROSS in CROSS's colour, the left DUAL's left one, the file kept anew (the published build's keys too, Style CROSS)";
    {
        static const char* aFile[2] = { "saberStyle=CROSS\nsaberColour.SINGLE.0=c5\nsaberColour.DUAL.0=c3\nsaberColour.DUAL.1=c4\nsaberColour.CROSS.0=c9\n",
                                        "saberColour.CROSS.0=c9\nsaberColour.SINGLE.0=c5\nsaberColour.DUAL.0=c3\nsaberColour.DUAL.1=c4\nsaberStyle=CROSS\n" };
        for (int k = 0; k < 2; k++) {
            setup();
            FILE* f = fopen(g_settings, "w");
            if (f) { fputs(aFile[k], f); fclose(f); }
            touchUi_Init(&fakeGame, g_settings);
            char buf[2048] = "";
            f = fopen(g_settings, "r");
            if (f) { buf[fread(buf, 1, sizeof(buf) - 1, f)] = 0; fclose(f); }
            printf("  %s: style %d hilts %d %d colours %d/%d; kept: %s%s%s\n", k ? "colour line first" : "as kept", fg.saberStyle, fg.aHilt[0], fg.aHilt[1],
                   SC(0, 0), SC(1, 1), strstr(buf, "saberStyle=CROSS") ? "saberStyle=CROSS " : "",
                   strstr(buf, "saberHiltRight=CROSS") ? "saberHiltRight=CROSS " : "", strstr(buf, "saberColourRight1=c9") ? "saberColourRight1=c9" : "");
            CHECK(fg.saberStyle == TOUCHUI_SABER_SINGLE && fg.aHilt[0] == 1 && fg.aHilt[1] == 0 && SC(0, 0) == 9 && SC(1, 1) == 4
                  && strstr(buf, "saberStyle=CROSS\n") && strstr(buf, "saberHiltRight=CROSS\n") && strstr(buf, "saberColourRight1=c9\n")
                  && strstr(buf, "saberColourLeft1=c4\n") && !strstr(buf, "saberStyle=SINGLE") && strstr(buf, "saberColour.SINGLE.0=c9\n")
                  && strstr(buf, "saberColour.CROSS.0=c9\n") && strstr(buf, "saberColour.DUAL.0=c9\n") && strstr(buf, "saberColour.DUAL.1=c4\n"));
        }
    }
    g_case = "hilt: SINGLE kept with an old CROSS colour too: SINGLE's own colour kept, the hilt NORMAL";
    {
        setup();
        FILE* f = fopen(g_settings, "w");
        if (f) { fputs("saberStyle=SINGLE\nsaberColour.SINGLE.0=c5\nsaberColour.CROSS.0=c9\n", f); fclose(f); }
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.saberStyle == TOUCHUI_SABER_SINGLE && fg.aHilt[0] == 0 && SC(0, 0) == 5);
    }
    g_case = "hilt: an unknown style's or hilt's name, or one not available here: left as it was (SINGLE, NORMAL)";
    {
        static const char* aFile[3] = { "saberStyle=WOBBLY\nsaberColour.WOBBLY.0=c1\n", "saberHilt=STAFF\n", "saberHilt=SPOON\nsaberStyle=NORMAL\n" };
        int bOk = 1;
        for (int k = 0; k < 3; k++) {
            setup();
            FILE* f = fopen(g_settings, "w");
            if (f) { fputs(aFile[k], f); fclose(f); }
            touchUi_Init(&fakeGame, g_settings);
            bOk &= fg.saberStyle == TOUCHUI_SABER_SINGLE && fg.aHilt[0] == 0 && SC(0, 0) == 0;
        }
        CHECK(bOk);
    }
    g_case = "hilt: a CROSS kept as a style where CROSS isn't available: SINGLE, NORMAL, in CROSS's colour";
    {
        setup();
        fg.bCrossOk = 0;
        FILE* f = fopen(g_settings, "w");
        if (f) { fputs("saberStyle=CROSS\nsaberColour.CROSS.0=c9\n", f); fclose(f); }
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.saberStyle == TOUCHUI_SABER_SINGLE && fg.aHilt[0] == 0 && SC(0, 0) == 9);
    }
    g_case = "hilt: not offered with only one hilt available (FILM still there)";
    setup();
    fg.bSaberBtn = 1; fg.bCrossOk = 0;
    tick();
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && !touchUi_GetSaberChip(HILT(0), NULL, NULL) && touchUi_GetSaberChip(TOUCHUI_CHIP_SHAPE, NULL, NULL)
          && touchUi_GetSaberChip(CARD(0), NULL, NULL));
    inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT);
    CHECK(fg.aHilt[0] == 0);
    inputWheel_SaberChip(INPUTWHEEL_CHIP_ADD);
    touchUi_Draw(T); // (laid out again as it is drawn)
    CHECK(fg.saberStyle == TOUCHUI_SABER_DUAL && !touchUi_GetSaberChip(HILT(1), NULL, NULL) && !touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_HILT, NULL, NULL)
          && touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_COLOUR, NULL, NULL));
    if (inputWheel_IsOpen()) inputWheel_Close(-1);
    fg.bCrossOk = 1;
    // small screens: every choice shown, each touch on one its own (the x, HILT
    // and the dots before their card), none moving as HILT is tapped
    static const struct { const char* name; float w, h, l, t, r, b; } aScr[] = {
        { "SE-sized (568x320)", 568, 320, 0, 0, 0, 0 },
        { "iPhone mini, Display Zoom (693x320)", 693, 320, 44, 0, 44, 21 },
        { "small phone (640x360)", 640, 360, 0, 0, 0, 0 },
        { "squat phone (640x300)", 640, 300, 0, 0, 0, 0 },
    };
    for (int k = 0; k < (int)(sizeof(aScr) / sizeof(aScr[0])); k++) {
        setup();
        fg.bSaberBtn = 1;
        saberState(4); // (two: STAFF, its blades each their own; FOLD, linked: AUTO shown)
        tick();
        touchUi_SetScreen(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b);
        tick();
        bxy(TOUCHUI_BTN_SABER, &sx, &sy);
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        int bAll = 1, bOwn = 1;
        for (int i = 0; i < TOUCHUI_NUM_CHIPS; i++) {
            if (i == TOUCHUI_CHIP_ADD || i == TOUCHUI_CHIP_LOOK) continue;
            float rc[4];
            if (!chipRect(i, rc)) { if (i != TOUCHUI_CHIP_THROW || aScr[k].h > 300) { printf("  %s: %d not shown\n", aScr[k].name, i); bAll = 0; } continue; }
            for (float y = rc[1] + 0.5f; y < rc[1] + rc[3]; y += 2.0f)
                for (float x = rc[0] + 0.5f; x < rc[0] + rc[2]; x += 2.0f) {
                    int want = i;
                    // (inside a card: its x, HILT or dot where one is)
                    for (int j = TOUCHUI_CHIP_HILT; j <= TOUCHUI_CHIP_REMOVE && i < TOUCHUI_CHIP_HILT; j++) {
                        float rj[4];
                        if (chipRect(j, rj) && x >= rj[0] && x <= rj[0] + rj[2] && y >= rj[1] && y <= rj[1] + rj[3] && want == i) want = j;
                    }
                    if (inputWheel_ChipAt(x, y) != want) { if (bOwn) printf("  %s: %.1f,%.1f is %d, not %d\n", aScr[k].name, x, y, inputWheel_ChipAt(x, y), want); bOwn = 0; }
                }
        }
        static char name[160];
        snprintf(name, sizeof(name), "hilt: %s, two sabers (STAFF, FOLD): every choice shown, each touch its own, clear", aScr[k].name);
        g_case = name;
        CHECK(bAll && bOwn && chipsClear(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b, aScr[k].name));
        {
            // HILT tapped: none of them moves
            float aR[TOUCHUI_NUM_CHIPS][4], rc[4];
            int aOn[TOUCHUI_NUM_CHIPS];
            for (int i = 0; i < TOUCHUI_NUM_CHIPS; i++) aOn[i] = chipRect(i, aR[i]);
            fg.bStaffOk = 0; // (STAFF to FOLD: the dots the same, AUTO stays)
            tapChip(HILT(0));
            int bStill = fg.aHilt[0] == 3 && fg.aHilt[1] == 3;
            for (int i = 0; i < TOUCHUI_NUM_CHIPS; i++)
                if (aOn[i]) bStill &= chipRect(i, rc) && fabsf(rc[0] - aR[i][0]) < 0.01f && fabsf(rc[1] - aR[i][1]) < 0.01f;
            snprintf(name, sizeof(name), "hilt: %s, two sabers: HILT tapped (STAFF to FOLD), no choice moves", aScr[k].name);
            g_case = name;
            CHECK(bStill);
        }
        if (inputWheel_IsOpen()) inputWheel_Close(-1);
        tick();
    }
}

static int g_layoutSaber = 1;
static void check_layout(const char* name, float W, float H, float l, float t, float r, float b)
{
    g_case = name;
    setup();
    fg.aItem[40] = fg.aItem[41] = fg.aItem[42] = 1;
    fg.bCheat = 1;
    fg.bSaberBtn = g_layoutSaber; // (CAMERA, SABER, CHEAT and VIEW all on the top row)
    tick();
    touchUi_SetScreen(W, H, l, t, r, b);
    float mx, my;
    bxy(TOUCHUI_BTN_MENU, &mx, &my);
    down(1, mx, my); wait(0.5); up(1, mx, my); // the tray: CHEAT and VIEW on
    downOn(2, TOUCHUI_BTN_TRAYVIEW); upOn(2, TOUCHUI_BTN_TRAYVIEW);
    tick();
    float aX[TOUCHUI_BTN_COUNT], aY[TOUCHUI_BTN_COUNT], aR[TOUCHUI_BTN_COUNT];
    int aShown[TOUCHUI_BTN_COUNT];
    for (int i = 0; i < TOUCHUI_BTN_TRAYGYRO; i++) aShown[i] = touchUi_GetButton(i, &aX[i], &aY[i], &aR[i]);
    for (int i = 0; i < TOUCHUI_BTN_TRAYGYRO; i++) {
        if (!aShown[i]) continue;
        // inside the safe area (8 pt in from an edge with no inset); the
        // cluster may reach into a side inset with nothing in it, as on iOS
        CHECK(aY[i] - aR[i] >= (t > 8 ? t : 8) - 0.5f);
        CHECK(aY[i] + aR[i] <= H - (b > 8 ? b : 8) + 0.5f);
        CHECK(aX[i] - aR[i] >= 0.0f && aX[i] + aR[i] <= W);
        if (i > TOUCHUI_BTN_FORCE) {
            CHECK(aX[i] - aR[i] >= (l > 8 ? l : 8) - 0.5f);
            CHECK(aX[i] + aR[i] <= W - (r > 8 ? r : 8) + 0.5f);
        }
        for (int j = 0; j < i; j++) {
            if (!aShown[j]) continue;
            float d = hypotf(aX[i] - aX[j], aY[i] - aY[j]);
            if (d < aR[i] + aR[j]) { printf("  overlap: %d and %d\n", i, j); }
            CHECK(d >= aR[i] + aR[j]);
        }
    }
}

// Wedges in what the overlay draws (a wheel's slices)
static int drawnWedges(void)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    int n = 0;
    for (int i = 0; i < l->numItems; i++) n += l->aItems[i].type == TOUCHUI_DRAW_WEDGE;
    return n;
}

// The wheels are the shared ones (inputWheel.c): a pad or the keys open
// them too, one at a time
static void test_shared_wheel(void)
{
    float wx, wy, deg, px, py;
    setup();
    bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);

    g_case = "shared wheel: a finger's wheel is the shared one, drawn here once";
    down(1, wx, wy);
    move(1, wx + 10.0f, wy);
    CHECK(touchUi_IsWheelOpen() && inputWheel_IsOpen() && inputWheel_GetKind() == INPUTWHEEL_FORCE);
    CHECK(fg.bHold == 1);
    CHECK(drawnWedges() == 14);
    g_case = "shared wheel: no other wheel opens over it (a pad, the keys)";
    CHECK(!inputWheel_Open(INPUTWHEEL_WEAPONS, 0));
    g_case = "shared wheel: the finger's slice is the wheel's; a tick on it";
    const int slice = touchUi_GetWheelSlice(24, &deg); // (Pull)
    polar(wx, wy, deg, 40, &px, &py);
    takeHaptics();
    move(1, px, py);
    CHECK(inputWheel_GetHot() == slice && touchUi_GetWheelHot() == slice);
    CHECK(takeHaptics() & TOUCHUI_HAPTIC_TICK);
    up(1, px, py);
    CHECK(!inputWheel_IsOpen() && !touchUi_IsWheelOpen());
    CHECK(fg.selectedPower == 24 && fg.bHold == 0);

    g_case = "shared wheel: a pad's wheel open: fingers leave it alone, nothing drawn here";
    CHECK(inputWheel_Open(INPUTWHEEL_WEAPONS, 1));
    CHECK(fg.bHold == 1);
    down(2, wx, wy);
    wait(0.4);
    move(2, wx + 20.0f, wy);
    up(2, wx + 20.0f, wy);
    CHECK(!touchUi_IsWheelOpen());
    CHECK(inputWheel_IsOpen() && inputWheel_GetKind() == INPUTWHEEL_WEAPONS);
    CHECK(presses(SC_E, 6) == 0);
    CHECK(drawnWedges() == 0);
    inputWheel_Close(-1);
    CHECK(fg.bHold == 0);
    g_case = "shared wheel: closed, the button works again";
    down(3, wx, wy);
    wait(0.1);
    up(3, wx, wy);
    CHECK(presses(SC_E, 6) == 1);

    g_case = "shared wheel: the card picked is the wheel's (the pad's too): the overlay shows it, and a tap picks it back";
    fg.bSaberBtn = 1;
    fg.saberStyle = 1;
    tick();
    iosSaberWheel_PickHand(1); // (as the pad set it)
    float sx, sy, cx, cy;
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    down(4, sx, sy);
    wait(0.1);
    up(4, sx, sy);
    CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelKind() == TOUCHUI_WHEEL_SABER && drawnText("colour for the left saber"));
    CHECK(touchUi_GetSaberChip(TOUCHUI_CHIP_CARD, &cx, &cy));
    down(5, cx, cy);
    up(5, cx, cy);
    CHECK(iosSaberWheel_GetPickedHand() == 0 && drawnText("colour for the right saber") && touchUi_IsWheelOpen());
    touchUi_SetShown(0);
    CHECK(!inputWheel_IsOpen() && fg.bHold == 0);
}

// ---- the hand cards (hiltmock/DESIGN.md)
// Whether a card is drawn outlined in gold (its box's border)
static int cardGold(int hand)
{
    float rc[4];
    if (!chipRect(TOUCHUI_CHIP_CARD + hand, rc)) return 0;
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++) {
        const touchUiDrawItem* it = &l->aItems[i];
        if (it->type == TOUCHUI_DRAW_RRECT && fabsf(it->x0 - rc[0]) < 0.01f && fabsf(it->y0 - rc[1]) < 0.01f && fabsf(it->x1 - rc[2]) < 0.01f)
            return it->stroke.b < 0.5f && it->stroke.r > 0.9f && it->lw > 1.9f;
    }
    return 0;
}
// A drawn text's alpha (-1: not drawn)
static float drawnTextAlpha(const char* t)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++)
        if (l->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(l->aPool + l->aItems[i].text, t)) return l->aItems[i].fill.a;
    return -1.0f;
}
// The gold rings drawn round a blade's dot (circles with a gold border and no fill)
static int goldRings(void)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    int n = 0;
    for (int i = 0; i < l->numItems; i++) {
        const touchUiDrawItem* it = &l->aItems[i];
        n += it->type == TOUCHUI_DRAW_CIRCLE && it->fill.a <= 0.0f && it->stroke.r > 0.9f && it->stroke.b < 0.5f && it->r < 8.0f;
    }
    return n;
}

// Whether a kept choice (iosSaberWheel_GetKept) is this
static int keptIs(const char* name, const char* want)
{
    for (int i = 0; i < iosSaberWheel_NumKept(); i++) {
        if (strcmp(iosSaberWheel_KeptName(i), name)) continue;
        char v[IOSSABERWHEEL_KEPT_LEN];
        iosSaberWheel_GetKept(i, v, sizeof(v));
        return !strcmp(v, want);
    }
    return 0;
}

// (the pad's words for the middle, as iosPad_WheelHint's)
static const char* padHint(int bPickable, int bCancel) { return bCancel ? "B to close" : bPickable ? "A to set" : "point the stick"; }
static int listHasText(const touchUiDrawList* l, const char* t)
{
    for (int i = 0; i < l->numItems; i++)
        if (l->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(l->aPool + l->aItems[i].text, t)) return 1;
    return 0;
}

static void test_saber_cards(void)
{
    float sx, sy, x, y, rc[4];
    setup();
    fg.bSaberBtn = 1; fg.bStaffOk = fg.bFoldOk = 1;
    fg.saberStyle = 1;
    fg.aHilt[0] = 2; // (the right hand STAFF, linked; the left NORMAL)
    fg.aBladeColour[0][0] = 1; fg.aBladeColour[1][0] = 5;
    tick();
    bxy(TOUCHUI_BTN_SABER, &sx, &sy);
    tapSaber();
    g_case = "cards: two sabers: the RIGHT card picked to start (gold), the LEFT not; the STAFF's two dots joined, the name once";
    CHECK(touchUi_IsWheelOpen() && iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1 && cardGold(0) && !cardGold(1));
    CHECK(touchUi_GetSaberChip(DOT(0, 0), NULL, NULL) && touchUi_GetSaberChip(DOT(0, 1), NULL, NULL) && !touchUi_GetSaberChip(DOT(1, 0), NULL, NULL)
          && drawnText("COLOUR 1") && drawnText("COLOUR 5") && drawnText("STAFF") && drawnText("NORMAL") && drawnText("LEFT") && drawnText("RIGHT")
          && drawnText("colour for the right saber") && goldRings() == 0);
    g_case = "cards: a tap on the LEFT card (outside its HILT and x): that hand (gold), a tick, the wheel open; its colour named in the middle";
    chipRect(CARD(1), rc);
    g_platTicks = 0;
    down(2, rc[0] + 20.0f, rc[1] + 32.0f); up(2, rc[0] + 20.0f, rc[1] + 32.0f);
    CHECK(iosSaberWheel_GetPickedHand() == 1 && cardGold(1) && !cardGold(0) && g_platTicks == 1 && touchUi_IsWheelOpen()
          && fg.saberStyle == 1 && drawnText("colour for the left saber"));
    tapColour(9);
    CHECK(SC(1, 1) == 9 && fg.aBladeColour[0][0] == 1 && iosSaber_GetBladeColour(0, 1) == 1);
    g_case = "cards: the STAFF's second dot: just that blade, only picked (still linked: the dots joined), ringed in gold; the hub says blade 2";
    g_platTicks = 0;
    fg.aBladeColour[0][1] = 7; // (its own colour, kept unseen while linked)
    CHECK(tapChip(DOT(0, 1)));
    CHECK(iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == 1 && !fg.aUnlinked[0] && g_platTicks == 1
          && cardGold(0) && goldRings() == 1 && drawnText("right saber, blade 2") && drawnText("COLOUR 1") && !drawnText("COLOUR 7"));
    g_case = "cards: blade 2 of a linked pair given blade 1's colour: still linked, nothing else changed";
    tapColour(1);
    CHECK(!fg.aUnlinked[0] && fg.aBladeColour[0][0] == 1 && fg.aBladeColour[0][1] == 7 && iosSaberWheel_GetPickedBlade() == 1 && goldRings() == 1);
    g_case = "cards: blade 2 of a linked pair given another colour: split, it alone that colour, blade 1 its own";
    tapColour(12);
    CHECK(fg.aUnlinked[0] && fg.aBladeColour[0][0] == 1 && fg.aBladeColour[0][1] == 12 && SC(1, 1) == 9 && iosSaberWheel_GetPickedBlade() == 1
          && goldRings() == 1 && drawnText("right saber, blade 2"));
    g_case = "cards: split, each half of the row its blade's dot and name (condensed); the first dot: blade 1 alone";
    {
        float r0[4], r1[4], c0[4];
        // (the halves the card's whole width, 0..52 and 52..104, as iosTouch.m's; the row from 22 pt down to HILT)
        CHECK(chipRect(DOT(0, 0), r0) && chipRect(DOT(0, 1), r1) && chipRect(CARD(0), c0) && fabsf(r0[0] - c0[0]) < 0.01f && fabsf(r0[2] - 52.0f) < 0.01f
              && fabsf(r1[0] - (c0[0] + 52.0f)) < 0.01f && fabsf(r1[0] + r1[2] - (c0[0] + c0[2])) < 0.01f && fabsf(r0[1] - c0[1] - 22.0f) < 0.01f
              && fabsf(r0[3] - 20.0f) < 0.01f && drawnText("COLOUR 1") && drawnText("COLOUR 12"));
    }
    CHECK(tapChip(DOT(0, 0)) && iosSaberWheel_GetPickedBlade() == 0 && drawnText("right saber, blade 1"));
    tapColour(3);
    CHECK(fg.aBladeColour[0][0] == 3 && fg.aBladeColour[0][1] == 12 && fg.aUnlinked[0]);
    g_case = "cards: the card itself (outside its dots and HILT): the whole hand again; a colour joins its blades again";
    chipRect(CARD(0), rc);
    down(2, rc[0] + rc[2] - 10.0f, rc[1] + 12.0f); up(2, rc[0] + rc[2] - 10.0f, rc[1] + 12.0f);
    CHECK(iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1 && goldRings() == 0 && drawnText("colour for the right saber"));
    tapColour(4);
    CHECK(fg.aBladeColour[0][0] == 4 && fg.aBladeColour[0][1] == 4 && !fg.aUnlinked[0]);
    g_case = "cards: linked, the dots' touch: the first from the card's left to midway between the dots, the second on to where the name starts (31 pt, as iOS's)";
    {
        float d0[4], d1[4], c0[4];
        CHECK(chipRect(DOT(0, 0), d0) && chipRect(DOT(0, 1), d1) && chipRect(CARD(0), c0) && fabsf(d0[0] - c0[0]) < 0.01f && fabsf(d0[0] + d0[2] - (c0[0] + 18.7f)) < 0.01f
              && fabsf(d1[0] - d0[0] - d0[2]) < 0.01f && fabsf(d1[0] + d1[2] - (c0[0] + 31.0f)) < 0.01f && fabsf(d0[1] - c0[1] - 22.0f) < 0.01f && fabsf(d0[3] - 20.0f) < 0.01f);
    }
    g_case = "cards: linked, a tap on the colour's name (31 pt on, over it, under it): the whole hand, still linked; just before it: blade 2";
    {
        float c0[4];
        chipRect(CARD(0), c0);
        static const float aX[] = { 31.5f, 40.0f, 60.0f, 90.0f }, aY[] = { 22.5f, 27.0f, 31.5f, 36.0f, 41.5f };
        int bOk = 1;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 5; j++) {
                tapChip(DOT(0, 1)); // (blade 2 picked to start)
                const float px = c0[0] + aX[i], py = c0[1] + aY[j];
                down(2, px, py); up(2, px, py);
                if (iosSaberWheel_GetPickedHand() != 0 || iosSaberWheel_GetPickedBlade() != -1 || fg.aUnlinked[0]) { printf("  name tap %.1f,%.1f: %d,%d\n", aX[i], aY[j], iosSaberWheel_GetPickedHand(), iosSaberWheel_GetPickedBlade()); bOk = 0; }
            }
        down(2, c0[0] + 30.5f, c0[1] + 31.5f); up(2, c0[0] + 30.5f, c0[1] + 31.5f);
        CHECK(bOk && iosSaberWheel_GetPickedBlade() == 1 && !fg.aUnlinked[0]);
    }
    g_case = "cards: linked, the first dot: blade 1 (ringed), the second follows it";
    CHECK(tapChip(DOT(0, 0)) && iosSaberWheel_GetPickedBlade() == 0 && !fg.aUnlinked[0] && goldRings() == 1);
    tapColour(6);
    CHECK(fg.aBladeColour[0][0] == 6 && iosSaber_GetBladeColour(0, 1) == 6 && !fg.aUnlinked[0]);
    g_case = "cards: SAME COLOUR under the LEFT card: on, every blade of both hands takes a colour; no dot ringed; off again: the pick as it was";
    {
        float a[4], b[4], c[4];
        CHECK(chipRect(CARD(1), a) && chipRect(TOUCHUI_CHIP_SAME_COLOUR, b) && chipRect(TOUCHUI_CHIP_SAME_HILT, c) && fabsf(a[0] - b[0]) < 0.01f
              && fabsf(b[0] - c[0]) < 0.01f && b[1] >= a[1] + a[3] + 9.99f && c[1] >= b[1] + b[3] + 5.99f && drawnText("SAME COLOUR") && drawnText("SAME HILT"));
    }
    tapChip(DOT(0, 1)); // (blade 2 picked, linked)
    g_platTicks = 0;
    CHECK(tapChip(TOUCHUI_CHIP_SAME_COLOUR) && iosSaberWheel_GetSameColour() && g_platTicks == 1 && touchUi_IsWheelOpen() && goldRings() == 0
          && drawnText("colour for both sabers") && settingsHave("saberSameColour=1\n"));
    CHECK(tapChip(TOUCHUI_CHIP_SAME_COLOUR) && !iosSaberWheel_GetSameColour() && iosSaberWheel_GetPickedBlade() == 1 && goldRings() == 1); // (off with no colour between: as it was)
    tapChip(TOUCHUI_CHIP_SAME_COLOUR);
    tapColour(14);
    CHECK(fg.aBladeColour[0][0] == 14 && iosSaber_GetBladeColour(0, 1) == 14 && SC(1, 1) == 14 && iosSaber_GetBladeColour(1, 1) == 14);
    g_case = "cards: SAME COLOUR off after a colour (every pair linked again): the pick the whole hand, its dots joined, no ring; a colour paints both blades";
    CHECK(tapChip(TOUCHUI_CHIP_SAME_COLOUR) && !iosSaberWheel_GetSameColour() && iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1
          && goldRings() == 0 && !fg.aUnlinked[0] && drawnText("colour for the right saber"));
    tapColour(5);
    CHECK(fg.aBladeColour[0][0] == 5 && iosSaber_GetBladeColour(0, 1) == 5 && !fg.aUnlinked[0] && SC(1, 1) == 14);
    g_case = "cards: SAME COLOUR on: a tap on a linked pair's dot is a tap on its card (the hand, no split, no ring)";
    tapChip(TOUCHUI_CHIP_SAME_COLOUR);
    tapChip(CARD(1));
    g_platTicks = 0;
    CHECK(tapChip(DOT(0, 1)) && iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1 && !fg.aUnlinked[0] && g_platTicks == 1 && goldRings() == 0
          && cardGold(0) && drawnText("colour for both sabers"));
    CHECK(tapChip(DOT(0, 0)) && iosSaberWheel_GetPickedBlade() == -1 && !fg.aUnlinked[0]);
    tapChip(TOUCHUI_CHIP_SAME_COLOUR);
    CHECK(!iosSaberWheel_GetSameColour() && iosSaberWheel_GetPickedBlade() == -1 && goldRings() == 0);
    tapChip(DOT(0, 1)); // (blade 2 picked again, for below)
    g_case = "cards: the LEFT card's x: one saber (the left hand's kept); the pick back to the RIGHT card; + 2ND SABER in its place";
    tapChip(CARD(1));
    {
        float xr[4];
        // (40 x 22: under it, from 22 pt down, the colours' row)
        CHECK(chipRect(TOUCHUI_CHIP_REMOVE, xr) && chipRect(CARD(1), rc) && fabsf(xr[2] - 40.0f) < 0.01f && fabsf(xr[3] - 22.0f) < 0.01f
              && fabsf(xr[0] + xr[2] - rc[0] - rc[2]) < 0.01f && fabsf(xr[1] - rc[1]) < 0.01f);
    }
    fg.aHilt[1] = 3; // (FOLD, to see it kept)
    g_platTicks = 0;
    CHECK(tapChip(TOUCHUI_CHIP_REMOVE));
    CHECK(fg.saberStyle == 0 && iosSaberWheel_GetPickedHand() == 0 && g_platTicks == 1 && touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_ADD, &x, &y)
          && !touchUi_GetSaberChip(CARD(1), NULL, NULL) && !touchUi_GetSaberChip(TOUCHUI_CHIP_SAME_COLOUR, NULL, NULL) && fg.aHilt[1] == 3 && SC(1, 1) == 14);
    g_case = "cards: + 2ND SABER: two again, the left hand as it was kept (FOLD), the ring painting it";
    CHECK(tapChip(TOUCHUI_CHIP_ADD) && fg.saberStyle == 1 && fg.aHilt[1] == 3 && iosSaberWheel_GetPickedHand() == 1 && cardGold(1) && drawnText("FOLD"));
    g_case = "cards: AUTO OPEN under FILM on the right, only while a FOLD is in either hand; a tap switches it (a tick, saved, open)";
    {
        float f[4], a[4], c[4];
        CHECK(chipRect(TOUCHUI_CHIP_AUTO, a) && chipRect(TOUCHUI_CHIP_SHAPE, f) && chipRect(CARD(0), c) && fabsf(a[0] - f[0]) < 0.01f
              && a[1] >= f[1] + f[3] + 5.99f && fabsf(f[0] + f[2] * 0.5f - c[0] - c[2] * 0.5f) < 0.01f && drawnText("AUTO\nOPEN"));
    }
    g_platTicks = 0;
    CHECK(tapChip(TOUCHUI_CHIP_AUTO) && !fg.bFoldAuto && g_platTicks == 1 && touchUi_IsWheelOpen() && settingsHave("saberFoldAuto=0\n"));
    CHECK(tapChip(TOUCHUI_CHIP_AUTO) && fg.bFoldAuto);
    g_case = "cards: no FOLD in hand (the left back to NORMAL... via its HILT): no AUTO, and its pad key does nothing";
    while (fg.aHilt[1] != 0 && tapChip(HILT(1))) {}
    CHECK(!touchUi_GetSaberChip(TOUCHUI_CHIP_AUTO, NULL, NULL) && !iosSaberWheel_ShowAuto());
    g_platTicks = 0;
    inputWheel_SaberChip(INPUTWHEEL_CHIP_AUTO);
    CHECK(fg.bFoldAuto && g_platTicks == 0);
    g_case = "cards: FOLD in the right hand of one saber: AUTO shown again";
    tapChip(TOUCHUI_CHIP_REMOVE);
    while (fg.aHilt[0] != 3 && tapChip(HILT(0))) {}
    touchUi_Draw(T);
    CHECK(fg.saberStyle == 0 && touchUi_GetSaberChip(TOUCHUI_CHIP_AUTO, NULL, NULL));
    g_case = "cards: FILM dim only while every hilt in use is CROSS";
    {
        while (fg.aHilt[0] != 1 && tapChip(HILT(0))) {}
        const float aCross = drawnTextAlpha("FILM");
        tapChip(HILT(0));
        const float aStaff = drawnTextAlpha("FILM");
        tapChip(TOUCHUI_CHIP_ADD); // (the left NORMAL)
        while (fg.aHilt[0] != 1 && tapChip(HILT(0))) {}
        const float aMixed = drawnTextAlpha("FILM");
        printf("  FILM's alpha: CROSS %.2f, STAFF %.2f, CROSS + NORMAL %.2f\n", aCross, aStaff, aMixed);
        CHECK(aCross > 0.0f && aCross < 0.35f && aStaff > 0.55f && aMixed > 0.55f);
    }
    g_case = "cards: the action buttons hidden while the saber wheel is open (the top row stays); back once it closes";
    {
        const touchUiDrawList* l = touchUi_Draw(T);
        int bFire = 0;
        for (int i = 0; i < l->numItems; i++) bFire |= l->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(l->aPool + l->aItems[i].text, "FIRE");
        CHECK(touchUi_AreActionsHidden() && !bFire && !drawnText("JUMP") && !drawnText("DUCK") && drawnText("MENU"));
        float fx, fy;
        bxy(TOUCHUI_BTN_FIRE, &fx, &fy);
        down(2, fx, fy); up(2, fx, fy); // (a tap where FIRE was: empty space, the wheel closes, nothing changed)
        CHECK(!touchUi_IsWheelOpen() && !touchUi_AreActionsHidden() && drawnText("FIRE") && drawnText("JUMP") && presses(SC_LCTRL, 4) == 0);
    }
    g_case = "cards: the Force wheel keeps its action buttons";
    {
        float wx, wy;
        bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
        down(1, wx, wy); wait(0.35); up(1, wx, wy);
        CHECK(touchUi_IsWheelOpen() && !touchUi_AreActionsHidden() && drawnText("FIRE"));
        tapColour(-1);
    }

    g_case = "SABER with a FOLD lit in hand: a tap flips it (a tap felt), the wheel stays shut";
    fg.aHilt[0] = 3; fg.bFoldLit = 1; fg.bFoldOpen = 0; fg.foldFlips = 0;
    tick(); takeHaptics();
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(fg.foldFlips == 1 && fg.bFoldOpen && !touchUi_IsWheelOpen() && (takeHaptics() & TOUCHUI_HAPTIC_TAP));
    g_case = "SABER with a FOLD lit: a 10 pt slide opens the wheel at once (no flip)";
    down(1, sx, sy); move(1, sx, sy + 10.5f);
    CHECK(touchUi_IsWheelOpen() && !touchUi_IsWheelTapMode() && fg.foldFlips == 1);
    up(1, sx, sy + 10.5f);
    tapColour(-1);
    g_case = "SABER with a FOLD lit: held 0.3 s (its ring filling): the wheel open to tap (no flip)";
    down(1, sx, sy); wait(0.2);
    CHECK(!touchUi_IsWheelOpen() && touchUi_GetRingProgress(TOUCHUI_BTN_SABER) > 0.5f);
    wait(0.11);
    CHECK(touchUi_IsWheelOpen() && touchUi_IsWheelTapMode() && fg.foldFlips == 1);
    up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen());
    tapColour(-1);
    g_case = "SABER with no FOLD lit (folded away, put out): a tap opens the wheel, as ever";
    fg.bFoldLit = 0;
    down(1, sx, sy); wait(0.1); up(1, sx, sy);
    CHECK(touchUi_IsWheelOpen() && fg.foldFlips == 1);
    tapColour(-1);

    g_case = "weapon wheel: the saber in hand picked again: a FOLD lit flips; another weapon: no flip";
    fg.aWeaponOwned[10] = fg.aWeaponSel[10] = 1; fg.curWeapon = 10; fg.bFoldLit = 1;
    {
        float nx, ny;
        bxy(TOUCHUI_BTN_NEXTWPN, &nx, &ny);
        down(1, nx, ny); wait(0.35); up(1, nx, ny);
        CHECK(touchUi_IsWheelOpen() && touchUi_GetWheelKind() == TOUCHUI_WHEEL_WEAPONS);
        tapColour(10);
        CHECK(!touchUi_IsWheelOpen() && fg.foldFlips == 2 && fg.selectedWeapon != 10);
        down(1, nx, ny); wait(0.35); up(1, nx, ny);
        tapColour(2);
        CHECK(!touchUi_IsWheelOpen() && fg.foldFlips == 2 && fg.selectedWeapon == 2);
    }
    fg.curWeapon = 2; fg.bFoldLit = 0;

    // (SABER's tap with a FOLD in each hand: the right one at once; a second tap down within
    // IOSSABERWHEEL_TAP_AGAIN_SECS of the first's lift, the left one too)
#define SABER_TAP() do { down(1, sx, sy); wait(0.1); up(1, sx, sy); } while (0)
    g_case = "SABER, a FOLD in each hand: a tap flips the right one at once, a second tap soon after the left one too";
    fg.saberStyle = 1; fg.aHilt[0] = fg.aHilt[1] = 3; fg.bFoldLit = 1; fg.bFoldOpen = fg.bFoldOpenL = 0; fg.foldFlips = 0;
    tick(); takeHaptics();
    SABER_TAP();
    CHECK(fg.foldFlips == 1 && fg.bFoldOpen && !fg.bFoldOpenL && !touchUi_IsWheelOpen() && (takeHaptics() & TOUCHUI_HAPTIC_TAP));
    wait(0.2); SABER_TAP();
    CHECK(fg.foldFlips == 2 && fg.bFoldOpen && fg.bFoldOpenL && !touchUi_IsWheelOpen() && (takeHaptics() & TOUCHUI_HAPTIC_TAP));
    g_case = "SABER, a FOLD in each hand: a third quick tap starts again, the right one";
    wait(0.2); SABER_TAP();
    CHECK(fg.foldFlips == 3 && !fg.bFoldOpen && fg.bFoldOpenL);
    g_case = "SABER, a FOLD in each hand, one open and one folded: a double tap swaps them";
    wait(0.5); SABER_TAP(); wait(0.3); SABER_TAP();
    CHECK(fg.foldFlips == 5 && fg.bFoldOpen && !fg.bFoldOpenL && !touchUi_IsWheelOpen());
    g_case = "SABER, a FOLD in each hand, one open and one folded: one tap makes them match";
    wait(0.5); SABER_TAP();
    CHECK(fg.foldFlips == 6 && !fg.bFoldOpen && !fg.bFoldOpenL);
    g_case = "SABER, a FOLD in each hand: the second tap down 0.4 s after the first's lift is a first again (the right one)";
    wait(0.5); SABER_TAP(); wait(0.4); SABER_TAP();
    CHECK(fg.foldFlips == 8 && !fg.bFoldOpen && !fg.bFoldOpenL);
    g_case = "SABER, a FOLD in each hand: the second touch held 0.3 s opens the wheel (the left one not flipped)";
    wait(0.5); SABER_TAP(); wait(0.1); down(1, sx, sy); wait(0.31);
    CHECK(touchUi_IsWheelOpen() && fg.foldFlips == 9 && fg.bFoldOpen && !fg.bFoldOpenL);
    up(1, sx, sy); tapColour(-1);
    g_case = "SABER, a FOLD in each hand: the second touch a 10 pt slide opens the wheel at once (no flip)";
    wait(0.5); SABER_TAP(); wait(0.1); down(1, sx, sy); move(1, sx, sy + 10.5f);
    CHECK(touchUi_IsWheelOpen() && fg.foldFlips == 10 && !fg.bFoldOpen && !fg.bFoldOpenL);
    up(1, sx, sy + 10.5f); tapColour(-1);
    g_case = "SABER, one FOLD (the left hand's, a NORMAL in the right): every tap flips it, quick ones too";
    fg.aHilt[0] = 0; fg.foldFlips = 0;
    wait(0.5); SABER_TAP(); wait(0.1); SABER_TAP(); wait(0.1); SABER_TAP();
    CHECK(fg.foldFlips == 3 && fg.bFoldOpenL && !fg.bFoldOpen && !touchUi_IsWheelOpen());
    g_case = "SABER, one FOLD (the right hand's, a STAFF in the left): every quick tap flips it";
    fg.aHilt[0] = 3; fg.aHilt[1] = 2; fg.foldFlips = 0;
    wait(0.5); SABER_TAP(); wait(0.1); SABER_TAP();
    CHECK(fg.foldFlips == 2 && !fg.bFoldOpen && fg.bFoldOpenL);
#undef SABER_TAP
    fg.saberStyle = 0; fg.aHilt[1] = 0; fg.bFoldLit = 0; fg.bFoldOpenL = 0;

    g_case = "cards: one saber: the RIGHT card never outlined in gold; its dot's gold ring all the same";
    setup();
    fg.bSaberBtn = 1; fg.bStaffOk = 1; fg.aHilt[0] = 2;
    tick();
    tapSaber();
    CHECK(touchUi_IsWheelOpen() && !cardGold(0) && goldRings() == 0 && tapChip(DOT(0, 1)) && !cardGold(0) && goldRings() == 1 && iosSaberWheel_GetPickedBlade() == 1);
    tapColour(-1);

    g_case = "pad / keys: the middle in the opener's words with one saber; what the ring paints with two, or a blade picked";
    {
        touchUi_SetShown(0);
        tick();
        fg.saberStyle = 0; fg.aHilt[0] = 2;
        iosSaberWheel_PickHand(0); // (the whole hand: as a pad finds it)
        inputWheel_Open(INPUTWHEEL_SABER, 1);
        inputWheel_SetHintFn(padHint);
        static touchUiDrawList list;
        inputWheelScreen sc = { 853.33f, 480.0f, 0, 0, 0, 0 };
        touchUiDraw_Begin(&list); inputWheel_Draw(&list, &sc);
        const int bOne = listHasText(&list, "point the stick") && !listHasText(&list, "tap a colour");
        iosSaberWheel_PickBlade(0, 1);
        touchUiDraw_Begin(&list); inputWheel_Draw(&list, &sc);
        const int bBlade = listHasText(&list, "right saber, blade 2");
        iosSaberWheel_PickHand(0);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_ADD);
        touchUiDraw_Begin(&list); inputWheel_Draw(&list, &sc);
        const int bTwo = listHasText(&list, "colour for the left saber");
        inputWheel_Point(3, 1, 0.0f);
        touchUiDraw_Begin(&list); inputWheel_Draw(&list, &sc);
        const int bHot = listHasText(&list, "A to set");
        CHECK(bOne && bBlade && bTwo && bHot);
        inputWheel_Close(-1);
        touchUi_SetShown(1);
        tick();
    }

    g_case = "pad / keys (the shared wheel's calls): the next card or blade, round: RIGHT, its blades (STAFF), LEFT, its blades (FOLD); only what is shown";
    setup();
    fg.bSaberBtn = 1; fg.bStaffOk = fg.bFoldOk = 1; fg.saberStyle = 1; fg.aHilt[0] = 2; fg.aHilt[1] = 3;
    tick();
    tapSaber();
    {
        static const int aWant[][2] = { { 0, 0 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, -1 }, { 0, 0 } };
        int bOk = 1;
        g_platTicks = 0;
        for (int k = 0; k < 7; k++) {
            inputWheel_SaberPickNext();
            if (iosSaberWheel_GetPickedHand() != aWant[k][0] || iosSaberWheel_GetPickedBlade() != aWant[k][1]) { printf("  next %d: %d,%d\n", k, iosSaberWheel_GetPickedHand(), iosSaberWheel_GetPickedBlade()); bOk = 0; }
        }
        CHECK(bOk && g_platTicks == 7 && inputWheel_IsOpen());
        // one NORMAL saber: the RIGHT card only, again and again
        inputWheel_SaberChip(INPUTWHEEL_CHIP_REMOVE);
        fg.aHilt[0] = 0;
        inputWheel_SaberPickNext();
        const int a = iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1;
        inputWheel_SaberPickNext();
        CHECK(a && iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1);
    }
    g_case = "pad / keys: add and take away the second saber, the picked card's HILT, SAME COLOUR, SAME HILT, AUTO: each with a tick, the wheel open, saved";
    {
        g_platTicks = 0;
        inputWheel_SaberChip(INPUTWHEEL_CHIP_ADD);
        const int bTwo = fg.saberStyle == 1 && iosSaberWheel_GetPickedHand() == 1;
        inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT + iosSaberWheel_GetPickedHand());
        const int bLeftHilt = fg.aHilt[1] == 0 && fg.aHilt[0] == 0; // (FOLD to NORMAL: round)
        inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_HILT);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT + iosSaberWheel_GetPickedHand());
        const int bBoth = fg.aHilt[0] == 1 && fg.aHilt[1] == 1;
        touchUi_Draw(T);
        const int bNoAuto = !touchUi_GetSaberChip(TOUCHUI_CHIP_AUTO, NULL, NULL);
        while (fg.aHilt[1] != 3) inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT + 1);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_AUTO);
        printf("  pad: two %d left hilt %d both %d no AUTO %d same %d/%d auto %d ticks %d\n", bTwo, bLeftHilt, bBoth, bNoAuto, iosSaberWheel_GetSameColour(),
               iosSaberWheel_GetSameHilt(), fg.bFoldAuto, g_platTicks);
        CHECK(bTwo && bLeftHilt && bBoth && bNoAuto && iosSaberWheel_GetSameColour() && iosSaberWheel_GetSameHilt() && !fg.bFoldAuto && inputWheel_IsOpen()
              && g_platTicks >= 7);
        tick(); // (a frame: saved)
        CHECK(settingsHave("saberStyle=DUAL\n") && settingsHave("saberSameColour=1\n") && settingsHave("saberSameHilt=1\n") && settingsHave("saberFoldAuto=0\n")
              && settingsHave("saberHiltRight=FOLD\n") && settingsHave("saberHiltLeft=FOLD\n"));
        inputWheel_SaberChip(INPUTWHEEL_CHIP_REMOVE);
        CHECK(fg.saberStyle == 0 && inputWheel_IsOpen());
        // (not shown with one saber: nothing; SAME COLOUR and SAME HILT count as off, kept on)
        CHECK(!inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR) && !iosSaberWheel_GetSameColour() && !iosSaberWheel_GetSameHilt() && keptIs("SameColour", "1")
              && keptIs("SameHilt", "1"));
    }
    g_case = "pad / keys: remembered over a restart, the overlay hidden as they were picked";
    {
        touchUi_SetShown(0);
        tick();
        fg.saberStyle = 1; fg.aHilt[0] = fg.aHilt[1] = 0; fg.bFoldAuto = 1;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.saberStyle == 0 && fg.aHilt[0] == 3 && fg.aHilt[1] == 3 && !fg.bFoldAuto && keptIs("SameColour", "1") && keptIs("SameHilt", "1"));
    }
    g_case = "the SABER button's picture: each saber in its hilt's shape, each blade in its colour; two in an X";
    {
        static const struct { int two, h0, h1, open; int nMin, nMax; const char* what; } aPic[] = {
            { 0, 0, 0, 0, 1, 1, "one NORMAL" }, { 0, 1, 0, 0, 3, 3, "one CROSS" }, { 0, 2, 0, 0, 2, 2, "one STAFF" },
            { 0, 3, 0, 0, 2, 2, "one FOLD folded" }, { 0, 3, 0, 1, 2, 2, "one FOLD open" }, { 1, 0, 0, 0, 2, 2, "two NORMAL" },
            { 1, 2, 3, 0, 4, 4, "STAFF + FOLD" },
        };
        int bOk = 1;
        for (int k = 0; k < (int)(sizeof(aPic) / sizeof(aPic[0])); k++) {
            setup();
            fg.bSaberBtn = 1; fg.bStaffOk = fg.bFoldOk = 1; fg.saberStyle = aPic[k].two; fg.aHilt[0] = aPic[k].h0; fg.aHilt[1] = aPic[k].h1;
            fg.bFoldLit = aPic[k].open; fg.bFoldOpen = fg.bFoldOpenL = aPic[k].open;
            fg.aBladeColour[0][0] = 1; fg.aBladeColour[0][1] = 11; fg.aUnlinked[0] = 1; fg.aBladeColour[1][0] = 7;
            tick();
            float bx, by, br;
            touchUi_GetButton(TOUCHUI_BTN_SABER, &bx, &by, &br);
            // (the blades: solid lines in a blade's colour, wider than the core, inside the button)
            const touchUiDrawList* l = touchUi_Draw(T);
            int nBlades = 0, nHinge = 0;
            for (int i = 0; i < l->numItems; i++) {
                const touchUiDrawItem* it = &l->aItems[i];
                if (it->type == TOUCHUI_DRAW_LINE && it->stroke.a > 0.6f && fabsf(it->stroke.g - 100.0f / 255.0f) < 0.01f && hypotf(it->x0 - bx, it->y0 - by) < br
                    && hypotf(it->x1 - bx, it->y1 - by) < br + 1.0f)
                    nBlades++;
                if (it->type == TOUCHUI_DRAW_CIRCLE && fabsf(it->x0 - bx) < 0.01f && fabsf(it->y0 - by) < 0.01f && it->r < 2.0f) nHinge++;
            }
            if (nBlades < aPic[k].nMin || nBlades > aPic[k].nMax || nHinge != (aPic[k].h0 == 3 && aPic[k].open)) {
                printf("  SABER picture, %s: %d blade lines, %d hinge\n", aPic[k].what, nBlades, nHinge);
                bOk = 0;
            }
        }
        CHECK(bOk);
    }
}

// ---- the saber wheel's fit, with the label font's own widths
// (touchUiFont.h: the font the GL renderer draws), at every size the touch
// overlay (Android) and the pad's and keys' wheel (desktops: 480 pt tall in a
// window that tall or more) are laid out at
static float textW(const char* t, float size, float xscale) { float w; touchUiDraw_MeasureText(t, size, xscale, &w, NULL); return w; }
// Every text the wheel draws in a card, a switch or a button beside the ring
// inside it, 2 pt in (its box at its size and width: the font's own)
// (each text drawn in a choice's box inside it, at least nMin of them)
static int textsInside(float W, float H, float l, float t, float r, float b, int nMin, const char* what)
{
    static touchUiDrawList list;
    touchUiDraw_Begin(&list);
    inputWheelScreen sc = { W, H, l, t, r, b };
    inputWheel_Draw(&list, &sc);
    static const int aBox[] = { TOUCHUI_CHIP_HILT, TOUCHUI_CHIP_HILT + 1, TOUCHUI_CHIP_CARD, TOUCHUI_CHIP_CARD + 1, TOUCHUI_CHIP_ADD, TOUCHUI_CHIP_SAME_COLOUR,
                                TOUCHUI_CHIP_SAME_HILT, TOUCHUI_CHIP_SHAPE, TOUCHUI_CHIP_AUTO, TOUCHUI_CHIP_EQUIP, TOUCHUI_CHIP_LOOK };
    int bOk = 1, n = 0;
    for (int i = 0; i < list.numItems; i++) {
        const touchUiDrawItem* it = &list.aItems[i];
        if (it->type != TOUCHUI_DRAW_TEXT) continue;
        float w, h, rc[4];
        const char* txt = list.aPool + it->text;
        touchUiDraw_MeasureText(txt, it->size, it->xscale, &w, &h);
        const float capH = it->size * 22.0f / 32.0f; // (its capitals: TOUCHUI_FONT_CAP)
        for (int k = 0; k < (int)(sizeof(aBox) / sizeof(aBox[0])); k++) {
            if (!chipRect(aBox[k], rc) || it->x0 < rc[0] || it->x0 > rc[0] + rc[2] || it->y0 < rc[1] || it->y0 > rc[1] + rc[3]) continue;
            n++;
            const float m = aBox[k] == TOUCHUI_CHIP_LOOK ? 0.0f : 2.0f;
            if (it->x0 - w * 0.5f < rc[0] + m - 0.01f || it->x0 + w * 0.5f > rc[0] + rc[2] - m + 0.01f || it->y0 - capH * 0.5f < rc[1] + m
                || it->y0 + capH * 0.5f > rc[1] + rc[3] - m) { printf("  %s: \"%s\" (%.1f pt) out of choice %d\n", what, txt, it->size, aBox[k]); bOk = 0; }
            break;
        }
    }
    return bOk && n >= nMin;
}

static void test_saber_fit(void)
{
    static const char* const aName[17] = { "ORIGINAL", "RED", "CRIMSON", "ORANGE", "AMBER", "YELLOW", "LIME", "GREEN", "TEAL", "CYAN",
                                           "LIGHT BLUE", "BLUE", "PURPLE", "MAGENTA", "PINK", "WHITE", "BLACK" };
    static const char* const aHilt[4] = { "NORMAL", "CROSS", "STAFF", "FOLD" };
    const float CW = 104.0f;
    g_case = "fit: the cards' text at its sizes, in its room (the widest names)";
    {
        // (as inputWheel.c draws them: a split blade's name 8.5 pt condensed, down to
        // 7.5 and then narrower; SAME COLOUR a little condensed at 10; HILT 3 pt gaps)
        float wColour = 0.0f, wBlade = 0.0f, bladeMin = 8.5f, bladeX = 0.82f, wHilt = 0.0f;
        for (int i = 0; i < 17; i++) {
            wColour = fmaxf(wColour, textW(aName[i], 10.0f, 1.0f));
            float size = 8.5f, xs = 0.82f, w = textW(aName[i], size, xs);
            if (w > 34.0f) { size = fmaxf(7.5f, 8.5f * 34.0f / w); w = textW(aName[i], size, xs); if (w > 34.0f) xs = fmaxf(0.7f, xs * 34.0f / w); }
            if (size < 8.5f) printf("  fit: %s split at %.2f pt, x %.3f: %.1f wide\n", aName[i], size, xs, textW(aName[i], size, xs));
            wBlade = fmaxf(wBlade, textW(aName[i], size, xs));
            bladeMin = fminf(bladeMin, size);
            bladeX = fminf(bladeX, xs);
        }
        for (int h = 0; h < 4; h++) wHilt = fmaxf(wHilt, textW("HILT", 8.0f, 1.0f) + 3.0f + textW(aHilt[h], 11.0f, 1.0f) + 3.0f + 4.0f);
        const float wSameC = textW("SAME COLOUR", 10.0f, fmaxf(0.82f, 71.0f / textW("SAME COLOUR", 10.0f, 1.0f))), wSameH = textW("SAME HILT", 10.0f, 1.0f);
        const float wPlus = textW("2ND SABER", 11.0f, 1.0f);
        printf("  fit: colour %.1f (room %.1f, joined %.1f), blades %.1f (room 34, smallest %.2f pt, x %.3f), HILT %.1f (room 76), SAME COLOUR %.1f (x %.3f) / SAME HILT %.1f (room 71), 2ND SABER %.1f, RIGHT %.1f, LEFT %.1f\n",
               wColour, CW - 8.0f - 21.0f, CW - 8.0f - 31.5f, wBlade, bladeMin, bladeX, wHilt, wSameC, 71.0f / textW("SAME COLOUR", 10.0f, 1.0f), wSameH, wPlus,
               textW("RIGHT", 11.0f, 1.0f), textW("LEFT", 11.0f, 1.0f));
        CHECK(21.0f + wColour <= CW - 8.0f && 10.0f + 21.5f + wColour <= CW - 8.0f && wHilt <= 88.0f - 12.0f + 0.2f && wSameC <= 71.0f + 0.01f && wSameH <= 71.0f
              && wPlus <= CW - 8.0f && textW("RIGHT", 11.0f, 1.0f) <= CW - 20.0f && textW("LEFT", 11.0f, 1.0f) <= CW - 20.0f - 40.0f);
        CHECK(bladeMin >= 7.5f && bladeX >= 0.7f && wBlade <= 34.0f + 0.01f);
        // (AUTO OPEN and SABER THROW on two lines, 4 pt further in: a little condensed, at 10 pt; SABER THROW smaller, 8 pt at the least)
        CHECK(textW("FILM", 10.0f, 1.0f) <= 64.0f - 33.0f && textW("AUTO\nOPEN", 10.0f, 0.82f) <= 64.0f - 37.0f && textW("SABER\nTHROW", 8.0f, 0.82f) <= 64.0f - 37.0f);
    }
    // (bTouch: 1 the touch overlay's, else as a pad and the keys open it; at: the cards
    // beside the ring (0), under it (1: a narrow, square or standing window), or left
    // out (2: too small for them and a ring of 126 pt))
    static const struct { const char* name; float w, h, l, t, r, b; int bTouch, at; } aScr[] = {
        { "Android 568x320", 568, 320, 0, 0, 0, 0, 1 }, { "Android 640x360", 640, 360, 0, 0, 0, 0, 1 }, { "Android 640x300", 640, 300, 0, 0, 0, 0, 1 },
        { "Android 667x375", 667, 375, 0, 0, 0, 0, 1 }, { "Android 693x320 (insets)", 693, 320, 44, 0, 44, 21, 1 }, { "Android 732x360", 732, 360, 0, 0, 0, 0, 1 },
        { "Android 780x360", 780, 360, 0, 0, 0, 0, 1 }, { "Android 800x360", 800, 360, 0, 0, 0, 0, 1 }, { "Android 852x393 (insets)", 852, 393, 59, 0, 59, 21, 1 },
        { "Android 873x393 (cutout left)", 873, 393, 32, 0, 0, 0, 1 }, { "Android 873x393 (cutout right)", 873, 393, 0, 0, 32, 0, 1 },
        { "Android 915x412", 915, 412, 0, 0, 0, 0, 1 }, { "Android tablet 853x640", 853, 640, 0, 0, 0, 0, 1 }, { "Android tablet 1067x667 (nav bar)", 1067, 667, 0, 0, 0, 40, 1 },
        { "desktop 640x480 (4:3)", 640, 480, 0, 0, 0, 0, 0 }, { "desktop 720x480 (3:2)", 720, 480, 0, 0, 0, 0, 0 }, { "desktop 768x480 (16:10)", 768, 480, 0, 0, 0, 0, 0 },
        { "desktop 853x480 (16:9)", 853.33f, 480, 0, 0, 0, 0, 0 }, { "desktop 1120x480 (21:9)", 1120, 480, 0, 0, 0, 0, 0 },
        { "desktop window 640x400", 640, 400, 0, 0, 0, 0, 0 }, { "desktop window 800x450", 800, 450, 0, 0, 0, 0, 0 },
        { "desktop window 600x400 (small)", 600, 400, 0, 0, 0, 0, 0 },
        { "desktop window 427x480 (half a 1080p screen)", 427, 480, 0, 0, 0, 0, 0, 1 }, { "desktop window 480x480 (square)", 480, 480, 0, 0, 0, 0, 0, 1 },
        { "desktop window 320x480 (standing up)", 320, 480, 0, 0, 0, 0, 0, 1 }, { "desktop window 400x300 (tiny)", 400, 300, 0, 0, 0, 0, 0, 2 },
        { "desktop window 320x240 (tiny)", 320, 240, 0, 0, 0, 0, 0, 2 }, { "Android split screen 400x700", 400, 700, 0, 24, 0, 48, 1, 1 },
        { "Android split screen 427x480", 427, 480, 0, 0, 0, 0, 1, 1 }, { "Android split screen 480x480", 480, 480, 0, 0, 0, 0, 1, 1 },
        { "Android split screen 320x480", 320, 480, 0, 0, 0, 0, 1, 1 }, { "Android split screen 426x393 (landscape halves)", 426, 393, 0, 0, 0, 0, 1, 2 },
        { "Android window 400x300", 400, 300, 0, 0, 0, 0, 1, 2 },
    };
    for (int k = 0; k < (int)(sizeof(aScr) / sizeof(aScr[0])); k++) {
        int bOk = 1;
        float hubMin = 1.0f;
        for (int game = 0; game < 2; game++)
            for (int st = 0; st < 6; st++) {
                setup();
                fg.bRealNames = 1;
                fg.bMots = game;
                fg.bSaberBtn = 1;
                saberState(st);
                tick();
                if (aScr[k].bTouch == 1) {
                    touchUi_SetScreen(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b);
                    tick();
                    tapSaber();
                }
                else {
                    // (as the pad and the keys open it: no buttons round it, no top row)
                    touchUi_SetShown(0);
                    tick();
                    inputWheel_Open(INPUTWHEEL_SABER, 1);
                    inputWheelScreen sc = { aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b };
                    inputWheel_Layout(&sc);
                }
                char what[200];
                snprintf(what, sizeof(what), "%s, %s, %s", aScr[k].name, game ? "MotS" : "JK", g_aStateName[st]);
                if (!inputWheel_IsOpen()) bOk = 0;
                else if (aScr[k].at == 2) {
                    // (no room for the cards: the ring alone, inside the screen; nothing else round it but THROW)
                    float cx0, cy0, r00, r10, rc0[4];
                    inputWheel_GetGeometry(&cx0, &cy0, &r00, &r10);
                    int bNone = 1;
                    for (int i = TOUCHUI_CHIP_CARD; i <= TOUCHUI_CHIP_AUTO; i++) if (i != TOUCHUI_CHIP_EQUIP && i != TOUCHUI_CHIP_LOOK && chipRect(i, rc0)) bNone = 0;
                    const float L = fmaxf(aScr[k].l, 8.0f), R = aScr[k].w - fmaxf(aScr[k].r, 8.0f), Tp = fmaxf(aScr[k].t, 8.0f), B = aScr[k].h - fmaxf(aScr[k].b, 8.0f);
                    if (!bNone || cx0 - r10 - 10.0f < L - 0.01f || cx0 + r10 + 10.0f > R + 0.01f || cy0 - r10 - 10.0f < Tp - 0.01f || cy0 + r10 + 10.0f > B + 0.01f) {
                        printf("  %s: cards %s, ring %.0f,%.0f r1 %.0f\n", what, bNone ? "out" : "SHOWN", cx0, cy0, r10);
                        bOk = 0;
                    }
                }
                else {
                    if (!layoutClear(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b, aScr[k].bTouch == 1, what)) bOk = 0;
                    // (beside the ring, or under it (the touch overlay's) or over it (a pad's, the keys': their prompt along the bottom), as the window wants)
                    float cx0, cy0, r00, r10, rc0[4];
                    inputWheel_GetGeometry(&cx0, &cy0, &r00, &r10);
                    chipRect(TOUCHUI_CHIP_CARD, rc0);
                    const int bUnder = rc0[1] > cy0 + r10, bOver = rc0[1] + rc0[3] < cy0 - r10;
                    if ((aScr[k].at == 1 ? !(aScr[k].bTouch == 1 ? bUnder : bOver) : (bUnder || bOver)) || r10 < 126.0f) {
                        printf("  %s: the cards %s the ring (r1 %.0f)\n", what, bUnder ? "under" : bOver ? "over" : "beside", r10);
                        bOk = 0;
                    }
                }
                // (the longest names: the right hand's LIGHT BLUE and MAGENTA, the left's ORIGINAL)
                fg.aBladeColour[0][0] = 10; fg.aBladeColour[0][1] = 13; fg.aBladeColour[1][0] = 0;
                if (!textsInside(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b, aScr[k].at == 2 ? 2 : 6, what)) bOk = 0;
                // (THROW in Jedi Knight wherever there is room for it; never in MotS)
                float rt[4];
                if (game && chipRect(TOUCHUI_CHIP_THROW, rt)) bOk = 0;
                if (!game && !chipRect(TOUCHUI_CHIP_THROW, rt) && aScr[k].h > 300 && aScr[k].at == 0) { printf("  %s: no THROW\n", what); bOk = 0; }
                // (the middle's line, as small as it goes)
                float cx, cy, r0, r1;
                inputWheel_GetGeometry(&cx, &cy, &r0, &r1);
                static const char* const aHub[] = { "colour for the right saber", "colour for the left saber", "colour for both sabers", "right saber, blade 2",
                                                    "slide toward a colour" };
                for (int h = 0; h < 5; h++) hubMin = fminf(hubMin, 2.0f * r0 / textW(aHub[h], 10.0f, 1.0f));
                if (inputWheel_IsOpen()) inputWheel_Close(-1);
            }
        static char name[200];
        snprintf(name, sizeof(name), "fit: %s: every state, both games: on screen, clear of the ring, THROW and each other; the middle's line at %.1f pt or more", aScr[k].name, 10.0f * fminf(1.0f, hubMin));
        g_case = name;
        printf("  %s: the middle's longest line at %.2f of 10 pt\n", aScr[k].name, fminf(1.0f, hubMin));
        // (the middle's line on the squat 640x300: as small as it goes, a point wider than the hole, as it was before the cards)
        CHECK(bOk && (hubMin >= 0.8f || aScr[k].h <= 300));
    }
}

// Whether the overlay draws a text
// The first drawn text that says t: where (its middle) and how big
static int drawnTextAt(const char* t, float* pX, float* pY, float* pSize)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++)
        if (l->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(l->aPool + l->aItems[i].text, t)) {
            *pX = l->aItems[i].x0; *pY = l->aItems[i].y0; *pSize = l->aItems[i].size;
            return 1;
        }
    return 0;
}

static int drawnText(const char* t)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++)
        if (l->aItems[i].type == TOUCHUI_DRAW_TEXT && !strcmp(l->aPool + l->aItems[i].text, t)) return 1;
    return 0;
}

// Whether the switch whose chip's middle is (x, y) is drawn on (its knob at
// the track's right), off (at its left), or neither (-1): inputWheel.c's
static int knobOn(float x, float y)
{
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++) {
        const touchUiDrawItem* it = &l->aItems[i];
        if (it->type != TOUCHUI_DRAW_CIRCLE || fabsf(it->r - 4.0f) > 0.01f || fabsf(it->y0 - y) > 0.01f) continue;
        if (fabsf(it->x0 - (x + 19.0f)) < 0.01f) return 1;
        if (fabsf(it->x0 - (x + 11.0f)) < 0.01f) return 0;
    }
    return -1;
}

// SABER THROW while sithewok holds Saber Throw on the Force wheel: the switch
// shows on, a little dim, whatever THROW says; a tap (and the pad's or keys'
// call) only ticks, THROW kept as the player set it; the cheat off again,
// the switch shows THROW as set
static void test_saber_throw_cheat(void)
{
    float sx, sy, tx = 0.0f, ty = 0.0f, cx, cy, r0, r1, px, py;
    for (int bOwn = 0; bOwn < 2; bOwn++) {
        setup();
        fg.bSaberBtn = 1;
        fg.bThrow = bOwn;
        fg.bCheat = 1;
        tick();
        bxy(TOUCHUI_BTN_SABER, &sx, &sy);
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
        polar(cx, cy, 270.0f, (r0 + r1) * 0.5f, &px, &py);
        g_case = bOwn ? "throw, the cheat on, THROW on: SABER THROW shows on, a little dim"
                      : "throw, the cheat on, THROW off: SABER THROW shows on all the same, a little dim";
        CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, &tx, &ty));
        CHECK(knobOn(tx, ty) == 1 && fabsf(drawnTextAlpha("SABER\nTHROW") - 0.7f) < 0.01f);
        g_case = "throw, the cheat on: a tap only ticks, the wheel open, THROW as the player set it (kept); the pad's and keys' call the same";
        g_platTicks = 0;
        down(2, tx, ty); up(2, tx, ty);
        const int ticks = g_platTicks;
        inputWheel_SaberChip(INPUTWHEEL_CHIP_THROW);
        tick();
        CHECK(fg.bThrow == bOwn && ticks == 1 && touchUi_IsWheelOpen() && knobOn(tx, ty) == 1 && settingsHave(bOwn ? "saberThrow=1" : "saberThrow=0"));
        down(2, px, py); up(2, px, py);
        CHECK(!touchUi_IsWheelOpen());
        g_case = "throw, the cheat off again: SABER THROW shows THROW as the player set it, not dim";
        fg.bCheat = 0;
        tick();
        down(1, sx, sy); wait(0.1); up(1, sx, sy);
        CHECK(touchUi_IsWheelOpen() && touchUi_GetSaberChip(TOUCHUI_CHIP_THROW, &tx, &ty) && knobOn(tx, ty) == bOwn
              && fabsf(drawnTextAlpha("SABER\nTHROW") - (bOwn ? 1.0f : 0.6f)) < 0.01f);
        g_case = "throw, the cheat off again: a tap switches it as ever";
        down(2, tx, ty); up(2, tx, ty);
        CHECK(fg.bThrow == !bOwn && knobOn(tx, ty) == !bOwn);
        down(2, px, py); up(2, px, py);
    }
}

// A button's label as drawn (the text item at its middle that says t): how
// far its widest line's ends are inside radius inner of the middle (the lines
// 1.19 of the size apart, the bottom one's foot a quarter of it above the
// block's), its size and xscale; and the same at 10 pt, full width
static int labelMargin(int btn, const char* t, float inner, float* pMargin, float* pSize, float* pXs, float* pMargin10)
{
    float bx, by;
    bxy(btn, &bx, &by);
    const touchUiDrawList* l = touchUi_Draw(T);
    for (int i = 0; i < l->numItems; i++) {
        const touchUiDrawItem* it = &l->aItems[i];
        if (it->type != TOUCHUI_DRAW_TEXT || strcmp(l->aPool + it->text, t) || fabsf(it->x0 - bx) > 0.01f || fabsf(it->y0 - by) > 0.01f) continue;
        int lines = 1;
        for (const char* p = t; *p; p++) lines += *p == '\n';
        float w, w10;
        touchUiDraw_MeasureText(t, it->size, it->xscale, &w, NULL);
        touchUiDraw_MeasureText(t, 10.0f, 1.0f, &w10, NULL);
        const float far = lines * 0.595f - 0.2496f;
        *pMargin = inner - sqrtf(w * w * 0.25f + it->size * far * it->size * far);
        *pMargin10 = inner - sqrtf(w10 * w10 * 0.25f + 10.0f * far * 10.0f * far);
        *pSize = it->size;
        *pXs = it->xscale;
        return 1;
    }
    return 0;
}

// FORCE's label with every power's name (iosGame_GetPowerName's: Jedi
// Knight's and Mysteries of the Sith's) and the tray's CONTROLS both ways:
// the widest line's ends 1.5 pt inside the round (FORCE's: inside the force
// meter's ring, 4 pt in; CONTROLS's: inside its 1.5 pt border), never under
// 8 pt; a name that fits at 10 pt stays so
static void test_label_fit(void)
{
    static const char* aNames[] = { "JUMP", "SPEED", "SEEING", "PULL", "HEALING", "PERSUADE", "BLINDING", "ABSORB", "PROTECT", "THROW",
                                    "GRIP", "LIGHTNING", "DESTRUCT", "DEADLY SIGHT", "DEFENSE", "FAR SIGHT", "PROJECT", "SABER THROW", "PUSH", "CHAIN LIGHT" };
    float r, m, size, xs, m10;
    setup();
    touchUi_GetButton(TOUCHUI_BTN_FORCE, NULL, NULL, &r);
    fg.curPower = 21;
    for (int k = 0; k < (int)(sizeof(aNames) / sizeof(aNames[0])); k++) {
        char t[32];
        snprintf(t, sizeof t, "FORCE\n%s", aNames[k]);
        fg.forceName = aNames[k];
        tick();
        g_case = "fit: FORCE's label with every power's name: inside the force meter's ring, 8 pt or more, a name that fits at 10 pt so";
        CHECK(labelMargin(TOUCHUI_BTN_FORCE, t, r - 4.0f, &m, &size, &xs, &m10));
        CHECK(m >= 1.5f - 0.01f && size >= 8.0f - 0.001f && xs >= 0.6f - 0.001f);
        if (m10 >= 1.5f) CHECK(fabsf(size - 10.0f) < 0.001f && fabsf(xs - 1.0f) < 0.001f);
        printf("  FORCE %-12s %.2f pt x%.2f, %.2f pt inside the ring\n", aNames[k], size, xs, m);
    }
    fg.forceName = NULL;
    g_case = "fit: the tray's CONTROLS, NORMAL and SWAPPED: inside its border, 8 pt or more";
    touchUi_GetButton(TOUCHUI_BTN_TRAYCTRL, NULL, NULL, &r);
    for (int bSwap = 0; bSwap < 2; bSwap++) {
        openTrayT();
        CHECK(labelMargin(TOUCHUI_BTN_TRAYCTRL, bSwap ? "CONTROLS\nSWAPPED" : "CONTROLS\nNORMAL", r - 1.5f, &m, &size, &xs, &m10));
        CHECK(m >= 1.5f - 0.01f && size >= 8.0f - 0.001f && xs >= 0.6f - 0.001f);
        printf("  CONTROLS %s: %.2f pt x%.2f, %.2f pt inside the border\n", bSwap ? "SWAPPED" : "NORMAL", size, xs, m);
        downOn(2, TOUCHUI_BTN_TRAYCTRL); upOn(2, TOUCHUI_BTN_TRAYCTRL); // (swaps; the tray closes)
        closeTrayT();
    }
}

// The LEFT card's x against its colours' row, at every touch size: the x's
// touch the card's top right, 40 x 22 (the x drawn 4..24 pt down); from 22 pt
// down the row's (the colour's name, a dot); a tap there never drops the
// second saber. And the pad's and keys' calls: what each one made does.
static void test_saber_x_row(void)
{
    static const struct { const char* name; float w, h, l, t, r, b; } aScr[] = {
        { "568x320", 568, 320, 0, 0, 0, 0 }, { "640x360", 640, 360, 0, 0, 0, 0 }, { "640x300", 640, 300, 0, 0, 0, 0 }, { "667x375", 667, 375, 0, 0, 0, 0 },
        { "693x320 (insets)", 693, 320, 44, 0, 44, 21 }, { "732x360", 732, 360, 0, 0, 0, 0 }, { "780x360", 780, 360, 0, 0, 0, 0 },
        { "852x393 (insets)", 852, 393, 59, 0, 59, 21 }, { "873x393 (cutout left)", 873, 393, 32, 0, 0, 0 }, { "915x412", 915, 412, 0, 0, 0, 0 },
        { "tablet 853x640", 853, 640, 0, 0, 0, 0 }, { "tablet 1067x667 (nav bar)", 1067, 667, 0, 0, 0, 40 }, { "split 400x700", 400, 700, 0, 24, 0, 48 },
        { "split 427x480", 427, 480, 0, 0, 0, 0 }, { "split 480x480", 480, 480, 0, 0, 0, 0 }, { "split 320x480", 320, 480, 0, 0, 0, 0 },
    };
    // (card-local points: under the x's touch, the row's; in it, the x's)
    static const float aRow[][2] = { { 64.0f, 22.5f }, { 84.0f, 22.5f }, { 99.0f, 23.0f }, { 103.5f, 26.0f }, { 70.0f, 27.0f }, { 90.0f, 31.5f }, { 40.0f, 26.5f } };
    static const float aX[][2] = { { 88.0f, 14.0f }, { 64.5f, 0.5f }, { 103.5f, 21.5f }, { 66.0f, 20.0f } };
    for (int k = 0; k < (int)(sizeof(aScr) / sizeof(aScr[0])); k++) {
        int bOk = 1;
        for (int st = 0; st < 3; st++) {
            // (the LEFT card NORMAL; FOLD linked; FOLD split)
            for (int p = 0; p < (int)(sizeof(aRow) / sizeof(aRow[0])) + (int)(sizeof(aX) / sizeof(aX[0])); p++) {
                const int bRowPt = p < (int)(sizeof(aRow) / sizeof(aRow[0]));
                const float* pt = bRowPt ? aRow[p] : aX[p - (int)(sizeof(aRow) / sizeof(aRow[0]))];
                setup();
                fg.bSaberBtn = 1; fg.bStaffOk = fg.bFoldOk = 1; fg.saberStyle = 1;
                fg.aHilt[1] = st ? 3 : 0; fg.aUnlinked[1] = st == 2; fg.aBladeColour[1][0] = 10; fg.aBladeColour[1][1] = 13;
                touchUi_SetScreen(aScr[k].w, aScr[k].h, aScr[k].l, aScr[k].t, aScr[k].r, aScr[k].b);
                tick();
                tapSaber();
                float rc[4];
                if (!touchUi_IsWheelOpen() || !chipRect(CARD(1), rc)) { printf("  x/row %s: no LEFT card\n", aScr[k].name); bOk = 0; continue; }
                down(2, rc[0] + pt[0], rc[1] + pt[1]); up(2, rc[0] + pt[0], rc[1] + pt[1]);
                const int bDropped = fg.saberStyle == 0;
                // (the row: the card's (a linked FOLD's name, 31 pt on), or a split FOLD's dot: each half its own)
                const int wantBlade = st == 2 ? (pt[0] >= 52.0f) : -1;
                if (bRowPt && (bDropped || iosSaberWheel_GetPickedHand() != 1 || iosSaberWheel_GetPickedBlade() != wantBlade || fg.aUnlinked[1] != (st == 2))) {
                    printf("  x/row %s, state %d: a tap at %.1f,%.1f: %s, picked %d,%d\n", aScr[k].name, st, pt[0], pt[1], bDropped ? "DROPPED" : "kept",
                           iosSaberWheel_GetPickedHand(), iosSaberWheel_GetPickedBlade());
                    bOk = 0;
                }
                if (!bRowPt && !bDropped) { printf("  x/row %s, state %d: a tap on the x at %.1f,%.1f kept two\n", aScr[k].name, st, pt[0], pt[1]); bOk = 0; }
                if (inputWheel_IsOpen()) inputWheel_Close(-1);
            }
        }
        static char name[160];
        snprintf(name, sizeof(name), "x and the colours' row, %s: a tap 22 pt down or more on the LEFT card is the row's (NORMAL, FOLD linked, split); the x's own still drops it", aScr[k].name);
        g_case = name;
        CHECK(bOk);
    }

    g_case = "pad / keys: SAME COLOUR on: the next card only (no blades: a dot is its card's); off: its blades again";
    setup();
    fg.bSaberBtn = 1; fg.bStaffOk = fg.bFoldOk = 1; fg.saberStyle = 1; fg.aHilt[0] = 2; fg.aHilt[1] = 3;
    tick();
    tapSaber();
    {
        inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR);
        int seq = 0;
        for (int k = 0; k < 4; k++) {
            inputWheel_SaberPickNext();
            seq = seq * 10 + iosSaberWheel_GetPickedHand() * 3 + iosSaberWheel_GetPickedBlade() + 1;
        }
        CHECK(iosSaberWheel_GetSameColour() && seq == 3030);
        inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR);
        inputWheel_SaberPickNext();
        CHECK(iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == 0);
    }
    g_case = "pad / keys: stepping onto blade 2 of a linked pair only picks it (still linked); a colour other than blade 1's splits it";
    {
        inputWheel_SaberPickNext();
        const int bPicked = iosSaberWheel_GetPickedBlade() == 1 && !fg.aUnlinked[0];
        inputWheel_Point(5, 1, 0.0f);
        inputWheel_Close(5);
        CHECK(bPicked && inputWheel_IsOpen() && fg.aUnlinked[0] && fg.aBladeColour[0][1] == 5);
    }
    g_case = "pad / keys: a choice that can't be made now does nothing (0) and keeps the colour pointed at; one made lets go of it (1)";
    {
        inputWheel_Point(4, 1, 0.0f);
        const int bAuto = inputWheel_SaberChip(INPUTWHEEL_CHIP_AUTO); // (a FOLD in the left hand: AUTO is there)
        const int hotAfterAuto = inputWheel_GetHot();
        fg.aHilt[1] = 0; fg.aHilt[0] = 0;
        inputWheel_Point(4, 1, 0.0f);
        g_platTicks = 0;
        const int bNoAuto = inputWheel_SaberChip(INPUTWHEEL_CHIP_AUTO), bNoHilt = (fg.bStaffOk = fg.bFoldOk = 0, fg.bCrossOk = 0, inputWheel_SaberChip(INPUTWHEEL_CHIP_HILT));
        const int hotKept = inputWheel_GetHot();
        inputWheel_SaberChip(INPUTWHEEL_CHIP_REMOVE);
        const int bNoSame = inputWheel_SaberChip(INPUTWHEEL_CHIP_SAME_COLOUR);
        CHECK(bAuto == 1 && hotAfterAuto == -1 && bNoAuto == 0 && bNoHilt == 0 && hotKept == 4 && bNoSame == 0 && inputWheel_GetHot() == -1 && g_platTicks == 1);
        if (inputWheel_IsOpen()) inputWheel_Close(-1);
    }
}

// The pad's and the keys' prompt along the bottom (inputOverlay.c's, iosPadPanel.m's):
// at 13 pt, on as few lines as fit at 0.8 of it or more (one where it does; two from
// 568 pt wide, as iOS's, but the PlayStation's in words at 568, three; four at the
// most, then to 0.6), each as short as it can be;
// every part whole, "close"
// last. The fullest prompts (two sabers, a FOLD, Jedi Knight's SABER THROW) as iosPad.c
// and iosKbMouse.c make them (the PlayStation's buttons in words, as drawn here).
static void test_prompt_lines(void)
{
    static const char* const aPrompt[] = {
        "Right stick: colour   A: set   Y: pick   LB: hilt   X: one saber   RB: same colour   LT: same hilt   RT: auto open   D-pad up: film   D-pad down: saber throw   L3: look   R3: equip   B: close",
        "Right stick: colour   Cross: set   Triangle: pick   L1: hilt   Square: one saber   R1: same colour   L2: same hilt   R2: auto open   D-pad up: film   D-pad down: saber throw   L3: look   R3: equip   Circle: close",
        "Mouse: colour   Click: set   1: one saber   2: pick   7: hilt   8: same colour   9: same hilt   0: auto open   3: equip   4: look   5: film   6: saber throw   Esc: close",
        "Right stick: colour   A: set   LB: hilt   X: 2nd saber   D-pad up: film   D-pad down: saber throw   L3: look   R3: equip   B: close",
    };
    static const float aW[] = { 568, 640, 667, 693, 720, 732, 780, 812, 852, 874, 912, 932, 956, 1024, 1366, 427, 480, 400, 320 };
    int bOk = 1, nTwo = 0, nMore = 0;
    float worst = 1.0f;
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < (int)(sizeof(aW) / sizeof(aW[0])); k++) {
            char t[512];
            snprintf(t, sizeof(t), "%s", aPrompt[i]);
            const float maxW = aW[k] - 16.0f - 28.0f;
            float w, w1;
            const int lines = touchUiDraw_BreakLines(t, 13.0f, 0.8f, maxW, 4, &w);
            // (the fewest: one line fewer doesn't fit at 0.8)
            char t2[512];
            float wl = 0.0f;
            snprintf(t2, sizeof(t2), "%s", aPrompt[i]);
            if (lines > 1) touchUiDraw_BreakLines(t2, 13.0f, 0.8f, maxW, lines - 1, &wl);
            const int bFewest = lines == 1 || wl * 0.8f > maxW;
            touchUiDraw_MeasureText(aPrompt[i], 13.0f, 1.0f, &w1, NULL);
            const float scale = w > maxW ? fmaxf(0.6f, maxW / w) : 1.0f;
            // (each part whole: the text the same but for the breaks where three spaces were)
            char back[512];
            int o = 0, nl = 0;
            for (const char* q = t; *q && o < 500; q++) { if (*q == '\n') { memcpy(back + o, "   ", 3); o += 3; nl++; } else back[o++] = *q; }
            back[o] = 0;
            float wm = 0.0f;
            touchUiDraw_MeasureText(t, 13.0f, 1.0f, &wm, NULL);
            // (two lines: the best break of all, the longer line as short as any)
            float best2 = w1;
            for (const char* q = strstr(aPrompt[i], "   "); q; q = strstr(q + 3, "   ")) {
                char a[512]; float wa, wb;
                snprintf(a, sizeof(a), "%.*s", (int)(q - aPrompt[i]), aPrompt[i]);
                touchUiDraw_MeasureText(a, 13.0f, 1.0f, &wa, NULL); touchUiDraw_MeasureText(q + 3, 13.0f, 1.0f, &wb, NULL);
                best2 = fminf(best2, fmaxf(wa, wb));
            }
            const int bRight = !strcmp(back, aPrompt[i]) && nl == lines - 1 && ((lines == 1) == (w1 * 0.8f <= maxW)) && fabsf(wm - w) < 0.01f
                               && (lines != 2 || fabsf(w - best2) < 0.01f) && bFewest && (w * 0.8f <= maxW + 0.01f || lines == 4) && w * 0.6f <= maxW + 0.01f
                               && w * scale <= maxW + 0.01f
                               && !strncmp(t + strlen(t) - 5, "close", 5);
            if (!bRight) { printf("  prompt %d at %.0f: %d lines [%s]\n", i, aW[k], lines, t); bOk = 0; }
            // (but the PlayStation's buttons in words at 568, the longest of all since AUTO OPEN and
            // SABER THROW: three lines, a hair too long for two at 0.8)
            const int bLong = i == 1 && aW[k] == 568.0f;
            if (aW[k] >= 568.0f) { worst = fminf(worst, scale); if (lines > (bLong ? 3 : 2)) { printf("  prompt %d at %.0f: %d lines\n", i, aW[k], lines); bOk = 0; } }
            nTwo += lines == 2;
            nMore += lines > 2;
            if (aW[k] == 568.0f || aW[k] == 640.0f || aW[k] == 693.0f || aW[k] == 720.0f || aW[k] == 732.0f || aW[k] == 320.0f)
                printf("  prompt %d at %.0f: %d line%s at %.2f of 13 pt%s%s\n", i, aW[k], lines, lines > 1 ? "s" : "", scale, lines > 1 ? ", the last: " : "", lines > 1 ? (strrchr(t, '\n') + 1) : "");
        }
    g_case = "prompt: the fullest prompts (Xbox, PlayStation in words, the keys) from 320 to 1366 pt wide: as few lines as fit at 0.8 or more, the best break; every part whole, close last; never wider than the room";
    CHECK(bOk && nTwo > 0 && nMore > 0);
    g_case = "prompt: from 568 pt wide (568, 640, 693, 720, 732 and every phone): one line or two (the PlayStation's in words at 568: three), at 0.8 of 13 pt or more";
    CHECK(worst >= 0.8f);
}

// A photo mode the keyboard or a pad started (inputPhoto.c): the fingers
// leave it alone; and the touch wheel's "lift to cancel"
static void test_other_photo_and_cancel(void)
{
    float wx, wy, fx, fy, cx, cy, r0, r1;
    setup();
    bxy(TOUCHUI_BTN_WHEEL, &wx, &wy);
    bxy(TOUCHUI_BTN_FIRE, &fx, &fy);

    g_case = "a photo mode the keys started: a finger already down lets go of its button";
    down(1, fx, fy);
    tick();
    CHECK(touchUi_IsKeyHeld(SC_LCTRL));
    fg.bFreeze = 1; // (Action Freeze, from O)
    tick();
    CHECK(!touchUi_IsKeyHeld(SC_LCTRL));
    up(1, fx, fy);
    g_case = "a photo mode the keys started: no wheel, no next power, no stick, nothing drawn";
    down(2, wx, wy);
    move(2, wx + 20.0f, wy);
    wait(0.4);
    up(2, wx + 20.0f, wy);
    CHECK(!touchUi_IsWheelOpen() && !inputWheel_IsOpen());
    CHECK(presses(SC_E, 6) == 0);
    down(3, 100, 300);
    move(3, 130, 300);
    CHECK(!touchUi_IsStickActive());
    up(3, 130, 300);
    CHECK(touchUi_Draw(T)->numItems == 0);
    g_case = "that photo mode over: the fingers work again";
    fg.bFreeze = 0;
    tick();
    down(4, wx, wy);
    move(4, wx + 10.0f, wy);
    CHECK(touchUi_IsWheelOpen());

    g_case = "the touch wheel: a finger in the hole says CANCEL / lift to cancel, no tick for it";
    up(4, wx + 10.0f, wy); // (not armed: open to tap)
    CHECK(touchUi_IsWheelOpen() && touchUi_IsWheelTapMode());
    touchUi_GetWheelGeometry(&cx, &cy, &r0, &r1);
    takeHaptics();
    down(5, cx, cy); // (the hole)
    CHECK(touchUi_GetWheelHot() == -1);
    CHECK(drawnText("CANCEL") && drawnText("lift to cancel"));
    CHECK(!(takeHaptics() & TOUCHUI_HAPTIC_TICK));
    up(5, cx, cy);
    CHECK(!touchUi_IsWheelOpen() && fg.bHold == 0);
}

// ------------------------------------------------------------ the saber wheel's state (iosSaberWheel, for every UI)

static int hubIs(const char* want) { return !strcmp(iosSaberWheel_HubText(1), want); }

static void test_saber_wheel_state(void)
{
    iosSaberWheelCard c, l;
    iosSaberWheelIcon ic;
    setup();
    fg.bStaffOk = fg.bFoldOk = 1;
    g_case = "wheel state: to start one saber, the RIGHT card picked (its whole hand), SAME COLOUR and SAME HILT off, AUTO on";
    iosSaberWheel_BeginLoad(); iosSaberWheel_EndLoad();
    iosSaberWheel_GetCard(0, &c); iosSaberWheel_GetCard(1, &l);
    CHECK(iosSaberWheel_NumSabers() == 1 && c.bShown && !l.bShown && c.bPicked && c.pickedBlade == -1 && c.numBlades == 1 && c.bLinked);
    CHECK(!iosSaberWheel_GetSameColour() && !iosSaberWheel_GetSameHilt() && iosSaberWheel_GetAuto() && !iosSaberWheel_ShowAuto());
    CHECK(iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_HAND && hubIs("tap a colour") && !strcmp(iosSaberWheel_HubText(0), "slide toward a colour"));
    g_case = "wheel state: one saber: a colour paints the right hand; the left card can't be picked";
    iosSaberWheel_ApplyColour(5);
    iosSaberWheel_PickHand(1);
    CHECK(fg.aBladeColour[0][0] == 5 && iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_PaintColour() == 5);
    g_case = "wheel state: + 2ND SABER: two, the LEFT card shown and picked; a colour paints only the left";
    iosSaberWheel_AddSecond();
    iosSaberWheel_GetCard(1, &l);
    CHECK(iosSaberWheel_NumSabers() == 2 && fg.saberStyle == 1 && l.bShown && l.bPicked && hubIs("colour for the left saber"));
    iosSaberWheel_ApplyColour(9);
    CHECK(fg.aBladeColour[1][0] == 9 && fg.aBladeColour[0][0] == 5);
    g_case = "wheel state: the RIGHT card: a colour paints only the right";
    iosSaberWheel_PickHand(0);
    iosSaberWheel_ApplyColour(3);
    CHECK(fg.aBladeColour[0][0] == 3 && fg.aBladeColour[1][0] == 9 && hubIs("colour for the right saber") && iosSaberWheel_PaintColour() == 3);
    g_case = "wheel state: HILT on a card: only that hand, round the available ones (NORMAL CROSS STAFF FOLD)";
    int seq[5];
    for (int k = 0; k < 5; k++) seq[k] = iosSaberWheel_CycleHilt(1);
    CHECK(seq[0] == 1 && seq[1] == 2 && seq[2] == 3 && seq[3] == 0 && seq[4] == 1 && fg.aHilt[0] == 0 && fg.aHilt[1] == 1);
    fg.bFoldOk = 0;
    iosSaberWheel_SetHilt(1, 2);
    CHECK(iosSaberWheel_CycleHilt(1) == 0); // (FOLD not available: past it)
    fg.bFoldOk = 1;
    g_case = "wheel state: SAME HILT on: a HILT tap changes both hands, to the tapped hand's next";
    iosSaberWheel_SetSameHilt(1);
    iosSaberWheel_SetHilt(1, 1);
    CHECK(fg.aHilt[0] == 1 && fg.aHilt[1] == 1);
    fg.aHilt[0] = 0;
    CHECK(iosSaberWheel_CycleHilt(1) == 2 && fg.aHilt[0] == 2 && fg.aHilt[1] == 2);
    iosSaberWheel_SetSameHilt(0);
    g_case = "wheel state: a new STAFF starts linked: blade 2 shows blade 1's colour; two dots";
    iosSaberWheel_SetHilt(0, 0); iosSaberWheel_SetHilt(1, 0);
    fg.aUnlinked[0] = 1; fg.aBladeColour[0][1] = 12;
    iosSaberWheel_SetHilt(0, 2);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.numBlades == 2 && c.bLinked && c.aColour[0] == 3 && c.aColour[1] == 3 && iosSaber_GetBladeColour(0, 1) == 3);
    g_case = "wheel state: the first dot of a linked pair: blade 1 (blade 2 follows)";
    iosSaberWheel_PickBlade(0, 0);
    iosSaberWheel_ApplyColour(6);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.bLinked && c.aColour[0] == 6 && c.aColour[1] == 6 && c.pickedBlade == 0 && hubIs("right saber, blade 1"));
    g_case = "wheel state: the second dot of a linked pair only picks it: still linked, ringed, the hub says blade 2";
    fg.aBladeColour[0][1] = 9; // (its own, unseen while linked)
    iosSaberWheel_PickBlade(0, 1);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.bLinked && c.aColour[1] == 6 && c.pickedBlade == 1 && iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_BLADE && hubIs("right saber, blade 2")
          && iosSaberWheel_PaintColour() == 6);
    g_case = "wheel state: blade 2 of a linked pair given blade 1's colour: still linked, nothing changed";
    iosSaberWheel_ApplyColour(6);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.bLinked && !fg.aUnlinked[0] && fg.aBladeColour[0][0] == 6 && fg.aBladeColour[0][1] == 9 && c.pickedBlade == 1);
    g_case = "wheel state: blade 2 of a linked pair given another colour: split, blade 2 that colour, blade 1 its own; a colour then paints blade 2 only";
    iosSaberWheel_ApplyColour(14);
    iosSaberWheel_GetCard(0, &c);
    CHECK(!c.bLinked && c.aColour[0] == 6 && c.aColour[1] == 14 && c.pickedBlade == 1 && hubIs("right saber, blade 2"));
    iosSaberWheel_ApplyColour(11);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.aColour[0] == 6 && c.aColour[1] == 11 && iosSaberWheel_PaintColour() == 11);
    g_case = "wheel state: a linked pair's second blade given a colour shown as blade 1's (green off REFORGED's wheel, the game's own): still linked";
    {
        fg.aHilt[1] = 2; fg.aUnlinked[1] = 0; fg.aBladeColour[1][0] = 7; // (the left STAFF, green: shown as the game's own in REFORGED)
        fg.saberLook = 1;
        iosSaberWheel_PickBlade(1, 1);
        iosSaberWheel_ApplyColour(0);
        const int bKept = !fg.aUnlinked[1] && fg.aBladeColour[1][0] == 7;
        fg.saberLook = 0;
        fg.aHilt[1] = 0;
        CHECK(bKept);
        iosSaberWheel_PickHand(0);
        iosSaberWheel_PickBlade(0, 1);
    }
    g_case = "wheel state: split, the first dot paints blade 1 only";
    iosSaberWheel_PickBlade(0, 0);
    iosSaberWheel_ApplyColour(2);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.aColour[0] == 2 && c.aColour[1] == 11 && !c.bLinked);
    g_case = "wheel state: the card itself: the whole hand, both blades, linked again";
    iosSaberWheel_PickHand(0);
    iosSaberWheel_ApplyColour(4);
    iosSaberWheel_GetCard(0, &c);
    CHECK(c.bLinked && c.aColour[0] == 4 && c.aColour[1] == 4 && c.pickedBlade == -1 && hubIs("colour for the right saber"));
    g_case = "wheel state: SAME COLOUR on: every blade of both hands, linked; the hub says so; no blade picked while it is on";
    iosSaberWheel_PickBlade(0, 1);
    iosSaberWheel_ApplyColour(10); // (split)
    iosSaberWheel_SetSameColour(1);
    iosSaberWheel_GetCard(0, &c);
    CHECK(iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_ALL && hubIs("colour for both sabers") && iosSaberWheel_GetPickedBlade() == -1 && c.pickedBlade == -1
          && iosSaberWheel_PaintColour() == 4);
    iosSaberWheel_SetSameColour(0);
    CHECK(iosSaberWheel_GetPickedBlade() == 1 && hubIs("right saber, blade 2")); // (off with no colour between: as it was)
    iosSaberWheel_SetSameColour(1);
    iosSaberWheel_ApplyColour(13);
    CHECK(fg.aBladeColour[0][0] == 13 && fg.aBladeColour[0][1] == 13 && fg.aBladeColour[1][0] == 13 && !fg.aUnlinked[0] && !fg.aUnlinked[1]);
    g_case = "wheel state: SAME COLOUR off after a colour (every pair linked again): the pick its whole hand; a colour paints both its blades (no blade 2 left picked)";
    iosSaberWheel_SetSameColour(0);
    iosSaberWheel_GetCard(0, &c);
    CHECK(iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1 && c.bLinked && c.pickedBlade == -1 && hubIs("colour for the right saber"));
    iosSaberWheel_ApplyColour(12);
    CHECK(fg.aBladeColour[0][0] == 12 && iosSaber_GetBladeColour(0, 1) == 12 && !fg.aUnlinked[0] && fg.aBladeColour[1][0] == 13);
    g_case = "wheel state: SAME COLOUR on: a dot is its card (linked or split: the whole hand, no split)";
    iosSaberWheel_PickBlade(0, 1);
    iosSaberWheel_ApplyColour(3); // (split)
    iosSaberWheel_PickHand(1);
    iosSaberWheel_SetSameColour(1);
    iosSaberWheel_PickBlade(0, 1);
    CHECK(iosSaberWheel_GetPickedHand() == 0 && iosSaberWheel_GetPickedBlade() == -1 && fg.aUnlinked[0] && !iosSaberWheel_BladesPickable(0));
    iosSaberWheel_SetSameColour(0);
    CHECK(iosSaberWheel_GetPickedBlade() == -1 && iosSaberWheel_BladesPickable(0) && !iosSaberWheel_BladesPickable(1)); // (the left NORMAL)
    iosSaberWheel_PickHand(0);
    iosSaberWheel_ApplyColour(12);
    g_case = "wheel state: a hilt with one blade: the dot picked goes back to the whole hand";
    iosSaberWheel_PickBlade(0, 1);
    iosSaberWheel_SetHilt(0, 1);
    CHECK(iosSaberWheel_GetPickedBlade() == -1 && iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_HAND);
    g_case = "wheel state: blade 2 picked, HILT round to STAFF again (FOLD, NORMAL, CROSS, STAFF): the whole hand, linked; a colour paints both blades";
    {
        iosSaberWheel_SetHilt(0, 2);
        iosSaberWheel_PickBlade(0, 1);
        int seq = 0;
        for (int k = 0; k < 4; k++) seq = seq * 10 + iosSaberWheel_CycleHilt(0);
        iosSaberWheel_GetCard(0, &c);
        CHECK(seq == 3012 && c.hilt == 2 && c.bLinked && c.pickedBlade == -1 && iosSaberWheel_GetPickedBlade() == -1 && hubIs("colour for the right saber"));
        iosSaberWheel_ApplyColour(8);
        CHECK(fg.aBladeColour[0][0] == 8 && iosSaber_GetBladeColour(0, 1) == 8 && !fg.aUnlinked[0]);
        // (and the same with no frame between: the hilt set straight back)
        iosSaberWheel_PickBlade(0, 1);
        iosSaberWheel_SetHilt(0, 0); iosSaberWheel_SetHilt(0, 2);
        CHECK(iosSaberWheel_GetPickedBlade() == -1);
        // (STAFF to FOLD, both two blades: blade 2 still picked)
        iosSaberWheel_PickBlade(0, 1);
        iosSaberWheel_SetHilt(0, 3);
        CHECK(iosSaberWheel_GetPickedBlade() == 1);
        iosSaberWheel_SetHilt(0, 1);
    }
    g_case = "wheel state: FILM dimmed only while every hilt in use is CROSS";
    iosSaberWheel_SetHilt(0, 1); iosSaberWheel_SetHilt(1, 0);
    CHECK(!iosSaberWheel_FilmDimmed());
    iosSaberWheel_SetHilt(1, 1);
    CHECK(iosSaberWheel_FilmDimmed());
    iosSaberWheel_SetHilt(1, 2);
    CHECK(!iosSaberWheel_FilmDimmed());
    iosSaberWheel_RemoveSecond();
    CHECK(iosSaberWheel_FilmDimmed()); // (one saber: the right one's CROSS alone)
    g_case = "wheel state: the LEFT card's x: one saber, the right card picked, the left hand's kept";
    iosSaberWheel_AddSecond(); iosSaberWheel_PickHand(1);
    iosSaberWheel_RemoveSecond();
    CHECK(iosSaberWheel_NumSabers() == 1 && iosSaberWheel_GetPickedHand() == 0 && fg.aHilt[1] == 2 && fg.aBladeColour[1][0] == 13);
    g_case = "wheel state: AUTO shown with a FOLD in either hand in use";
    iosSaberWheel_SetHilt(0, 0);
    CHECK(!iosSaberWheel_ShowAuto());
    fg.aHilt[1] = 3;
    CHECK(!iosSaberWheel_ShowAuto()); // (the left hand's, with one saber: not in use)
    iosSaberWheel_AddSecond();
    CHECK(iosSaberWheel_ShowAuto());
    iosSaberWheel_SetAuto(0);
    CHECK(!fg.bFoldAuto && !iosSaberWheel_GetAuto());
    iosSaberWheel_SetAuto(1);
    g_case = "wheel state: SABER's picture: each hand's hilt, FOLD open or not, each blade's colour";
    fg.aHilt[0] = 2; fg.aUnlinked[0] = 1; fg.aBladeColour[0][0] = 1; fg.aBladeColour[0][1] = 11;
    fg.bFoldLit = 1; fg.bFoldOpen = fg.bFoldOpenL = 1;
    iosSaberWheel_GetIcon(&ic);
    CHECK(ic.numSabers == 2 && ic.aHand[0].hilt == 2 && ic.aHand[0].aColour[0] == 1 && ic.aHand[0].aColour[1] == 11 && !ic.aHand[0].bOpen
          && ic.aHand[1].hilt == 3 && ic.aHand[1].bOpen);
    g_case = "wheel state: SABER's tap flips a FOLD lit in hand, and only then";
    CHECK(iosSaberWheel_TapFlips() && iosSaberWheel_Flip() && fg.foldFlips == 1);
    fg.bFoldLit = 0;
    CHECK(!iosSaberWheel_TapFlips() && !iosSaberWheel_Flip() && fg.foldFlips == 1);
    g_case = "wheel state: green off Jedi Knight's REFORGED wheel shows as the game's own, kept as picked";
    fg.saberLook = 1;
    iosSaberWheel_PickHand(0); iosSaberWheel_ApplyColour(7);
    iosSaberWheel_GetCard(0, &c);
    CHECK(fg.aBladeColour[0][0] == 7 && c.aColour[0] == 0 && iosSaberWheel_PaintColour() == 0);
    fg.saberLook = 0;

    g_case = "wheel state: SAME COLOUR and SAME HILT on, x: one saber, both count as off (kept on); a STAFF on the right: its dots pick (ringed), a colour only the right hand, HILT only the right";
    {
        iosSaberWheel_BeginLoad(); iosSaberWheel_EndLoad();
        iosSaberWheel_AddSecond();
        iosSaberWheel_SetHilt(1, 1); // (the left CROSS)
        iosSaberWheel_PickHand(1);
        iosSaberWheel_ApplyColour(5);
        iosSaberWheel_SetSameColour(1); iosSaberWheel_SetSameHilt(1);
        const int bOnWithTwo = iosSaberWheel_GetSameColour() && iosSaberWheel_GetSameHilt();
        iosSaberWheel_RemoveSecond();
        CHECK(bOnWithTwo && !iosSaberWheel_GetSameColour() && !iosSaberWheel_GetSameHilt() && keptIs("SameColour", "1") && keptIs("SameHilt", "1")
              && iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_HAND && hubIs("tap a colour"));
        iosSaberWheel_SetHilt(0, 2); // (a STAFF on the right: only the right hand)
        CHECK(fg.aHilt[0] == 2 && fg.aHilt[1] == 1);
        iosSaberWheel_PickBlade(0, 1);
        iosSaberWheel_GetCard(0, &c);
        CHECK(iosSaberWheel_GetPickedBlade() == 1 && c.pickedBlade == 1 && iosSaberWheel_PaintKind() == IOSSABERWHEEL_PAINT_BLADE && hubIs("right saber, blade 2"));
        iosSaberWheel_ApplyColour(11);
        CHECK(fg.aUnlinked[0] && fg.aBladeColour[0][1] == 11 && fg.aBladeColour[1][0] == 5 && iosSaber_GetBladeColour(1, 1) == 5);
        iosSaberWheel_PickHand(0);
        iosSaberWheel_ApplyColour(2);
        iosSaberWheel_CycleHilt(0);
        CHECK(fg.aBladeColour[0][0] == 2 && fg.aBladeColour[1][0] == 5 && fg.aHilt[0] == 3 && fg.aHilt[1] == 1);
        g_case = "wheel state: + 2ND SABER again: the left saber as the player left it (CROSS, its colour); SAME COLOUR and SAME HILT on again";
        iosSaberWheel_AddSecond();
        CHECK(fg.aHilt[1] == 1 && fg.aBladeColour[1][0] == 5 && iosSaberWheel_GetSameColour() && iosSaberWheel_GetSameHilt() && hubIs("colour for both sabers"));
        iosSaberWheel_SetSameColour(0); iosSaberWheel_SetSameHilt(0);
    }
    g_case = "wheel kept: every choice by its name, and back the same";
    {
        char aName[32][40], aVal[32][IOSSABERWHEEL_KEPT_LEN];
        const int n = iosSaberWheel_NumKept();
        iosSaberWheel_SetSameHilt(1);
        fg.bFoldAuto = 0; fg.bFoldOpen = 1; fg.bFoldOpenL = 0;
        for (int i = 0; i < n && i < 32; i++) { snprintf(aName[i], 40, "%s", iosSaberWheel_KeptName(i)); iosSaberWheel_GetKept(i, aVal[i], IOSSABERWHEEL_KEPT_LEN); }
        const int st = fg.saberStyle, h0 = fg.aHilt[0], h1 = fg.aHilt[1], c00 = fg.aBladeColour[0][0], c01 = iosSaber_GetBladeColour(0, 1), u0 = fg.aUnlinked[0];
        iosSaberWheel_BeginLoad();
        fg.bFoldAuto = 1; fg.bFoldOpen = 0; fg.bFoldOpenL = 1;
        CHECK(fg.saberStyle == 0 && fg.aHilt[0] == 0 && fg.aBladeColour[0][0] == 0 && !iosSaberWheel_GetSameHilt());
        for (int i = 0; i < n && i < 32; i++) iosSaberWheel_LoadKept(aName[i], aVal[i]);
        iosSaberWheel_EndLoad();
        CHECK(n == 15 && fg.saberStyle == st && fg.aHilt[0] == h0 && fg.aHilt[1] == h1 && fg.aBladeColour[0][0] == c00 && iosSaber_GetBladeColour(0, 1) == c01
              && fg.aUnlinked[0] == u0 && iosSaberWheel_GetSameHilt() && !iosSaberWheel_GetSameColour() && !fg.bFoldAuto && fg.bFoldOpen && !fg.bFoldOpenL);
        g_case = "wheel kept: FoldOpen alone (a build before kept one for both): both hands";
        iosSaberWheel_BeginLoad(); iosSaberWheel_LoadKept("FoldAuto", "0"); iosSaberWheel_LoadKept("FoldOpen", "1"); iosSaberWheel_EndLoad();
        CHECK(!fg.bFoldAuto && fg.bFoldOpen && fg.bFoldOpenL);
        g_case = "wheel kept: FoldOpenRight and FoldOpenLeft, each its own (FoldOpen, the right's, as kept)";
        iosSaberWheel_BeginLoad(); iosSaberWheel_LoadKept("FoldAuto", "0"); iosSaberWheel_LoadKept("FoldOpen", "0");
        iosSaberWheel_LoadKept("FoldOpenRight", "0"); iosSaberWheel_LoadKept("FoldOpenLeft", "1"); iosSaberWheel_EndLoad();
        CHECK(!fg.bFoldOpen && fg.bFoldOpenL);
        g_case = "wheel kept: FoldOpen changed by a build before since (not the right's as kept): both hands as it says";
        iosSaberWheel_BeginLoad(); iosSaberWheel_LoadKept("FoldAuto", "0"); iosSaberWheel_LoadKept("FoldOpen", "1");
        iosSaberWheel_LoadKept("FoldOpenRight", "0"); iosSaberWheel_LoadKept("FoldOpenLeft", "0"); iosSaberWheel_EndLoad();
        CHECK(fg.bFoldOpen && fg.bFoldOpenL);
        g_case = "wheel kept: FoldOpen as the right hand's, each hand under its own name";
        {
            char v0[IOSSABERWHEEL_KEPT_LEN] = "", vr[IOSSABERWHEEL_KEPT_LEN] = "", vl[IOSSABERWHEEL_KEPT_LEN] = "";
            fg.bFoldOpen = 1; fg.bFoldOpenL = 0;
            for (int i = 0; i < n && i < 32; i++) {
                if (!strcmp(iosSaberWheel_KeptName(i), "FoldOpen")) iosSaberWheel_GetKept(i, v0, sizeof(v0));
                if (!strcmp(iosSaberWheel_KeptName(i), "FoldOpenRight")) iosSaberWheel_GetKept(i, vr, sizeof(vr));
                if (!strcmp(iosSaberWheel_KeptName(i), "FoldOpenLeft")) iosSaberWheel_GetKept(i, vl, sizeof(vl));
            }
            CHECK(!strcmp(v0, "1") && !strcmp(vr, "1") && !strcmp(vl, "0"));
            fg.bFoldAuto = 1;
            for (int i = 0; i < n && i < 32; i++)
                if (!strncmp(iosSaberWheel_KeptName(i), "FoldOpen", 8)) { iosSaberWheel_GetKept(i, v0, sizeof(v0)); CHECK(!strcmp(v0, "0")); }
            fg.bFoldAuto = 0;
        }
        printf("  kept:");
        for (int i = 0; i < n && i < 32; i++) printf(" %s=%s", aName[i], aVal[i]);
        printf("\n");
    }
    g_case = "wheel kept: links put on after the hilts, whatever the order kept";
    iosSaberWheel_BeginLoad();
    iosSaberWheel_LoadKept("LinkRight", "0"); iosSaberWheel_LoadKept("ColourRight2", "c8"); iosSaberWheel_LoadKept("ColourRight1", "c2"); iosSaberWheel_LoadKept("HiltRight", "FOLD");
    iosSaberWheel_EndLoad();
    CHECK(fg.aHilt[0] == 3 && fg.aUnlinked[0] && iosSaber_GetBladeColour(0, 0) == 2 && iosSaber_GetBladeColour(0, 1) == 8 && fg.bFoldAuto);
    g_case = "wheel kept: a published build's (SINGLE red; DUAL blue/green): one saber in its red, the left the DUAL's green";
    iosSaberWheel_BeginLoad();
    iosSaberWheel_LoadKept("Colour.SINGLE.0", "c1"); iosSaberWheel_LoadKept("Colour.DUAL.0", "c11"); iosSaberWheel_LoadKept("Colour.DUAL.1", "c7");
    iosSaberWheel_EndLoad();
    CHECK(fg.saberStyle == 0 && fg.aBladeColour[0][0] == 1 && fg.aBladeColour[1][0] == 7 && fg.aHilt[0] == 0 && fg.aHilt[1] == 0 && !fg.aUnlinked[0] && !fg.aUnlinked[1]);
    g_case = "wheel kept: a published build's with DUAL picked: the right hand DUAL's right colour (the cvars' names too)";
    iosSaberWheel_BeginLoad();
    iosSaberWheel_LoadKept("ColourSingle", "c1"); iosSaberWheel_LoadKept("ColourDualRight", "c11"); iosSaberWheel_LoadKept("ColourDualLeft", "c7");
    iosSaberWheel_LoadKept("Style", "DUAL");
    iosSaberWheel_EndLoad();
    CHECK(fg.saberStyle == 1 && fg.aBladeColour[0][0] == 11 && fg.aBladeColour[1][0] == 7);
    g_case = "wheel kept: the hilt branch's shared hilt: both hands";
    iosSaberWheel_BeginLoad();
    iosSaberWheel_LoadKept("Hilt", "CROSS"); iosSaberWheel_LoadKept("Style", "DUAL");
    iosSaberWheel_EndLoad();
    CHECK(fg.aHilt[0] == 1 && fg.aHilt[1] == 1);
    g_case = "wheel kept: each hand's own wins over what builds before kept";
    iosSaberWheel_BeginLoad();
    iosSaberWheel_LoadKept("Hilt", "CROSS"); iosSaberWheel_LoadKept("HiltLeft", "STAFF"); iosSaberWheel_LoadKept("Colour.SINGLE.0", "c1"); iosSaberWheel_LoadKept("ColourLeft1", "c4");
    iosSaberWheel_EndLoad();
    CHECK(fg.aHilt[0] == 0 && fg.aHilt[1] == 2 && fg.aBladeColour[0][0] == 0 && fg.aBladeColour[1][0] == 4);
    g_case = "wheel kept: one changed in its store (a cvar typed): taken at once; an unknown name: nothing";
    CHECK(iosSaberWheel_SetKept("HiltRight", "STAFF") && fg.aHilt[0] == 2 && !iosSaberWheel_SetKept("Wobbly", "1"));
    g_case = "wheel kept: the Android file keeps them all, pad-made changes too";
    {
        setup();
        fg.bStaffOk = fg.bFoldOk = 1;
        touchUi_Init(&fakeGame, g_settings);
        fg.aHilt[1] = 3; fg.saberStyle = 1; // (as a pad or the keys would)
        tick();
        char buf[4096] = "";
        FILE* f = fopen(g_settings, "r");
        if (f) { buf[fread(buf, 1, sizeof(buf) - 1, f)] = 0; fclose(f); }
        CHECK(strstr(buf, "saberStyle=DUAL\n") && strstr(buf, "saberHiltLeft=FOLD\n") && strstr(buf, "saberHiltRight=NORMAL\n") && strstr(buf, "saberLinkRight=1\n")
              && strstr(buf, "saberFoldAuto=1\n") && strstr(buf, "saberSameColour=0\n") && strstr(buf, "saberLook=CLASSIC\n") && strstr(buf, "saberColour.SINGLE.0=")
              && strstr(buf, "saberColour.DUAL.1="));
        fg.aHilt[1] = 0; fg.saberStyle = 0;
        touchUi_Init(&fakeGame, g_settings);
        CHECK(fg.aHilt[1] == 3 && fg.saberStyle == 1);
    }
}

int main(void)
{
    test_stick();
    test_look();
    test_buttons();
    test_quicksave();
    test_menu();
    test_gyro_settings();
    test_gyro_aim();
    test_wheels();
    test_items_cheats();
    test_other_input();
    test_swapped();
    test_camera();
    test_saber();
    test_saber_look();
    test_saber_shape();
    test_saber_hilt();
    test_saber_throw();
    test_saber_throw_cheat();
    test_saber_wheel_state();
    test_saber_cards();
    test_saber_fit();
    test_saber_x_row();
    test_prompt_lines();
    test_label_fit();
    test_shared_wheel();
    test_other_photo_and_cancel();
    test_level_tray();
    check_layout("layout: iPhone-sized (852x393, notch insets)", 852, 393, 59, 0, 59, 21);
    check_layout("layout: Android phone (873x393, cutout on the left)", 873, 393, 32, 0, 0, 0);
    check_layout("layout: Android phone (873x393, cutout on the right)", 873, 393, 0, 0, 32, 0);
    check_layout("layout: tablet (1067x667 at x1.2, nav bar)", 1067, 667, 0, 0, 0, 40);
    check_layout("layout: Android phone (732x360), everything on", 732, 360, 0, 0, 0, 0);
    // (narrower than any iPhone: all three items, CAMERA, SABER, CHEAT and VIEW
    // don't fit in one row there, on iOS either; without SABER they do)
    g_layoutSaber = 0;
    check_layout("layout: small phone (640x360), no saber yet", 640, 360, 0, 0, 0, 0);
    remove(g_settings);
    printf("%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
