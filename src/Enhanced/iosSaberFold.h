// Added: entire file. FOLD, Dark Rey's folding double saber (iosSaber's
// IOSSABER_HILT_FOLD; drawn by iosSaberStaff): two hilts hinged at their
// pommels, folded (both halves side by side, the blades parallel and close,
// held by one hilt, swung as the game's own one saber) or open (a straight
// staff, a blade out of each end, held as STAFF is). Here: whether each one
// is folded or open, and the snap between (0.3 s, the free half swinging out
// sideways and down through half a turn round the hinge, with a mechanical
// click made from the game's own sounds).
//
// The local player's (each saber its own: with two, one in each hand):
//  - It ignites folded (lit, picked, a new level, a load, a death).
//  - AUTO (AUTO OPEN, on to start): the first swing opens it, and it folds
//    again once 5 s have gone by without one (never before a swing); two
//    open and fold together. Off: each changes only when the player flips
//    it, and stays as they left it (lit again, a new level, a load: kept,
//    the platform keeping each with the AUTO switch).
//  - A flip always wins, for that saber: folded by hand, it stays folded
//    (AUTO opening it no more) until 5 s go by without a swing; opened by
//    hand, it stays open until a swing and then 5 s without one.
//  - The flips: SABER's tap (iosSaberFold_TapFlip) the right hand's FOLD, a
//    second tap soon after the left's too (so a double tap flips both; one
//    FOLD in hand, every tap flips it); the saber's key the same
//    (iosSaberFold_KeyFlip); the saber picked again on a weapon wheel, both
//    (iosSaberFold_Flip).
//  - Thrown (Mysteries of the Sith's throw, and Jedi Knight's, iosSaberThrow):
//    thrown open, it flies open; thrown folded, it leaves the hand folded and
//    snaps open 0.25 s after (clear of the thrower), with the click; two are
//    each their own. Caught with AUTO on: open (it was open in flight; the
//    other in hand opening with it, as on a swing, unless folded by hand),
//    and AUTO folds it 5 s after if no swing comes; AUTO off: as the player
//    left that saber (thrown folded: snapping shut on the catch, as it is
//    back in the hand, before it is lit again). A throw is not a swing.
// Other players' (multiplayer): as their game sends it (iosSaberFold_SetOpenBits,
// the look's trailing flags byte, jkPlayerLook); folded until then.
//
// Only how it looks: it strikes as the game's own saber whatever it is (the
// second blade never strikes), and the game is sent nothing of it but the look.

#ifndef _ENHANCED_IOSSABERFOLD_H
#define _ENHANCED_IOSSABERFOLD_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

struct SithThing;

#define IOSSABERFOLD_SNAP_MS 300          // (the snap: open from folded, or back)
#define IOSSABERFOLD_CALM_MS 5000         // (AUTO folds it this long after the last swing)
#define IOSSABERFOLD_THROWN_SNAP_MS 250   // (thrown folded: it snaps open this long after it leaves the hand)
#define IOSSABERFOLD_AGAIN_MS 350         // (a second tap or press this soon: the left hand's too, as JUMP's double tap)

// ---- for the overlays (touch, pad, keyboard)
// Whether the local player has a FOLD saber lit in hand now (either hand's),
// so SABER's tap flips it (iosSaberFold_TapFlip) instead of opening the wheel
int iosSaberFold_CanFlip(void);
// The local player's FOLDs in hand flipped (each from how it is: folded to
// open, open to folded; two the same flip together, two apart swap); 0 if
// there is none to flip (iosSaberFold_CanFlip)
int iosSaberFold_Flip(void);
// SABER's tap: with a FOLD in each hand, the right hand's flipped, or with
// bAgain (a second tap within IOSSABERFOLD_AGAIN_MS of one that gave 2) the
// left's; with one in hand, that one. 2: the right one of two flipped (a tap
// again soon flips the left), 1: flipped, 0: none to flip
int iosSaberFold_TapFlip(int bAgain);
// The saber's key pressed again (Jedi Knight's): as a tap, a second press
// within IOSSABERFOLD_AGAIN_MS (the game's clock) being the second tap; 0 if
// there is none to flip
int iosSaberFold_KeyFlip(void);
// AUTO (see above), remembered by the platform
int iosSaberFold_GetAuto(void);
void iosSaberFold_SetAuto(int bOn);
// With AUTO off, how the player left each saber (0 the right hand's, 1 the
// left's; 1: open), for the platform to keep and give back at start; with
// AUTO on, whatever it is now. (GetHeldOpen: the right's; SetHeldOpen: both,
// as builds before kept it, one for both)
int iosSaberFold_GetSaberHeldOpen(int saber);
void iosSaberFold_SetSaberHeldOpen(int saber, int bOpen);
int iosSaberFold_GetHeldOpen(void);
void iosSaberFold_SetHeldOpen(int bOpen);
// Whether a thing's saber in that hand (0: the right, 1: the left) is open or
// opening (its look, for the SABER icon); 0 for a folded one, and for any
// other saber. Changes are seen by asking again (the icon compares).
int iosSaberFold_IsOpen(struct SithThing* pThing, int hand);
// ...the local player's (the SABER button's picture: iosSaberWheel)
int iosSaberFold_LocalOpen(int hand);

// ---- for drawing (iosSaberStaff)
// How far open a thing's saber is now (saber 0: the right's, 1: the left's; 0
// folded .. 1 open), eased, by the game's clock
float iosSaberFold_OpenFraction(struct SithThing* pThing, int saber);
// A saber of the local player's out on a throw (saber 0, 1, as above): how
// far open it is (not out: as in the hand), and the hilt it was thrown with
// (-1: none out)
float iosSaberFold_ThrownOpenFraction(int saber);
int iosSaberFold_ThrownHilt(int saber);

// ---- multiplayer (the look's flags, jkPlayerLook: each hand's FOLD open):
// bit 0 the right FOLD open (where it is going), bit 1 the left; another
// player's set as their game sent it (each saber snapping there, on our
// clock, as it changes; iosSaberFold_Tick, from the look as last heard)
int iosSaberFold_OpenBits(struct SithThing* pThing);
void iosSaberFold_SetOpenBits(struct SithThing* pThing, int bits);

// ---- the engine's side
// Once a frame (jkDev, after the throws'): AUTO, the snaps, throws and catches
void iosSaberFold_Tick(void);
// A swing begun (jkSaber_Enable, through iosSaberStaff_Swing)
void iosSaberFold_Swing(struct SithThing* pThing);
// A saber of the local player's thrown (the throw's own code, as it leaves
// the hand: saber 0 the right, 1 the left; pSaber the thing flying; shown,
// the saber whose look it had in the hand: its own, or with one saber, the
// one saber's, 0, whichever hand's it is)
void iosSaberFold_Released(int saber, struct SithThing* pSaber, int shown);
// A new level or a load (jkDev, with the throws' reset): every saber in hand,
// folded with AUTO on (as the player left it with AUTO off)
void iosSaberFold_Reset(void);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERFOLD_H
