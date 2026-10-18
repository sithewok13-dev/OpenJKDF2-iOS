// Added: entire file. Two of iosSaber's hilts, each hand's (iosSaber_HandHilt):
//   STAFF  Darth Maul's double-bladed saber: two mirror-image hilts joined end
//          to end (silver, black grip rings, red buttons, finned emitter
//          shrouds), a blade of the game's length out of each end, exactly
//          opposite, in the hand's colour, shape and look (FILM too)
//   FOLD   Dark Rey's folding double saber: two hilts joined at their pommels
//          by a black hinge block (each silver, red bands, a black pronged
//          emitter), folded side by side or open into a straight staff
//          (iosSaberFold says which, and the snap between)
// Drawn here: the hilts (built in code, the game's own hilt left out), the
// second blade, and how each is carried:
//  - Third person, standing and walking: held turned in the hand (45 degrees
//    round its grip), so the back blade clears the floor; STAFF held in both
//    hands close together while standing still. Swinging, blocking, a hit
//    taken and so on: back to the game's own pose at once (within 0.08 s,
//    before its hits count), the left hand letting go, and back after. FOLD
//    open is held in one hand; folded, as the game's own one saber.
//  - Now and then, well away from a fight, standing still: one spin of the
//    staff round the hand, with a whoosh (one of the game's own swing sounds).
//  - First person: the game's own pose; any blade of these coming within a
//    few centimetres of the eye fades there (drawn see-through, cut short),
//    so the back blade never fills the view. FOLD folded: the two blades side
//    by side across the view, the same distance from the eye.
//  - Thrown (either throw): spinning flat like a buzzsaw, two of the game's
//    thrown sabers end to end (FOLD folded: side by side, snapping open as it
//    flies, iosSaberFold). Drawn only: the throw flies and strikes as ever.
// Only how it looks: every saber strikes as the game's own, its one blade
// where the game has it (the second blade never strikes); two sabers (DUAL)
// are two of them, each in its own hand.

#ifndef _ENHANCED_IOSSABERSTAFF_H
#define _ENHANCED_IOSSABERSTAFF_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

struct SithThing;
struct rdThing;
struct rdModel3;
struct rdMatrix34;

// ---- the hilts
// The hilt of the saber in a thing's hand as it is drawn (0: the right, 1:
// the left): iosSaber_HandHilt's (two sabers with the right one thrown, the
// one in hand the left's), or the test switch's below
int iosSaberStaff_HiltOf(struct SithThing* pThing, int hand);
// The hilt of a thing's saber (0: the right one, 1: the left one), wherever it is
int iosSaberStaff_SaberHilt(struct SithThing* pThing, int saber);
// Which saber (0 the right one, 1 the left) is in a hand now (two sabers with
// the right one thrown: the left one is in the right hand)
int iosSaberStaff_SaberInHand(struct SithThing* pThing, int hand);
// STAFF or FOLD: one of these
int iosSaberStaff_IsDouble(int hilt);
// For testing, until the hilt is the player's choice: the local player's
// sabers' hilt (one saber's, SINGLE; both, DUAL), whatever iosSaber says (-1:
// as iosSaber says)
void iosSaberStaff_SetTestHilt(int hilt);

// ---- drawing (jkPlayer: bPov 1 the first person hand, 0 third person)
// Where a hand's saber is drawn (its blade's frame, as rdPolyline_Draw takes
// it): the hand's joint (pHand) turned as it is held now; 1 if the hand's
// saber is one of these, in hand (pHilt: the model drawn there, the game's
// hand or hilt), else 0 and pOut is pHand
int iosSaberStaff_HeldFrame(struct SithThing* pThing, int hand, int bPov, const struct rdModel3* pHilt,
                            const struct rdMatrix34* pHand, struct rdMatrix34* pOut);
// Its second blade's frame (from the far end, or the free half), from where
// the saber is drawn (pHeld, HeldFrame's); 0 for any other saber
int iosSaberStaff_SecondBlade(struct SithThing* pThing, int hand, int bPov, const struct rdMatrix34* pHeld, struct rdMatrix34* pOut);
// Its hilt, drawn there (pLine: the thing's blade, whose texture mode its
// faces are drawn in), lit or not
void iosSaberStaff_DrawHilt(struct SithThing* pThing, int hand, int bPov, struct rdThing* pLine, const struct rdMatrix34* pHeld);
// First person, round the drawing of the hand (pPov, the point of view model;
// hand 1 the two-saber style's mirrored arm): the game's own hilt in it left
// out, for one of these
void iosSaberStaff_BeginPovHand(struct SithThing* pActor, struct rdThing* pPov, int hand);
void iosSaberStaff_EndPovHand(void);
// First person, a hand's blades (jkSaber_Draw: pHand the hand's joint, blade
// the colour's, iosSaber_BeginBlade's): for one of these, both drawn where it
// is held, each fading near the eye; 0 for any other saber (drawn as ever)
int iosSaberStaff_DrawPovBlades(struct SithThing* pActor, struct rdThing* pLine, const struct rdModel3* pPovModel,
                                const struct rdMatrix34* pHand, int hand, int blade);
// A thing drawn (sithRender_RenderThing, its orient placed): a thrown STAFF
// or FOLD of the local player's, drawn here (as two, spinning flat; *pRet the
// draw's), 1; 0 for anything else (drawn as ever)
int iosSaberStaff_DrawThrown(struct SithThing* pThing, int* pRet);

// ---- the engine's side
// rdPuppet_BuildJointMatrices, the joints posed but not yet put together:
// the left arm's grip on the staff, or let go
void iosSaberStaff_PoseArms(struct rdThing* pRdThing);
// A swing begun (jkSaber_Enable): no spin for a while; FOLD's AUTO
void iosSaberStaff_Swing(struct SithThing* pThing);
// Once a frame (jkDev): the spin, when it is due
void iosSaberStaff_Tick(void);
// The level going (iosSaber_FreeWorld): what was found in its models forgotten
void iosSaberStaff_FreeWorld(void);
// Whether a thing is doing anything but carrying its saber (the third person
// puppet: a swing, a block, a hit taken, a Force power, the saber drawn or put
// away; the game counting a swing's hits), as this module goes by
int iosSaberStaff_IsBusy(struct SithThing* pThing);

// (for checks) The local player's: how far it is held turned (third person,
// 0..1), its left hand's grip (+) or let go (-), the spin's angle now
// (degrees); and in first person the last drawn blades' tips on screen
// (normalised: x right, y up), index 0..3 (hand 0's two, hand 1's two)
float iosSaberStaff_Weight(int what);
int iosSaberStaff_PovTip(int i, float* pX, float* pY, float* pDepth);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERSTAFF_H
