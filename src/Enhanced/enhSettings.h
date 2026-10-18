#ifndef _ENHANCED_ENHSETTINGS_H
#define _ENHANCED_ENHSETTINGS_H

// Added: entire file. Where the enhanced gameplay's choices are kept: the
// saber (one or two, each hand's hilt, each blade's colour, the saber wheel's
// switches), Force Jump's style, first or third person, THROW.
//
// The gameplay itself always holds the current ones (iosSaber_GetStyle,
// iosSaber_GetHandHilt, iosSaber_GetBladeColour, iosGame_IsThirdPerson...),
// which shared code reads. They are remembered:
//   - on iOS by the touch overlay, in NSUserDefaults, as since the first
//     builds (iosTouch.m: read at its start, written when changed); nothing
//     here runs there, so no setting moves and none is lost;
//   - with the touch overlay (Android), in its settings file (touchUi.c);
//   - elsewhere as global cvars, kept in openjkdf2_cvars.json and settable
//     from the console:
//       enh_saberLook             CLASSIC (the default) or REFORGED: how the
//                                 blades' colours are drawn (iosSaber)
//       enh_saberShape            SPIKE (the default) or BEAM (FILM on the screen, and taken too): the blades'
//                                 shape (iosSaberShape)
//       enh_saber<name>           the saber wheel's choices, each by its name
//                                 (iosSaberWheel.h: Style SINGLE or DUAL,
//                                 HiltRight, HiltLeft, ColourRight1 ... a
//                                 colour's key, "game" the game's own; the
//                                 links, SAME COLOUR and SAME HILT, FOLD's
//                                 AUTO and how it was left), from the saved
//                                 ones and, the first time, from what builds
//                                 before kept (enh_saberHilt, both hands';
//                                 enh_saberStyle CROSS; enh_saberColourSingle,
//                                 enh_saberColourDualRight, enh_saberColourDualLeft,
//                                 enh_saberColourCross): those are read
//                                 once; the published build's are kept
//                                 too (enhSettings_Published, below), the
//                                 others not saved again
//       enh_forceJumpStyle        0 smooth, 1 steps, 2 classic (IOSGAME_JUMP_*;
//                                 classic by default, the desktops' own jump)
//       enh_thirdPerson           0 or 1
//       enh_saberThrow            0 (the default) or 1: THROW, Jedi Knight's
//                                 Saber Throw on the Force wheel (iosSaberThrowJk)
//     A cvar changed (the console, the file) is applied; a choice changed in
//     the game (a wheel, a pad, the keys, the camera key...) is written back,
//     to be saved with the rest.

#if defined(JK_ENHANCED) && !defined(TARGET_IOS)

#include <stdio.h>
#include "Enhanced/iosSaber.h"

#ifdef __cplusplus
extern "C" {
#endif

// The saber as the published build kept it (v0.8-beta2: one style, each
// style's colours), worked out from the choices now and kept beside them
// (here, and in the touch overlay's file, touchUi.c), so that going back to
// it finds the player's saber, and this build, back again, reads it as
// moved if that one wrote over it. Each by its key ("game" the game's own):
//   ENHSETTINGS_PUB_STYLE        SINGLE or DUAL (CROSS: one saber with a CROSS
//                                in the right hand, as builds before HILT
//                                kept it; a build without CROSS takes it as
//                                SINGLE). The saber wheel's own Style too
//   ENHSETTINGS_PUB_SINGLE       one saber's colour: the right hand's first blade's
//   ENHSETTINGS_PUB_DUAL_RIGHT   DUAL's right one: the right hand's first blade's
//   ENHSETTINGS_PUB_DUAL_LEFT    DUAL's left one: the left hand's first blade's
//   ENHSETTINGS_PUB_CROSS        CROSS's (as one saber's)
enum {
    ENHSETTINGS_PUB_STYLE, ENHSETTINGS_PUB_SINGLE, ENHSETTINGS_PUB_DUAL_RIGHT, ENHSETTINGS_PUB_DUAL_LEFT, ENHSETTINGS_PUB_CROSS,
    ENHSETTINGS_NUM_PUB
};
static inline void enhSettings_Published(int what, char* pOut, int len)
{
    if (!pOut || len <= 0) return;
    const char* v = NULL;
    if (what == ENHSETTINGS_PUB_STYLE) {
        const int style = iosSaber_GetStyle();
        v = style == IOSSABER_STYLE_DUAL ? iosSaber_StyleName(style)
            : (iosSaber_GetHandHilt(IOSSABER_HAND_RIGHT) == IOSSABER_HILT_CROSS ? "CROSS" : iosSaber_StyleName(IOSSABER_STYLE_SINGLE));
    }
    else if (what > ENHSETTINGS_PUB_STYLE && what < ENHSETTINGS_NUM_PUB) {
        const int hand = what == ENHSETTINGS_PUB_DUAL_LEFT ? IOSSABER_HAND_LEFT : IOSSABER_HAND_RIGHT;
        v = iosSaber_ColourKey(iosSaber_GetBladeColour(hand, 0));
        if (!v) v = "game";
    }
    snprintf(pOut, len, "%s", v ? v : "");
}

// sithCvar_Startup: the cvars, before the saved ones are read
void enhSettings_StartupCvars(void);

// Once a frame (enh_Tick): cvars and gameplay kept the same (see above)
void enhSettings_Sync(void);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED && !TARGET_IOS

#endif // _ENHANCED_ENHSETTINGS_H
