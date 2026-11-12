#ifndef _ENHANCED_IOSITEMICONS_H
#define _ENHANCED_IOSITEMICONS_H

// Added: entire file. The touch overlay's LIGHT, IR and BACTA as pictures: the
// game's own 24 x 24 inventory icons (ui\bm\icfield16.bm, icirgog16.bm,
// icbacta16.bm: Jedi Knight's Res1hi.gob, Mysteries of the Sith's JKMsndLO.goo,
// the same three), read from the player's own game files through the game's
// file system -- never shipped with the app -- and made big and smooth for a
// phone's screen once, when the overlay first wants them (off the game's
// thread: iosItemIcons_ReadFiles there, iosItemIcons_PrepareFromSized
// anywhere), at the screen's own pixels:
//   - the item alone: its purple tile cut away, with a faint dark edge round
//     it so it reads on a bright wall;
//   - upscaled with Hyllian's xBR-lv2 (MIT licence: iosItemIcons.c), then an
//     edge-keeping smooth (a bilateral filter) and a light sharpen;
//   - LIGHT off (dimmed) and on (lit, a glow from its bright core), IR off
//     (dimmed) and on (its two lens cutouts lit red-pink, brighter in the
//     middle, a little bloom), BACTA with liquid in its glass for the tanks
//     carried and a glow rising through it while held (iosItemIcons_Bacta).
// Plain C (the overlay makes its pictures from these pixels). With anything
// missing or not as expected, nothing: the overlay keeps its text buttons.

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

#define IOSITEMICONS_GRID 24 // (the originals' pixels across: the pictures cover this many grid units)

enum {
    IOSITEMICON_LIGHT_OFF = 0,
    IOSITEMICON_LIGHT_ON,
    IOSITEMICON_IR_OFF,
    IOSITEMICON_IR_ON,
    IOSITEMICON_NUM_STILL
};

// The three icons' files read through the game's file system (on the game's
// own thread: it is the game's), each into its aBuf: 1 if all three were
#define IOSITEMICONS_FILE_MAX 4096
int iosItemIcons_ReadFiles(unsigned char aBuf[3][IOSITEMICONS_FILE_MAX], int aLen[3]);
// The pictures made from the three files' bytes, s pixels for each of the
// originals' (1..16), then shrunk to outPx across (the screen's own pixels;
// 0 or more than 24 s: 24 s): 1 if done (then or before, at that size), 0 if
// any icon isn't one, or isn't the game's own as this was made for (a mod's:
// the text buttons stay). It takes some tens of milliseconds and touches
// nothing of the game's: it can run on another thread, while nothing else
// here is used.
int iosItemIcons_PrepareFromSized(const unsigned char* const aBm[3], const int aLen[3], int s, int outPx);
int iosItemIcons_PrepareFrom(const unsigned char* const aBm[3], const int aLen[3], int s); // (outPx 24 s)
// Both, at once (the Linux rig's)
int iosItemIcons_Prepare(int s);
int iosItemIcons_IsReady(void);
void iosItemIcons_Free(void);

// The pictures' size in pixels (square), 0 if not ready
int iosItemIcons_Size(void);
// One of LIGHT's or IR's pictures: premultiplied RGBA, Size x Size, rows top
// down; NULL if not ready
const unsigned char* iosItemIcons_Still(int which);
// BACTA drawn into pOut (Size x Size premultiplied RGBA): the liquid in its
// glass at level (0..1: the tanks carried out of 5; 0 empty) glowing from
// the bottom up as glow goes 0..1 (the hold). 0 if not ready.
int iosItemIcons_Bacta(float level, float glow, unsigned char* pOut);

// Where each item's picture sits: the grid point (in the originals' pixels)
// put at the middle of its button
enum { IOSITEMICONS_LIGHT = 0, IOSITEMICONS_IR, IOSITEMICONS_BACTA };
void iosItemIcons_Centre(int item, float* pX, float* pY);

// The game's HUD shows a small tile for each item switched on, down its top
// right corner (jkHudInv): the field light's and the IR goggles' among them.
// While the touch overlay has these pictures up it says so here (on, each
// frame it is shown), and its LIGHT and IR, lit, stand in for those two
// tiles wherever the HUD would show them (iosTouch: as buttons, or with the
// touch controls hidden as pictures alone, to see); the HUD then leaves just
// those two out (the rest of its column as ever: Force powers, keys). Off --
// the overlay's text buttons, no overlay, any other platform -- the game's
// own tiles.
#define IOSITEMICONS_BIN_IRGOGGLES 41  // (SITHBIN_IRGOGGLES, in both games)
#define IOSITEMICONS_BIN_FIELDLIGHT 42 // (SITHBIN_FIELDLIGHT)
void iosItemIcons_SetHudStandIn(int bOn);
int iosItemIcons_HudLeavesOut(int bin); // (1: jkHudInv draws no tile for this bin)
// CAMERA's HUD photo is the game's frame alone (iosCapture): LIGHT and IR,
// standing in, are drawn into it as the overlay shows them, so it matches
// the screen. The overlay says, each frame, where each picture is
// (IOSITEMICONS_LIGHT or _IR: its top left and side, in the overlay's
// points, the overlay this size) and how strong (OPACITY); side 0 while
// that one isn't standing in (off, the text buttons, the overlay hidden)
void iosItemIcons_SetStandInPlace(int item, float x, float y, float side, float alpha, float viewW, float viewH);
// ...and iosCapture_AfterHud draws them, lit, into the frame read back
// (w x h RGBA, rows top down), without their soft shadow; nothing if none
// stands in
void iosItemIcons_DrawStandIns(unsigned char* pRgba, int w, int h);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSITEMICONS_H
