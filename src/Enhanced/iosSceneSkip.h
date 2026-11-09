#ifndef _ENHANCED_IOSSCENESKIP_H
#define _ENHANCED_IOSSCENESKIP_H

// Added: entire file. A level's own scenes skipped: the scripted scenes the
// levels play with the game's own engine -- a dark Jedi met before a duel
// (Jedi Knight's Yun, Maw, Gorc and Pic, Sariss, Boc, Jerec; Mysteries of the
// Sith's rancor, Abron, the last level's coffin...), a level's end. The level's
// script takes the player's controls away (SITH_AF_CONTROLSDISABLED) and puts
// the camera on something else (a ghost it flies along a path), and gives both
// back when it is over.
//
// A skip doesn't cut the script short: the game runs on, many times faster,
// unseen (the world isn't drawn: a black frame), its sounds silenced, until
// the scene ends by itself, so the level's scripts finish exactly as when the
// scene is watched -- the dark Jedi woken, doors, music, the autosave -- and
// play starts in the same state. A tap on the touch overlay asks for it
// (iosTouch), and AUTOPLAY's show skips every scene it meets (iosAutoplay).
//
// Single player only: in a multiplayer or co-op game the scenes play as
// always (each player's game keeps its own time with the host's).

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

// Whether one of the level's scenes is playing now (the player's controls
// taken away, the camera on something else; the player alive; not a photo
// mode's camera), single player
int iosSceneSkip_InScene(void);

// Whether it can be skipped now: in a scene, not being skipped already, the
// game running (no wheel holding it). A scene still going after two minutes
// of it skipped (none of the games' own: a level made to keep the controls
// away) is given up on: it plays on as it is, and can't be skipped again.
int iosSceneSkip_CanSkip(void);

// The scene skipped: run on unseen to its own end. 0, and nothing changed,
// if it can't be now.
int iosSceneSkip_Skip(void);

// Whether a skip is running (the world unseen, the overlay's "Skipping...")
int iosSceneSkip_IsSkipping(void);

// AUTOPLAY's show (on while it runs): every scene skipped by itself from its
// very start -- the update it begins in -- so nothing of it is ever drawn (a
// frame with a scene on and no skip yet is left black too)
void iosSceneSkip_SetAuto(int bOn);

// Counts the scenes begun (one more each time a scene starts, checked once
// a frame): the overlay shows its "Tap to skip" when it changes
unsigned int iosSceneSkip_Serial(void);

// The engine's side. Frame: once a frame (enh_Tick). Run: just after the
// game's own update (jkMain_GameplayTick), while a skip runs, the game run on
// many updates more, as many as fit in a share of a frame. HidesWorld:
// whether the world is left undrawn this frame (jkGame_Update).
void iosSceneSkip_Frame(void);
void iosSceneSkip_Run(void);
int iosSceneSkip_HidesWorld(void);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSCENESKIP_H
