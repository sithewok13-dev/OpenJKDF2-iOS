// Added: entire file. The saber wheel's choices, for every input that shows
// the wheel (iOS's touch overlay, iosTouch.m; the touch overlay of Android
// and the desktops, touchUi.c; a pad and the keys, inputWheel.c), so each
// shows the same and does the same. Logic only, no drawing: over iosSaber
// (each hand's saber, the local player's) and iosSaberFold (FOLD's AUTO and
// its flip).
//
// The wheel (hiltmock/DESIGN.md): a card for each hand, on its own side of
// the colour ring (RIGHT always; LEFT with two sabers, a "+ 2ND SABER" slot
// with one). Each card: the hand's hilt with its own HILT button, and its
// blades' colours (a dot each; STAFF and FOLD two, joined while linked).
// Tapping a card picks its whole hand for the ring to paint; tapping a blade's
// dot picks just that blade (a linked pair splits only when its second blade
// is given a colour other than the first's). SAME COLOUR (a colour tap paints
// every blade of both hands) and SAME HILT (a HILT tap changes both hands) sit
// under the LEFT card, and act only with two sabers; FILM under the RIGHT card, dimmed while every hilt in use is
// CROSS; AUTO under FILM while a FOLD is in either hand; RANDOM under where
// AUTO goes (a new hilt and new colours for each saber held; held, new
// colours only).
//
// Hands: 0 the right (IOSSABER_HAND_RIGHT), 1 the left. Blades: 0 a hand's
// first, 1 its second (STAFF's and FOLD's other one).

#ifndef _ENHANCED_IOSSABERWHEEL_H
#define _ENHANCED_IOSSABERWHEEL_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

// ---- one saber or two (iosSaber's style)
int iosSaberWheel_NumSabers(void); // 1 or 2
// "+ 2ND SABER": two sabers, the left hand's as it was kept; the ring then
// paints the left hand
void iosSaberWheel_AddSecond(void);
// The LEFT card's x: one saber (the left hand's kept for next time); the ring
// paints the right hand if it painted the left
void iosSaberWheel_RemoveSecond(void);

// ---- each hand's hilt
int iosSaberWheel_GetHilt(int hand);
// (SAME HILT on: both hands to it; only one available)
void iosSaberWheel_SetHilt(int hand, int hilt);
// A hand's HILT tapped: its next available hilt (NORMAL, CROSS, STAFF, FOLD,
// round again); SAME HILT on: both hands to that. The hand's hilt now.
int iosSaberWheel_CycleHilt(int hand);

// ---- SAME COLOUR and SAME HILT (the switches under the LEFT card; off to
// start: nothing changes for a player who never touches them; remembered).
// On only while two sabers are held: with one, each counts as off (Get says
// 0), its setting kept (GetKept) for when the second saber comes back.
int iosSaberWheel_GetSameColour(void);
void iosSaberWheel_SetSameColour(int bOn);
int iosSaberWheel_GetSameHilt(void);
void iosSaberWheel_SetSameHilt(int bOn);

// ---- what the ring paints
// A tap on a card (outside its HILT): that whole hand (only the right with
// one saber)
void iosSaberWheel_PickHand(int hand);
// A tap on a blade's dot (a STAFF or FOLD card): just that blade, picked only
// (a linked pair stays linked: ApplyColour). A hand with one blade, or SAME
// COLOUR on (iosSaberWheel_BladesPickable 0): the whole hand, as its card.
void iosSaberWheel_PickBlade(int hand, int blade);
int iosSaberWheel_BladesPickable(int hand); // a hand in use with two blades, SAME COLOUR off: its dots pick a blade
int iosSaberWheel_GetPickedHand(void);  // the card outlined in gold
// The dot ringed in gold: 0, 1; -1 for the whole hand (and while SAME COLOUR
// is on: every blade is painted; the blade picked before is back once it is
// off, unless a colour was set meanwhile, which links every pair again). A
// blade picked on a hand whose hilt goes to one blade: its whole hand, for good.
int iosSaberWheel_GetPickedBlade(void);
enum { IOSSABERWHEEL_PAINT_ALL = 0, IOSSABERWHEEL_PAINT_HAND, IOSSABERWHEEL_PAINT_BLADE };
int iosSaberWheel_PaintKind(void);      // ALL (SAME COLOUR on), the picked HAND, or one BLADE
// A colour tapped on the ring (iosSaber's id): SAME COLOUR on, every blade of
// both hands; a whole hand, both its blades (linked again); a blade, that one
// (a linked first blade: the second follows; a linked second blade: split
// from the first, with that colour, if it isn't the first's as shown)
void iosSaberWheel_ApplyColour(int id);
// The colour the hub names and the ring shows as picked (as shown): the
// picked blade's, or the picked hand's first blade's
int iosSaberWheel_PaintColour(void);
// The hub's line under the colour's name: what the ring paints ("colour for
// both sabers", "colour for the left saber", "right saber, blade 2"; one
// saber's whole hand: bTapMode "tap a colour", else "slide toward a colour")
const char* iosSaberWheel_HubText(int bTapMode);

// ---- a hand's card
typedef struct {
    int bShown;      // RIGHT always; LEFT with two sabers
    int hilt;        // (iosSaber_HiltName for HILT's name)
    int numBlades;   // its dots: 1, or 2 (STAFF, FOLD)
    int bLinked;     // two dots: joined, one name (linked); or each with its own
    int aColour[2];  // each blade's colour as shown ([1] is [0] with one blade, or linked)
    int bPicked;     // the gold outline: the ring paints this hand (or one of its blades)
    int pickedBlade; // the gold ring round a dot: 0, 1; -1 none (the whole hand, not picked, or SAME COLOUR on)
} iosSaberWheelCard;
void iosSaberWheel_GetCard(int hand, iosSaberWheelCard* pOut);

// ---- RANDOM (under the RIGHT card's switches, where AUTO's go below it;
// a pad's View, the keys' R)
// Each saber held (one or two) a new look: a hilt other than its own (through
// iosSaberWheel_SetHilt, as its HILT: a new STAFF or FOLD joined) and
// colours other than its own as shown -- or (bColoursOnly: RANDOM held) the
// colours only, the hilts kept. SAME HILT on: both hands one new hilt
// (neither's own); SAME COLOUR on: every blade one new colour (no blade's
// own), each pair linked again. A STAFF or FOLD's joined dots one colour,
// split dots two (neither its own, nor each other's). A colour "its own"
// also where it would look the same (ORIGINAL and the game's own blade's
// colour: the same swatch). Colours only from this game's wheel in this
// look (iosSaber_ColourOnWheel), hilts only those available. Nothing else
// changes: how many sabers, the look, FILM, AUTO, THROW; the pick as it can
// stand. 1 if anything changed.
int iosSaberWheel_Random(int bColoursOnly);
// RANDOM's own dice (never the C library's rand(), which the game has):
// seeded from the clock at its first throw, unless seeded here first (the
// tests: the same seed, the same throws)
void iosSaberWheel_SeedRandom(unsigned int seed);

// ---- FILM and AUTO
int iosSaberWheel_FilmDimmed(void); // every hilt in use is CROSS (its blades keep their own shape)
int iosSaberWheel_ShowAuto(void);   // a FOLD in either hand in use: AUTO is shown
int iosSaberWheel_GetAuto(void);    // (iosSaberFold's, one for both hands; remembered)
void iosSaberWheel_SetAuto(int bOn);

// ---- the SABER button (top row)
// Its picture: each saber in its hilt's shape, each blade in its colour, two
// in an X (the left hand's underneath). Compare with the last one drawn to
// know when to draw it again.
typedef struct {
    int numSabers;
    struct {
        int hilt;
        int bOpen;      // FOLD: open (a staff with a hinge), else folded (two blades side by side)
        int aColour[2]; // each blade's as shown (iosSaber_ColourRGB)
    } aHand[2];          // [0] the right, [1] the left (with two)
} iosSaberWheelIcon;
void iosSaberWheel_GetIcon(iosSaberWheelIcon* pOut);
// With a FOLD lit in hand, a tap on SABER flips it (a slide or a 0.3 s hold
// opens the wheel); else any touch opens the wheel, as ever
int iosSaberWheel_TapFlips(void);
int iosSaberWheel_Flip(void); // (1 if it flipped: the click comes with the snap)

// ---- how the platforms keep it (each its own store: iOS NSUserDefaults,
// "iosTouchSaber" + name; the desktops' cvars, "enh_saber" + name; Android's
// settings file, "saber" + name). Every choice above as text by its name:
//   Style                        SINGLE or DUAL
//   HiltRight, HiltLeft          NORMAL, CROSS, STAFF, FOLD
//   ColourRight1, ColourRight2,  a colour's key ("red"; "game" the game's own),
//   ColourLeft1, ColourLeft2     each blade's
//   LinkRight, LinkLeft          1: the second blade follows the first
//   SameColour, SameHilt         0 or 1
//   FoldAuto                     1 (to start) or 0
//   FoldOpen                     with AUTO off, how FOLD was left (1 open)
// Saved when one differs from what was last saved (a pad or the keys change
// them too: compare often, as GetKept gives them).
#define IOSSABERWHEEL_KEPT_LEN 32
int iosSaberWheel_NumKept(void);
const char* iosSaberWheel_KeptName(int i);
void iosSaberWheel_GetKept(int i, char* pOut, int len);
// Loading: everything to its start, then each name kept with its value, then
// the end. Names that builds before kept are taken too, and moved, for any
// choice not kept under its own name: Hilt (both hands'), Style CROSS (one
// saber with the right hand's CROSS), each style's colours (Colour.SINGLE.0,
// Colour.DUAL.0, Colour.DUAL.1, Colour.CROSS.0; the cvars' ColourSingle,
// ColourDualRight, ColourDualLeft, ColourCross): the right hand the colour of
// the style kept (one saber: SINGLE's; two: DUAL's right one), the left
// DUAL's left one, each hand linked. So a player sees what they had.
void iosSaberWheel_BeginLoad(void);
void iosSaberWheel_LoadKept(const char* name, const char* value);
void iosSaberWheel_EndLoad(void);
// The names builds before kept (for a store that asks for each name it reads)
int iosSaberWheel_NumOldKept(void);
const char* iosSaberWheel_OldKeptName(int i);
// One kept choice changed in its store (a cvar typed in the console): taken
// now (one of the names above; 0 if not one)
int iosSaberWheel_SetKept(const char* name, const char* value);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERWHEEL_H
