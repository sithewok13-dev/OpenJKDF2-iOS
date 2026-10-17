// Added: entire file. The low guard: how a crossguard saber (iosSaber's CROSS
// hilt) is carried, one or two of them, first and third person, Kyle's and
// Mara's alike: Kylo Ren's low, aggressive carry from the films. While
// standing, walking, running, crouching or jumping (not attacking), each
// blade is held low and angled out to its side, the hand by the hip; with two
// sabers the hands further apart. The free hand (one saber) lets go of the
// hilt the game holds in both hands, and hangs by the side.
//
// Only how it looks. Third person, each arm's joints are turned (shoulder,
// elbow, wrist) once the game has posed the body, toward the guard's
// directions in the body's own frame (so it fits any person's model); first
// person, the arm in view is turned round its hand and moved down and out.
// Either way it blends out quickly as a swing, a block or anything else the
// game plays on the arms begins, so the swing is exactly the game's own (and,
// with two sabers, the left arm's the two-saber echo, iosSaber), and back in
// after. The game counts saber hits only while a swing's own marker says so
// (JKFLAG_SABERDAMAGE, after the swing's first sixth of a second): from
// then on nothing here is turned at all, so hits, their places and damage
// are the game's own. Selfies pose the arms themselves (iosSelfie): none here
// then.
//
// For the local player, and in multiplayer for other players whose games
// sent a CROSS style (the style already sent, jkPlayerLook).

#ifndef _ENHANCED_IOSSABERSTANCE_H
#define _ENHANCED_IOSSABERSTANCE_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

struct SithThing;
struct rdThing;
struct rdMatrix34;
struct rdVector3;

// rdPuppet_BuildJointMatrices, once the joints are put together (world): a
// low guard's arms turned, as much as its weight is now (moved on by the game's
// clock, once per moment)
void iosSaberStance_PoseArms(struct rdThing* pRdThing);

// jkPlayer_DrawPov: the first person arm's placement with its low guard put on
// (hand 0: the right arm, pPlace its placement, the hand found in the model
// as it was drawn last; 1: the two-saber style's mirrored left arm, pPivot
// its hand in the arm model's own space). 0: none (pPlace as it was).
int iosSaberStance_PovPlace(struct SithThing* pActor, struct rdThing* pPov, int hand, const struct rdVector3* pPivot, struct rdMatrix34* pPlace);

// (for checks) How far into the low guard a thing's hand is now, third person
// (0: the game's own pose, 1: the guard); hand 2 and 3: the local player's in
// first person
float iosSaberStance_Weight(struct SithThing* pThing, int hand);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERSTANCE_H
