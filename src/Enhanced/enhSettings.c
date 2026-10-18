// Added: entire file. See enhSettings.h.

#include "Enhanced/enhSettings.h"

#if defined(JK_ENHANCED) && !defined(TARGET_IOS)

#include <stdio.h>
#include <string.h>

#include "Enhanced/iosSaber.h"
#include "Enhanced/iosSaberShape.h"
#include "Enhanced/iosSaberThrowJk.h"
#include "Enhanced/iosSaberWheel.h"
#include "Enhanced/iosGame.h"
#include "Main/sithCvar.h"
#include "General/stdString.h"
#include "engine_config.h"
#include "types_enums.h"

// The cvars' own copies (linked)
static char enhSettings_saberLook[SITHCVAR_MAX_STRLEN];
static char enhSettings_saberShape[SITHCVAR_MAX_STRLEN];
static int32_t enhSettings_jumpStyle = IOSGAME_JUMP_CLASSIC; // (the desktops' own jump, as before; iOS: STEPS)
static int32_t enhSettings_bThirdPerson = 0;
static int32_t enhSettings_bSaberThrow = 0;

// The saber wheel's choices (iosSaberWheel's kept names, "enh_saber" + name;
// "" till first kept: the load's start, or what builds before kept), and
// what builds before kept (read to be moved; the published build's kept
// again as it reads them, enhSettings_Published, the others not saved
// again: a string cvar with an updatable default never is, sithCvar_SaveVar)
#define ENHSETTINGS_MAX_KEPT 24
#define ENHSETTINGS_MAX_OLD 16
static char enhSettings_aKeptName[ENHSETTINGS_MAX_KEPT][48];
static char enhSettings_aKept[ENHSETTINGS_MAX_KEPT][SITHCVAR_MAX_STRLEN];
static char enhSettings_aKeptSynced[ENHSETTINGS_MAX_KEPT][SITHCVAR_MAX_STRLEN];
static char enhSettings_aOldName[ENHSETTINGS_MAX_OLD][48];
static char enhSettings_aOld[ENHSETTINGS_MAX_OLD][SITHCVAR_MAX_STRLEN];
static int enhSettings_numKept = 0, enhSettings_numOld = 0;
// (the published build's: each colour's place among the old ones, -1 none; and Style's among the kept)
static const char* const enhSettings_aPubName[ENHSETTINGS_NUM_PUB] = {
    "enh_saberStyle", "enh_saberColourSingle", "enh_saberColourDualRight", "enh_saberColourDualLeft", "enh_saberColourCross"
};
static int enhSettings_aPubOld[ENHSETTINGS_NUM_PUB];
static int enhSettings_styleKept = -1;

// What was last made the same both ways (cvars and gameplay), to tell which changed
typedef struct {
    int look;
    int shape;
    int jumpStyle;
    int bThirdPerson;
    int bSaberThrow;
} enhSettingsState;
static enhSettingsState enhSettings_synced;
static int enhSettings_bSynced = 0;

void enhSettings_StartupCvars(void)
{
    sithCvar_RegisterStr("enh_saberLook", iosSaber_LookName(IOSSABER_LOOK_CLASSIC), enhSettings_saberLook, CVARFLAG_GLOBAL);
    sithCvar_RegisterStr("enh_saberShape", iosSaberShape_Name(IOSSABERSHAPE_DEFAULT), enhSettings_saberShape, CVARFLAG_GLOBAL);
    enhSettings_numKept = 0;
    enhSettings_styleKept = -1;
    for (int i = 0; i < iosSaberWheel_NumKept() && enhSettings_numKept < ENHSETTINGS_MAX_KEPT; i++) {
        const int k = enhSettings_numKept++;
        snprintf(enhSettings_aKeptName[k], sizeof(enhSettings_aKeptName[k]), "enh_saber%s", iosSaberWheel_KeptName(i));
        if (!strcmp(enhSettings_aKeptName[k], enhSettings_aPubName[ENHSETTINGS_PUB_STYLE])) enhSettings_styleKept = k;
        sithCvar_RegisterStr(enhSettings_aKeptName[k], "", enhSettings_aKept[k], CVARFLAG_GLOBAL);
    }
    // (read; the published build's colours saved again as it reads them, the rest not)
    enhSettings_numOld = 0;
    for (int w = 0; w < ENHSETTINGS_NUM_PUB; w++) enhSettings_aPubOld[w] = -1;
    for (int i = 0; i < iosSaberWheel_NumOldKept() && enhSettings_numOld < ENHSETTINGS_MAX_OLD; i++) {
        const char* name = iosSaberWheel_OldKeptName(i);
        if (strchr(name, '.')) continue; // (the overlays' spelling: not a cvar's)
        const int k = enhSettings_numOld++;
        snprintf(enhSettings_aOldName[k], sizeof(enhSettings_aOldName[k]), "enh_saber%s", name);
        int pub = -1;
        for (int w = ENHSETTINGS_PUB_STYLE + 1; w < ENHSETTINGS_NUM_PUB; w++)
            if (!strcmp(enhSettings_aOldName[k], enhSettings_aPubName[w])) pub = w;
        if (pub >= 0) enhSettings_aPubOld[pub] = k;
        sithCvar_RegisterStr(enhSettings_aOldName[k], "", enhSettings_aOld[k], CVARFLAG_GLOBAL | (pub >= 0 ? 0 : CVARFLAG_UPDATABLE_DEFAULT));
    }
    sithCvar_RegisterInt("enh_forceJumpStyle", IOSGAME_JUMP_CLASSIC, &enhSettings_jumpStyle, CVARFLAG_GLOBAL);
    sithCvar_RegisterBool("enh_thirdPerson", 0, &enhSettings_bThirdPerson, CVARFLAG_GLOBAL);
    sithCvar_RegisterBool("enh_saberThrow", 0, &enhSettings_bSaberThrow, CVARFLAG_GLOBAL);
}

// The cvars, as the gameplay takes them (an unknown name or key: the default)
static void enhSettings_FromCvars(enhSettingsState* p)
{
    const int look = iosSaber_LookByName(enhSettings_saberLook);
    p->look = look >= 0 ? look : IOSSABER_LOOK_CLASSIC;
    const int shape = iosSaberShape_ByName(enhSettings_saberShape);
    p->shape = shape >= 0 ? shape : IOSSABERSHAPE_DEFAULT;
    p->jumpStyle = (enhSettings_jumpStyle >= 0 && enhSettings_jumpStyle < IOSGAME_JUMP_NUM_STYLES) ? enhSettings_jumpStyle : IOSGAME_JUMP_CLASSIC;
    p->bThirdPerson = enhSettings_bThirdPerson ? 1 : 0;
    p->bSaberThrow = enhSettings_bSaberThrow ? 1 : 0;
}

static void enhSettings_FromGameplay(enhSettingsState* p)
{
    p->look = iosSaber_GetLook();
    p->shape = iosSaberShape_Get();
    p->jumpStyle = iosGame_GetForceJumpStyle();
    p->bThirdPerson = iosGame_IsThirdPerson() ? 1 : 0;
    p->bSaberThrow = iosSaberThrowJk_Get() ? 1 : 0;
}

// The saber wheel's choices: the first time, the saved ones (and what builds
// before kept) into the gameplay, then all of them into the cvars; after, a
// cvar changed (the console) into the gameplay, and the gameplay changed (a
// wheel, a pad, the keys) into the cvar
static void enhSettings_SyncKept(int bFirst)
{
    char now[SITHCVAR_MAX_STRLEN];
    if (bFirst) {
        iosSaberWheel_BeginLoad();
        for (int k = 0; k < enhSettings_numOld; k++)
            if (enhSettings_aOld[k][0]) iosSaberWheel_LoadKept(enhSettings_aOldName[k] + 9, enhSettings_aOld[k]);
        for (int k = 0; k < enhSettings_numKept; k++)
            if (enhSettings_aKept[k][0]) iosSaberWheel_LoadKept(enhSettings_aKeptName[k] + 9, enhSettings_aKept[k]);
        iosSaberWheel_EndLoad();
    }
    for (int k = 0; k < enhSettings_numKept; k++) {
        if (!bFirst && enhSettings_aKept[k][0] && strcmp(enhSettings_aKept[k], enhSettings_aKeptSynced[k])) {
            iosSaberWheel_SetKept(enhSettings_aKeptName[k] + 9, enhSettings_aKept[k]);
            stdString_SafeStrCopy(enhSettings_aKeptSynced[k], enhSettings_aKept[k], SITHCVAR_MAX_STRLEN);
        }
        // (Style as the published build reads it: one saber with a CROSS, CROSS; this build reads that back as one saber)
        if (k == enhSettings_styleKept) enhSettings_Published(ENHSETTINGS_PUB_STYLE, now, sizeof(now));
        else iosSaberWheel_GetKept(k, now, sizeof(now));
        if (bFirst || strcmp(now, enhSettings_aKeptSynced[k])) {
            sithCvar_SetStr(enhSettings_aKeptName[k], now);
            stdString_SafeStrCopy(enhSettings_aKeptSynced[k], now, SITHCVAR_MAX_STRLEN);
        }
    }
    // the published build's colours, as the choices are now (after the old ones were read: the first time, moved)
    for (int w = ENHSETTINGS_PUB_STYLE + 1; w < ENHSETTINGS_NUM_PUB; w++) {
        const int k = enhSettings_aPubOld[w];
        if (k < 0) continue;
        enhSettings_Published(w, now, sizeof(now));
        if (strcmp(now, enhSettings_aOld[k])) sithCvar_SetStr(enhSettings_aOldName[k], now);
    }
}

void enhSettings_Sync(void)
{
    enhSettingsState c, g;
    enhSettings_FromCvars(&c);
    const int bFirst = !enhSettings_bSynced;
    if (!enhSettings_bSynced) {
        // (the first time: the saved ones into the gameplay)
        enhSettings_FromGameplay(&g);
        enhSettings_synced = g;
        enhSettings_bSynced = 1;
    }
    else {
        enhSettings_FromGameplay(&g);
    }
    enhSettingsState* s = &enhSettings_synced;
    enhSettings_SyncKept(bFirst);

    // A cvar changed: into the gameplay
    if (c.look != s->look) {
        iosSaber_SetLook(c.look);
        s->look = g.look = c.look;
    }
    if (c.shape != s->shape) {
        iosSaberShape_Set(c.shape);
        s->shape = g.shape = c.shape;
    }
    if (c.jumpStyle != s->jumpStyle) {
        iosGame_SetForceJumpStyle(c.jumpStyle);
        s->jumpStyle = g.jumpStyle = c.jumpStyle;
    }
    if (c.bThirdPerson != s->bThirdPerson) {
        iosGame_SetThirdPerson(c.bThirdPerson);
        s->bThirdPerson = g.bThirdPerson = c.bThirdPerson;
    }
    if (c.bSaberThrow != s->bSaberThrow) {
        iosSaberThrowJk_Set(c.bSaberThrow);
        s->bSaberThrow = g.bSaberThrow = c.bSaberThrow;
    }

    // The gameplay changed (the camera key, a pick made in the game): into the cvar
    if (g.look != s->look) {
        sithCvar_SetStr("enh_saberLook", iosSaber_LookName(g.look));
        s->look = g.look;
    }
    if (g.shape != s->shape) {
        sithCvar_SetStr("enh_saberShape", iosSaberShape_Name(g.shape));
        s->shape = g.shape;
    }
    if (g.jumpStyle != s->jumpStyle) {
        sithCvar_SetInt("enh_forceJumpStyle", g.jumpStyle);
        s->jumpStyle = g.jumpStyle;
    }
    if (g.bThirdPerson != s->bThirdPerson) {
        sithCvar_SetBool("enh_thirdPerson", g.bThirdPerson);
        s->bThirdPerson = g.bThirdPerson;
    }
    if (g.bSaberThrow != s->bSaberThrow) {
        sithCvar_SetBool("enh_saberThrow", g.bSaberThrow);
        s->bSaberThrow = g.bSaberThrow;
    }
}

#endif // JK_ENHANCED && !TARGET_IOS
