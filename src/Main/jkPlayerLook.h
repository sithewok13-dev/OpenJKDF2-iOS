// Added: entire file. In multiplayer, each player's saber as they chose it
// (the SABER wheel: one saber or two, each hand's hilt, each blade's colour), so
// the others see it: each game sends its own player's every few seconds
// (jkNetExt, JKNETEXT_LOOK), and keeps the others' as they come. Colours go
// by their keys ("lightblue"), not their places in a list, so builds with
// other lists still agree; one this game doesn't have is the game's own.
//
// Every build with the enhanced gameplay (JK_ENHANCED: iOS, and the desktops
// by default) sends its player's choice, however it was made (the iOS
// wheel, the enh_saber* cvars). A build without it sends nothing, and the
// others draw its player's saber as the game does.

#ifndef _MAIN_JKPLAYERLOOK_H
#define _MAIN_JKPLAYERLOOK_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef QOL_IMPROVEMENTS

#define JKPLAYERLOOK_KEY_LEN 16
#define JKPLAYERLOOK_MAX_BLADES 2  // (hands)
#define JKPLAYERLOOK_HAND_BLADES 2 // (each hand's blades: STAFF and FOLD have two)
#define JKPLAYERLOOK_SEND_MS (3000)  // (each game's own, this often)
#define JKPLAYERLOOK_KEEP_MS (10000) // (one not heard from this long: forgotten)
#define JKPLAYERLOOK_MIN_MS (250)    // (and never more often than this: a FOLD flipped and flipped back)

// The message (JKNETEXT_LOOK), every build since the first sends at least
// the first 34 bytes; a longer one is fine for every build (each reads what
// it knows, and only takes one shorter than that as broken):
//   [0]       JKPLAYERLOOK_VERSION (1)
//   [1]       the style as builds before the hilts know it: 0 one saber, 1 two,
//             2 one saber with a crossguard (CROSS, a style there); one a game
//             doesn't know it draws as one saber
//   [2..17]   the right hand's (first) blade's colour key, [18..33] the
//             left's ("" the game's own)
//   [34]      the flags (builds with the hilts; older builds don't send it):
//             bits 0-1 the right hand's hilt, bits 2-3 the left's (iosSaber's
//             IOSSABER_HILT_*: 0 NORMAL, 1 CROSS, 2 STAFF, 3 FOLD); bit 4, bit
//             5: the right, the left FOLD open (where it is going); bit 6, bit
//             7: the right, the left hand's second blade a colour of its own
//             (else it shows the first's)
//   [35..50]  the right hand's second blade's colour key, [51..66] the
//             left's (STAFF's and FOLD's other blade: as it is shown, the
//             first's while linked)
// A game that has the flags goes by them: [1] is then only one saber or two.
// (A build of the hilts before the hands had their own sent 35 bytes, the
// flags' bits 0-1 both hands' hilt: read so.)
#define JKPLAYERLOOK_FLAG_FOLD_OPEN(hand) (0x10 << (hand))
#define JKPLAYERLOOK_FLAG_SPLIT(hand) (0x40 << (hand))

typedef struct jkPlayerLook
{
    int style;                                                   // (the wheel's, iosSaber's: 0 one saber, 1 two; one a game
                                                                 // doesn't know it draws as one saber)
    int aHilt[JKPLAYERLOOK_MAX_BLADES];                          // (iosSaber's, each hand's: 0 NORMAL, the game's own. From a
                                                                 // build before the hilts: CROSS for its CROSS, else NORMAL)
    char aColourKey[JKPLAYERLOOK_MAX_BLADES][JKPLAYERLOOK_HAND_BLADES][JKPLAYERLOOK_KEY_LEN]; // ([hand][blade]; "" the game's own)
    int flags;                                                   // (JKPLAYERLOOK_FLAG_*; 0 from older builds)
} jkPlayerLook;

void jkPlayerLook_Startup(void);
// Once a frame in a level
void jkPlayerLook_Tick(void);
// Another player's look (a player thing of someone else's game), NULL if none
// known: drawn as the game does
const jkPlayerLook* jkPlayerLook_ForThing(SithThing* pThing);

// This game's own player's look, from whoever offers one (iosSaber); 0 if none
int jkPlayerLook_GetLocal(jkPlayerLook* pOut);

#endif // QOL_IMPROVEMENTS

#ifdef __cplusplus
}
#endif

#endif // _MAIN_JKPLAYERLOOK_H
