// Added: entire file. AUTOPLAY's panel (MENU's tray's AUTOPLAY), over the
// touch overlay: what the show plays with, and START (the show itself:
// src/Enhanced/iosAutoplay.c; its gestures: iosTouch.m).
//
// A card over the dimmed game, which holds still meanwhile, in the overlay's
// look (the level list's: dark, gold, the wheels' condensed lettering), with
// seven rows, each "NAME   < value >": a tap on a row's right half goes on to
// the next value, on its left half back to the one before.
//   LEVELS      SHUFFLE ALL / PICK ONE (the cheat's list of levels opens to
//               pick from, whether cheat mode is on or not; the row then
//               names the level)
//   DIFFICULTY  EASY / MEDIUM / HARD (the default)
//   CROWD       1x / 2x (the default) / 3x
//   ALARM       ON (the default) / OFF
//   WEAPONS     ALL (the default), or "3 PICKED": a tap opens the picker, a
//               tile for each weapon of the game in the weapon wheel's
//               colours and names, tapped to tick or untick (one at least
//               stays ticked), ALL and DONE (its X leaves it as it was)
//   HUD         OFF (the default) / ON
//   CONTROLS    OFF (the default) / ON: the touch controls shown during the
//               show as a player's thumbs would work them (display only)
// then START, an X, and the hint "Double tap: new fight · Hold: exit". A
// touch off the card closes it, as the X does. Everything is remembered
// (the app's settings).

#ifndef _PLATFORM_IOS_IOSAUTOPLAYPANEL_H
#define _PLATFORM_IOS_IOSAUTOPLAYPANEL_H

#ifdef TARGET_IOS

#ifdef __cplusplus
extern "C" {
#endif

// Opens the panel over the overlay (pOverlay: its UIView*), if AUTOPLAY can
// start now (else a message says why); closes it
void iosAutoplayPanel_Open(void* pOverlay);
void iosAutoplayPanel_Close(void);
int iosAutoplayPanel_IsOpen(void);
// Once a frame while the overlay is up: closed once the game isn't being
// played (a menu, a level loading); laid out again for the screen's size
void iosAutoplayPanel_Tick(void);

#ifdef __cplusplus
}
#endif

#endif // TARGET_IOS

#endif // _PLATFORM_IOS_IOSAUTOPLAYPANEL_H
