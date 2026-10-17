// Added: entire file. The player's lightsaber as they choose it: its style
// (one saber or two), its hilt (the game's own, or one with a crossguard) and
// each blade's colour, picked on the touch overlay's SABER wheel.
//
// Only how it looks changes, for the local player (and, in multiplayer, for
// each other player as their game sends it: jkPlayerLook): the game's own
// saber (its materials, length, damage, saves, what the game itself sends
// other players) is left as the game set it. Each blade's colour is put on
// just while it is drawn (iosSaber_BeginBlade/EndBlade, round the blade's
// draws in jkPlayer.c and jkSaber.c), so it shows wherever the blade is drawn:
// first and third person, selfies, Action Freeze.
//
// The colours: the game's own (no change), the seven the game has materials
// for (saber<colour>0.mat, the tip; saber<colour>1.mat, the side -- in Jedi
// Knight and Mysteries of the Sith alike), and more made from the game's blue
// blade: its pattern of light kept texel for texel (the white middle, the
// colour out to the edges), only the colour changed, and drawn from a
// true-colour copy of its texture (std3D_AddToTextureCache's pTrueColour).
// Made once per level, for the colours in use (2 small textures each).
//
// Two looks, the player's choice (the saber wheel's switch): REFORGED (the
// default), most colours glowing like the film blades; and CLASSIC, every
// colour as first made. It changes how every blade is drawn here, the
// player's and other players' alike. Colours keep their keys and ids, and
// nothing new is sent; but what the player sees of their own blade is what
// other players are sent (iosSaber_GetShownColour): in Jedi Knight a green
// kept shows, and is sent, as ORIGINAL while REFORGED is on.

#ifndef _PLATFORM_IOS_IOSSABER_H
#define _PLATFORM_IOS_IOSSABER_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

// ---- colours. Ids are only the order of the list (the wheel's, round from
// the gap): keep a choice by its key, which stays the same.
#define IOSSABER_COLOUR_GAME 0 // the game's own: nothing changed
int iosSaber_NumColours(void);
int iosSaber_ColourOnWheel(int id);         // 1: the wheel offers it in this game and look (all but a few)
const char* iosSaber_ColourName(int id);    // as the wheel shows it ("LIGHT BLUE"), NULL for no such id
const char* iosSaber_ColourKey(int id);     // as it is kept ("lightblue")
int iosSaber_ColourByKey(const char* key);  // -1 if none
// The colour to show it in (a swatch on the wheel): its blade's edge colour
// (the game's own: the game's colour the player's blade has now, if it is one
// of them; else grey), in the look
void iosSaber_ColourRGB(int id, unsigned char* pR, unsigned char* pG, unsigned char* pB);

// ---- the look (see above), remembered by the platform by its name
enum { IOSSABER_LOOK_CLASSIC = 0, IOSSABER_LOOK_REFORGED, IOSSABER_NUM_LOOKS };
int iosSaber_GetLook(void);
void iosSaber_SetLook(int look);
void iosSaber_ToggleLook(void);            // the other one
const char* iosSaber_LookName(int look);    // "CLASSIC", "REFORGED": as the switch shows it and it is kept
int iosSaber_LookByName(const char* name);  // -1 if none

// ---- styles: how many sabers. Only the ones that work can be picked
// (iosSaber_StyleAvailable); each keeps its own blade colours, so going from
// one style to another and back finds them as they were. Any hilt (below)
// goes with either.
//   SINGLE  the game's one saber
//   DUAL    a saber in each hand
// The numbers are the ones other players' games are sent (jkPlayerLook);
// builds before the hilts had CROSS as a third style (2), which is still
// sent to them for one saber with a crossguard.
enum { IOSSABER_STYLE_SINGLE = 0, IOSSABER_STYLE_DUAL, IOSSABER_NUM_STYLES };
#define IOSSABER_MAX_BLADES 2
int iosSaber_StyleAvailable(int style);
const char* iosSaber_StyleName(int style);  // "SINGLE", "DUAL": as the wheel shows it and it is kept; NULL for no such style
int iosSaber_StyleNumBlades(int style);
// A style as it was kept (its name), or -1. A hilt's name (CROSS, kept as a
// style by builds before the hilts) is SINGLE with that hilt in *pHilt (left
// as it is otherwise; pHilt may be NULL): the settings' move to the hilts
int iosSaber_StyleByName(const char* name, int* pHilt);
int iosSaber_GetStyle(void);
void iosSaber_SetStyle(int style);
int iosSaber_GetColour(int style, int blade);  // as picked and kept
void iosSaber_SetColour(int style, int blade, int id);
// As it is shown (drawn, thrown, sent to other players): one this game's
// wheel lacks in the look is the game's own (REFORGED, Jedi Knight: a green
// kept is ORIGINAL, the game's green; CLASSIC shows it green again)
int iosSaber_GetShownColour(int style, int blade);

// ---- hilts: what the player's sabers are, both hands alike, with either
// style (the saber wheel's HILT, a tap for the next). Each blade's shape and
// the hilt's own parts (iosSaberShape), how it is carried (iosSaberStance)
// and how it sounds (iosSaberSound) go by it.
//   NORMAL  the game's own: its blade the shape switch's (SPIKE or FILM),
//           carried, swung and heard as the game has it
//   CROSS   a crossguard: the blade jagged, crackling and pointed, two short
//           side blades out of vents on the hilt, all in the blade's colour;
//           carried low, in Kylo Ren's guard; its hum rougher, lower and
//           crackling. Only how it looks, is carried and sounds: the blade
//           strikes as the game's own, the side blades not at all.
//   STAFF   Maul's double-bladed staff (not here yet: not available)
//   FOLD    Dark Rey's folding double saber (not here yet: not available)
// HILT goes round them in this order, past those not available. The numbers
// are the ones other players' games are sent (jkPlayerLook, two bits: four
// at most); one a game doesn't have it draws as NORMAL.
enum { IOSSABER_HILT_NORMAL = 0, IOSSABER_HILT_CROSS, IOSSABER_HILT_STAFF, IOSSABER_HILT_FOLD, IOSSABER_NUM_HILTS };
int iosSaber_HiltAvailable(int hilt);       // 1: it can be picked (and is drawn as itself for other players)
const char* iosSaber_HiltName(int hilt);    // "NORMAL", "CROSS", "STAFF", "FOLD": as HILT shows it and it is kept; NULL for none
int iosSaber_HiltByName(const char* name);  // -1 if none (available or not)
// Whether its blades keep a shape of their own whatever the shape's switch
// says (CROSS): the switch is dimmed on the wheel while it is picked
int iosSaber_HiltKeepsShape(int hilt);
int iosSaber_GetHilt(void);                 // the local player's
void iosSaber_SetHilt(int hilt);            // (only one available)
// The one HILT goes to next from this one: the next available, round again
// (itself if no other is)
int iosSaber_NextHilt(int hilt);
// The hilt in a thing's hand (0: right, 1: left) as it is drawn: the player's
// hilt in each hand the style has a saber in; NORMAL for a hand it hasn't,
// and for anyone whose saber isn't a player's choice (the local player's,
// or in multiplayer another player's as their game sent it)
struct SithThing;
int iosSaber_HandHilt(struct SithThing* pThing, int hand);

// ---- the engine's side.
// Round the drawing of one of a thing's blades (blade 0: the right hand's, 1:
// the left's): its colour put on, then the game's put back. Nothing changes
// for anyone but the local player (and, in multiplayer, the other players
// whose games sent their choice), or for the game's own colour.
// Its shape, spike or beam (or the crossguard's, the CROSS hilt), is put on here too
// (iosSaberShape), whatever the colour.
struct SithThing;
struct rdThing;
void iosSaber_BeginBlade(struct SithThing* pThing, struct rdThing* pLine, int blade);
void iosSaber_EndBlade(struct rdThing* pLine);

// Whether this thing carries a player's second blade (the local player with a
// two-blade style, not in a selfie, both in hand; in multiplayer, another
// player whose game sent a two-blade style): a saber in the left hand too, drawn
// with its own hilt, and striking as the game's own two-saber mode does
// (jkSaber_IsDual: JKFLAG_DUALSABERS, which the game itself never sets)
int iosSaber_HasSecondBlade(struct SithThing* pThing);

// Second, the two-saber style's left arm (rdPuppet_BuildJointMatrices, each
// joint posed and not yet put together; each two-saber player's, up to eight
// at once): the player's left arm takes the
// right arm's pose of IOSSABER_ECHO_MS before, mirrored, so the two sabers
// are held apart and swing one after the other, the second blade striking
// where it is drawn. (The game's own stance holds the hilt in both hands.)
// Only while the saber is on.
#define IOSSABER_ECHO_MS 200
void iosSaber_PoseArms(struct rdThing* pRdThing);

// The level going (sithWorld_FreeEntry): the colours made for it freed
// (they are made again, from the next level's palette, when next drawn)
void iosSaber_FreeWorld(void);

// A thing drawn (sithRender_RenderThing): a saber the player threw, in its
// colour (iosSaberThrow_ThrownColour) just while it is drawn, between these
// A player's saber hilt just drawn (jkPlayer: first person the hand, the point
// of view model; third person the weapon mesh; pHilt that model, pOrient the
// blade's frame there): with the CROSS hilt, its vents (iosSaberShape_DrawVents), the
// blade lit or not, while the model is the saber's
struct rdMatrix34;
struct rdModel3;
void iosSaber_DrawHilt(struct SithThing* pThing, struct rdThing* pLine, struct rdMatrix34* pOrient, const struct rdModel3* pHilt);
// ...the same for either hand's hilt (0: the right, as above; 1: the left, the
// second saber's)
void iosSaber_DrawHandHilt(struct SithThing* pThing, struct rdThing* pLine, struct rdMatrix34* pOrient, const struct rdModel3* pHilt, int hand);

void iosSaber_BeginThrown(struct SithThing* pThing);
void iosSaber_EndThrown(struct SithThing* pThing);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _PLATFORM_IOS_IOSSABER_H
