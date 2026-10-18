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
//                                 once, and not saved again
//       enh_forceJumpStyle        0 smooth, 1 steps, 2 classic (IOSGAME_JUMP_*;
//                                 classic by default, the desktops' own jump)
//       enh_thirdPerson           0 or 1
//       enh_saberThrow            0 (the default) or 1: THROW, Jedi Knight's
//                                 Saber Throw on the Force wheel (iosSaberThrowJk)
//     A cvar changed (the console, the file) is applied; a choice changed in
//     the game (a wheel, a pad, the keys, the camera key...) is written back,
//     to be saved with the rest.

#if defined(JK_ENHANCED) && !defined(TARGET_IOS)

#ifdef __cplusplus
extern "C" {
#endif

// sithCvar_Startup: the cvars, before the saved ones are read
void enhSettings_StartupCvars(void);

// Once a frame (enh_Tick): cvars and gameplay kept the same (see above)
void enhSettings_Sync(void);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED && !TARGET_IOS

#endif // _ENHANCED_ENHSETTINGS_H
