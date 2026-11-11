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

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSITEMICONS_H
