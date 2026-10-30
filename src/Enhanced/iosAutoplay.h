// Added: entire file. AUTOPLAY, a self-playing show for screen recordings
// (MENU's tray on iOS: its panel in the touch overlay, iosTouch.m; the game
// side, the show and its autopilot, is iosAutoplay.c).
//
// START saves the game as it is ("Before AUTOPLAY"), then loads a level (the
// one picked, or one of the episode's shuffled) at AUTOPLAY's difficulty,
// straight in, and plays one fight there: one of the level's best groups of
// enemies, Kyle (Mara in Mysteries of the Sith) placed somewhere safe near
// it with a clear view, copies of the group's own kinds about it (CROWD),
// everyone near woken (ALARM), and more of its kinds coming in waves once
// it is down. The autopilot plays as a person would, through the game's own
// keys and mouse movement (as the touch overlay does), with god mode, full
// ammo and full Force throughout (the sithewok cheat, on for the show
// only). It runs until the player acts: iosAutoplay_Next (a double tap) is
// a new fight, iosAutoplay_Stop (a long press) loads the save back.
//
// This header is the touch overlay's too, which can't include the engine's
// headers: plain C types only.

#ifndef _ENHANCED_IOSAUTOPLAY_H
#define _ENHANCED_IOSAUTOPLAY_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

// WEAPONS: a bit for each weapon (iosAutoplay_WeaponBit: Jedi Knight's bins 1-10,
// Mysteries of the Sith's 121-140 as 1-20); all of them: ALL
#define IOSAUTOPLAY_WEAPONS_ALL 0xFFFFFFFFu
int iosAutoplay_WeaponBit(int bin); // (-1: not a weapon AUTOPLAY knows)

typedef struct iosAutoplayOptions {
    int levelRow;          // LEVELS: -1 SHUFFLE ALL, else PICK ONE's row of the level list (iosAutoplay_LevelRows)
    int difficulty;        // DIFFICULTY: 0 easy, 1 medium, 2 hard
    int crowd;             // CROWD: 1, 2 or 3 times the group
    int bAlarm;            // ALARM: everyone near woken
    unsigned int weapons;  // WEAPONS: IOSAUTOPLAY_WEAPONS_ALL, or the bits of those picked
    int bHud;              // HUD: the game's HUD shown during the show
    int bControls;         // CONTROLS: the touch controls shown as a player's thumbs would work them
} iosAutoplayOptions;

// Whether AUTOPLAY can start now: a single-player level being played
int iosAutoplay_CanStart(void);
// START: 1 if it started
int iosAutoplay_Start(const iosAutoplayOptions* pOptions);
// A long press: the show ends and the save is loaded back
void iosAutoplay_Stop(void);
// A double tap: a new fight (SHUFFLE ALL: another level; PICK ONE: another
// group in this level). Ignored while a level loads: 1 if it was taken.
int iosAutoplay_Next(void);
// Whether the show is on (from START until it ends)
int iosAutoplay_IsOn(void);
// Once a frame (enh_Tick)
void iosAutoplay_Tick(double now);

// The level list for PICK ONE: the episode's levels (the cheat's list, its
// rows: iosGame_LevelRowInfo), whether or not the cheat is on; 0 in
// multiplayer or with no level running
int iosAutoplay_LevelRows(void);

// The game's side of the show (engine code):
// Whether the show holds this key down (and every other key up: the
// keyboard is the autopilot's while it plays)
int iosAutoplay_OwnsKeys(void);
int iosAutoplay_IsScancodeDown(int scancode);
// Whether the game's HUD is put away (HUD OFF)
int iosAutoplay_HidesHud(void);
// Whether a loading screen goes straight on (no waiting for a tap)
int iosAutoplay_SkipsLoadingWait(void);
// The level was finished (its exit, a boss down): the episode entry to go to
// instead of the next one (another level of the show), or -1
int iosAutoplay_LevelEnded(void);
// A level opened (jkDev_Custom_LevelOpened); a game loaded (jkDev_Custom_GameLoaded)
void iosAutoplay_LevelOpened(void);
void iosAutoplay_GameLoaded(void);

// CONTROLS ON: what the autopilot does, for the overlay to show as a
// player's thumbs would work it (display only). Buttons: IOSAUTOPLAY_BTN_*.
#define IOSAUTOPLAY_BTN_FIRE    1
#define IOSAUTOPLAY_BTN_ALT     2
#define IOSAUTOPLAY_BTN_DUCK    4
#define IOSAUTOPLAY_BTN_JUMP    8
#define IOSAUTOPLAY_BTN_FORCE   16
#define IOSAUTOPLAY_BTN_NEXTWPN 32
#define IOSAUTOPLAY_BTN_WHEEL   64
typedef struct iosAutoplayDisplay {
    int bOn;             // the controls shown (CONTROLS ON, a fight going on)
    float moveX, moveY;  // the stick: right and forward, -1..1
    int bRun;            // pushed up onto the run marker
    unsigned int buttons;
} iosAutoplayDisplay;
void iosAutoplay_GetDisplay(iosAutoplayDisplay* pOut);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSAUTOPLAY_H
