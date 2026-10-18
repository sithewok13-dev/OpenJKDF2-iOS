// Added: entire file. The Force, weapon and saber wheels, in plain C: what
// is on them, which slice a direction points into, what picking one does,
// and how they look -- drawn into the touch overlay's draw list
// (Platform/Touch/touchUiDraw.h), so the game's GL draws them
// (touchUiRender_gl.c) on the desktops and Android. No SDL, GL or platform
// code: the game through iosGame.c and iosSaber.c (src/Enhanced), a tick in
// the hand through platServices.
//
// It is the iOS touch overlay's wheel (iosTouch.m: the maps, the slices'
// hysteresis, what a pick does, the looks, the saber wheel's choices) and
// the Android touch overlay's (touchUi.c, which drew the first two in GL),
// made one, for every input that opens a wheel:
//   - a game controller and a keyboard and mouse (iosPad.c, iosKbMouse.c),
//     through inputOverlay.c (inputUi.h) everywhere but iOS;
//   - fingers (touchUi.c, Android): the maps, the angles, the pick and the
//     drawing from here, the finger's own gestures (slide, hold, tap) kept
//     there, each finger's slice handed in with inputWheel_Point.
// One wheel is open at a time, and the game holds still while it is
// (iosGame_SetHold: single player).
//
// Every power has a fixed slice of the Force wheel, learned or not (those
// are dimmed); every weapon its slot on the weapon wheel, in the number key
// order, with its ammo count; every blade colour its slice on the saber
// wheel, with a card for each hand beside it (its hilt, its blades'
// colours; iosSaberWheel.h) and the saber's other choices. The gap at the bottom cancels. A learned power's level shows as
// four stars, filled one per level.
//
// Units: points (y down) for drawing; angles in degrees counter-clockwise
// from pointing right (90 is up), as the slices are.

#ifndef _PLATFORM_INPUT_INPUTWHEEL_H
#define _PLATFORM_INPUT_INPUTWHEEL_H

#include "Platform/Touch/touchUiDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

// The wheels (as iosPad.h's IOSPAD_WHEEL_* and the touch overlays number them)
enum { INPUTWHEEL_FORCE = 0, INPUTWHEEL_WEAPONS, INPUTWHEEL_SABER, INPUTWHEEL_NUM_KINDS };

// The "slice" of the gap at the bottom (pointing there cancels)
#define INPUTWHEEL_CANCEL (-2)
#define INPUTWHEEL_MAX 17
#define INPUTWHEEL_MAX_GROUPS 5

typedef struct {
    int bin;   // a Force power's or weapon's bin (SITHBIN_*), or a blade colour (iosSaber)
    int group; // its colour group
    float deg; // its middle
} inputWheelSlice;

typedef struct {
    const char* title; // shown beside the ring, or NULL
    float titleDeg;    // about where
    unsigned char r, g, b;
} inputWheelGroup;

typedef struct {
    const inputWheelSlice* aSlices;
    int numSlices;
    const inputWheelGroup* aGroups;
    int numGroups;
    float sliceDeg; // each slice's width
    float gapDeg;   // between two groups (the gap at the bottom is what is left)
    float nameMin;  // the smallest a name goes to keep its row (its stars) at their size; 0: 7 pt, as alone
} inputWheelMap;

// The saber wheel's choices (hiltmock/DESIGN.md; what each does is
// iosSaberWheel's, shared with iOS): a card for each hand on its own side of
// the ring (the RIGHT card right of it; the LEFT card left of it with two
// sabers, a dashed "+ 2ND SABER" slot there with one). Each card: the hand's
// name, its blades' colours (a dot each; STAFF and FOLD two, joined while
// linked), its own HILT button ("HILT STAFF >": that hand's next hilt). A tap
// on a card picks its whole hand for the ring to paint; on a blade's dot (two
// blades) just that blade. The LEFT card's x: back to one saber. Under the
// LEFT card SAME COLOUR and SAME HILT; under the RIGHT card FILM (the blade's
// shape: dim while every hilt in use is CROSS, iosSaberWheel_FilmDimmed) and,
// while a FOLD is in either hand, FOLD's AUTO. In the middle the colours' look
// (CLASSIC / REFORGED) and EQUIP (the saber in hand). In Jedi Knight, single
// player, THROW at the ring's top right (Saber Throw on the Force wheel,
// iosSaberThrowJk), where it always was.
// Per hand (0 the right, 1 the left: IOSSABER_HAND_*): INPUTWHEEL_CHIP_CARD +
// hand, INPUTWHEEL_CHIP_HILT + hand, INPUTWHEEL_CHIP_DOT + 2 * hand + blade.
enum {
    INPUTWHEEL_CHIP_CARD = 0,                      // ...the right's, then the left's
    INPUTWHEEL_CHIP_HILT = 2,                      // ...the right's, then the left's
    INPUTWHEEL_CHIP_DOT = 4,                       // ...the right's two, then the left's two
    INPUTWHEEL_CHIP_ADD = 8,                       // "+ 2ND SABER"
    INPUTWHEEL_CHIP_REMOVE,                        // the LEFT card's x
    INPUTWHEEL_CHIP_SAME_COLOUR, INPUTWHEEL_CHIP_SAME_HILT,
    INPUTWHEEL_CHIP_EQUIP, INPUTWHEEL_CHIP_LOOK,
    INPUTWHEEL_CHIP_SHAPE,                         // FILM
    INPUTWHEEL_CHIP_AUTO, INPUTWHEEL_CHIP_THROW, INPUTWHEEL_NUM_CHIPS
};

// ------------------------------------------------------------ the maps

// A wheel's map for this game (bMots: Mysteries of the Sith); the saber
// wheel's is made from iosSaber's colours as it is asked for
const inputWheelMap* inputWheel_GetMap(int kind, int bMots);

// a - b, as -180..180
float inputWheel_AngleDiff(float a, float b);

// The slice a direction points into, or INPUTWHEEL_CANCEL in the gap at the
// bottom. The slice already pointed at (cur) holds 4 degrees past its edges;
// a gap between two groups goes to the nearer slice.
int inputWheel_SliceForAngle(const inputWheelMap* m, float deg, int cur);

// ------------------------------------------------------------ the open wheel

// Opens a wheel: every power, weapon or colour in its place, read from the
// game now (learned, owned, ammo, levels), to point at and pick -- by
// letting go of what opened it, or (bTapMode) by a tap, a click or A. The
// game holds still. 0 if one is open already, or the saber wheel is asked
// for with no saber (iosGame_IsSaberButtonShown). (The opener closes the
// typing line first, where its keyboard would cover the wheel.)
int inputWheel_Open(int kind, int bTapMode);
int inputWheel_IsOpen(void);
int inputWheel_GetKind(void);
int inputWheel_IsTapMode(void);
void inputWheel_SetTapMode(int bTapMode);
const inputWheelMap* inputWheel_GetOpenMap(void);

// What is pointed at: a slice, INPUTWHEEL_CANCEL, or -1 (nothing: lifting or
// letting go changes nothing); bNeedle: a short needle at the middle's edge
// shows which way (needleDeg). Each new slice (or the gap) pointed at ticks
// in the hand (platServices_Haptic) and calls the slice hook.
void inputWheel_Point(int slice, int bNeedle, float needleDeg);
// ...from a direction (a stick, the mouse), held a little past a slice's
// edges; bAim 0: nothing
void inputWheel_Aim(int bAim, float deg);
// ...the next slice round (+1 clockwise, -1 the other way)
void inputWheel_Step(int dir);
int inputWheel_GetHot(void);
// With nothing pointed at (-1), whether the middle says lifting cancels
// ("CANCEL", "lift to cancel"): a finger on the wheel off its ring or in
// its hole (touchUi.c). Only how it looks: no tick, nothing picked. Reset
// by inputWheel_Point.
void inputWheel_SetCancelShown(int bShown);
// Whether a slice can be picked: a learned power, a weapon the player has
// and could switch to, any colour
int inputWheel_IsPickable(int slice);

// Closes the wheel, picking slice if it can be picked (the power, the
// weapon -- the game takes it once it runs again --, the colour), else
// nothing. A colour is set and the saber wheel stays open, to pick more
// (the other blade, another colour): returns 1 while it is still open.
// -1 closes the saber wheel too.
int inputWheel_Close(int slice);

// One of the saber wheel's choices (INPUTWHEEL_CHIP_*), as a tap on it
// does: the wheel stays open (to pick more), each with a tick, but EQUIP (the
// saber in hand, as set: the wheel closes; nothing with no saber to take).
// The look: the wheel made again in it. Everything is remembered by the
// platform around this (each store compares iosSaberWheel_GetKept, the look,
// FILM and THROW as they change). Nothing (no tick, 0; the slice pointed at
// kept) for one not to be had now: THROW where it isn't offered, AUTO with
// no FOLD in hand, the LEFT card's with one saber, HILT with one hilt...;
// 1 if it was made (the slice pointed at let go of, but EQUIP's: closed).
int inputWheel_SaberChip(int chip);
// The next card or blade for the ring to paint (a pad's Y, the keys' 2): the
// RIGHT card, its blades (STAFF, FOLD; not with SAME COLOUR on), the LEFT
// card, its blades, round again; only what is shown. Only picked (a linked
// pair stays linked). With a tick, the wheel open.
void inputWheel_SaberPickNext(void);
// Whether EQUIP can take the saber now (the player has it and can switch to it)
int inputWheel_CanEquipSaber(void);

// Called for each new slice pointed at, after the tick (a pad's rumble);
// NULL for none. Set by whoever opens the wheel, kept until it closes.
void inputWheel_SetSliceHook(void (*pfn)(void));
// For an opener whose own buttons stay on the screen around the wheel (the
// touch overlay, touchUi.c): whether a box (points: left, top, right,
// bottom) is clear of them. The group names and THROW go only where it is,
// and never above topClear (points from the top); the saber wheel's cards
// and switches only under topClear (the opener hides its other buttons
// while the saber wheel is open: touchUi.c's action buttons). NULL:
// anywhere on the screen. Set by whoever opens the wheel, kept until it
// closes.
void inputWheel_SetKeepClear(int (*pfn)(float x0, float y0, float x1, float y1), float topClear);
// What the middle says to do, in the opener's words, given what is pointed
// at (bPickable: a slice that can be picked; bCancel: the gap); NULL from it
// keeps the touch wording ("lift to select"). NULL for the touch wording.
void inputWheel_SetHintFn(const char* (*pfn)(int bPickable, int bCancel));

// ------------------------------------------------------------ drawing

typedef struct {
    float w, h;                   // the screen, in points
    float inL, inT, inR, inB;     // the safe area's insets
} inputWheelScreen;

// Lays the open wheel out on a screen without drawing it (drawing does
// this too): where it is, and its slices and choices, for pointing at with
// inputWheel_SliceAt and inputWheel_ChipAt before the first frame is drawn
void inputWheel_Layout(const inputWheelScreen* pScreen);

// The open wheel, over the game dimmed: its slices (the one pointed at
// popped out), each name with its stars or ammo, the needle, the group
// names, the middle's name and hint, "cancel" in the gap; on the saber
// wheel its choices beside it. Nothing if no wheel is open.
void inputWheel_Draw(touchUiDrawList* pList, const inputWheelScreen* pScreen);

// Text for the label font, which is printable ASCII: the few other
// characters the prompts and names use (a PlayStation pad's symbols, the
// D-pad's arrows, dashes) in words, any other as '?'
void inputWheel_Ascii(const char* in, char* out, int outSize);

// Where it was last laid out or drawn (points), for the tests and for pointing with a
// finger or a pointer
void inputWheel_GetGeometry(float* pX, float* pY, float* pR0, float* pR1);
// The slice under a point on the ring (cur as for SliceForAngle),
// INPUTWHEEL_CANCEL in the gap, -1 off the ring
int inputWheel_SliceAt(float x, float y, int cur);
// The saber wheel's choice under a point, or -1
int inputWheel_ChipAt(float x, float y);
// Where one of the saber wheel's choices is (its middle), and whether it is shown
int inputWheel_GetChip(int chip, float* pX, float* pY);
// ...its box (points: left, top, width, height: where a tap takes it, a
// little round it too), and whether it is shown
int inputWheel_GetChipRect(int chip, float* pX, float* pY, float* pW, float* pH);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_INPUT_INPUTWHEEL_H
