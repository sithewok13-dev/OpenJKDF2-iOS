// Added: entire file. Saber Throw in Jedi Knight, which has none of its own:
// a THROW switch on the saber wheel (off unless switched on; remembered by
// the platform around this: the iOS overlay in NSUserDefaults, elsewhere the
// cvar enh_saberThrow, Android's touch overlay in its own settings too) puts
// SABER THROW on the Force wheel for the whole game. The sithewok cheat puts
// it there too, whatever the switch says. Single player only: never in
// multiplayer or co-op, nor in Mysteries of the Sith, which has its own
// (iosSaberThrow).
//
// The power (bin 37, Mysteries of the Sith's f_saberthrow, free in Jedi
// Knight) is registered while it is on the wheel, after the game's own items
// (so the HUD's list of powers never has it), and is outside the game's
// Force powers: it never uses or changes stars, the star screen, ranks,
// alignment or the ending. Its level follows Kyle's Jedi rank, read live:
// ceil(rank / 2) stars (4 with the cheat). It needs the lightsaber: with
// none (levels 1-3, the start of level 16 on the light side) it is dimmed
// and can't be used. Switched off (or dimmed) while it is the power picked,
// the one picked before it is picked again (else the next one there is).
//
// A throw costs Mysteries of the Sith's prices, 100 / 80 / 60 / 40 Force at
// 1 / 2 / 3 / 4 stars, and a full meter always throws; nothing while the
// game's surges (s_dark, s_light) or sithewok's endless Force are on, as for
// the game's own neutral powers. Kyle throws with his "magic" move (the
// toss where his model has none) the level's own cut thrown saber
// (+force_saber), made to fly as Mysteries of the Sith's does (speed, time
// out, spin, damage, light) and to strike with sparks, never dropping
// a saber to pick up; it hums as it flies. It turns back when it strikes or
// has flown its time and flies back to him, harmless now (sliding along a wall
// in its way, through anyone), and is caught; 5 s after the throw it is back
// however it went. The saber never
// leaves the inventory (a game saved mid-throw keeps it): with none left in
// hand he is on his fists at once, without the saber's put-away, and the
// saber can't be picked until one is back, when it is taken in hand again.
// Two sabers (DUAL) throw one a press, the right then the left, each in its
// own colour, as in Mysteries of the Sith (iosSaberThrow). A thrown saber is
// drawn in the colour, look and shape of the one thrown (iosSaber).
//
// What is out and where is in memory only: a new level or a load takes our
// sabers out of the world and leaves them in hand (back in his hand, if a throw
// had left him on his fists: the power's item keeps that in a saved game).

#ifndef _ENHANCED_IOSSABERTHROWJK_H
#define _ENHANCED_IOSSABERTHROWJK_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

struct SithThing; // (types.h's; this header is the touch overlays' too, which have none)

#define IOSSABERTHROWJK_BIN 37 // (Mysteries of the Sith's f_saberthrow; no item of Jedi Knight's)

// THROW: SABER THROW on the Force wheel (off to start; the platform keeps it)
int iosSaberThrowJk_Get(void);
void iosSaberThrowJk_Set(int bOn);
void iosSaberThrowJk_Toggle(void);
// Whether THROW is offered, on the saber wheel: Jedi Knight, single player
// (and no level of a mod's own with a bin 37)
int iosSaberThrowJk_Offered(void);
// Whether SABER THROW is on the Force wheel now: THROW on, or sithewok on
// (offered). Dimmed there, iosGame_IsPowerAvailable, while it can't be used.
int iosSaberThrowJk_OnWheel(void);

// jkHudInv_InitItems, before the game's items are read: the power let go
// (registered again, after them, once a level runs); and after: whether they
// have a bin 37 of their own (a mod's), when none of this is offered
void iosSaberThrowJk_ItemsLoading(void);
void iosSaberThrowJk_ItemsLoaded(void);
// Once a frame: the power kept as THROW and the cheat say, the sabers out
// on their way, the one due next thrown
void iosSaberThrowJk_Tick(void);
// The use-force key with the power picked (sithInventory's power keys),
// Jedi Knight: 1 if it is this throw's (thrown or refused)
int iosSaberThrowJk_Activate(struct SithThing* pPlayer);
// sithWeapon's picks (SelectWeapon, the next and previous weapon): whether
// this weapon is held back for now -- the saber, while none is in hand
int iosSaberThrowJk_WeaponHeldBack(struct SithThing* pThing, int bin);
// A new level or a load: our sabers out of the world, all in hand
void iosSaberThrowJk_Reset(void);
// A game about to be read (sithGamesave_RestoreFile, before its inventory):
// the on-fists mark in the power's item cleared, so only the save's own counts
// (a game saved without the power has none)
void iosSaberThrowJk_GameLoading(void);
// A game loaded (after the reset): its sabers of ours taken away at once, and
// the saber in hand again if it was saved soon after a throw left him on his fists
void iosSaberThrowJk_GameLoaded(void);
// Whether he is on his fists only because of a throw (sithewok keeps the saber
// for the next level, not the fists)
int iosSaberThrowJk_OnFists(void);

// For the drawing (iosSaberThrow's, which asks this in Jedi Knight): which
// sabers are in hand (bit 0 the right, bit 1 the left; 3 with nothing out),
// and a thrown saber of ours: the colour (the SABER wheel's id), style and
// hilt (its hand's) it was thrown in, or -1
int iosSaberThrowJk_HandsInHand(void);
int iosSaberThrowJk_ThrownColour(struct SithThing* pThing);
int iosSaberThrowJk_ThrownBladeColour(struct SithThing* pThing, int blade); // (blade 1: STAFF's and FOLD's second)
int iosSaberThrowJk_ThrownStyle(struct SithThing* pThing);
int iosSaberThrowJk_ThrownHilt(struct SithThing* pThing);
int iosSaberThrowJk_ThrownHand(struct SithThing* pThing); // (0 the right saber or the one, 1 the left; -1)

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERTHROWJK_H
