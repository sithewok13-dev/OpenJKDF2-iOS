// Added: entire file. Co-op campaign play (jkCoop): the Force as the story gives it, each player their
// own, as one player has it. Everything here does nothing outside a co-op game (deathmatch keeps the
// host's Max Rank and the multiplayer character's rank and stars).
//
// - The multiplayer character gives only its looks in co-op (its model, sounds and saber; in Mysteries
//   of the Sith its class's ammo too), not its rank, powers or stars: a campaign begins as one player's
//   does (Jedi Knight: no Force until the story gives it, after level 3).
// - What a level's end gives (its rank, its stars, the star for all its secrets, its power) and what is
//   spent on the Force powers screen between levels are kept (jkCoop carries them), and so is the side
//   chosen after Jedi Knight's level 14. The Force carried in is had as the level opens, before its
//   scripts start, as in single player: so what a level does to the Force is what everyone has
//   (Mysteries of the Sith's first Mara level: rank 2, 3 stars), and the Force's energy is full at each
//   level's start.
// - Each player keeps their own side, light or dark; the host's path decides the levels (jkCoop). Where
//   the paths split, a player on the other side than the host's path gets their side's power (Jedi
//   Knight's Sariss: Absorb or Destruction).
// - What the host's level does to the Force as it begins, in a part the game runs on the host only (a
//   timer: Mysteries of the Sith's first Kyle and first Mara levels), is done to everyone's.
// - Secrets count for everyone: the host's count (the game counts everyone's finds there) is everyone's,
//   so each player can earn the star for finding all of a level's secrets; and stars a level gives in a
//   part the game runs on the host only (Jedi Knight's 16aEscapeShip: its all-secrets star, as its end is
//   touched) are given to every player.
// - A player new to the campaign (not in the host's save) catches up: the host's rank, the powers the
//   story has offered the host, and as many stars as the host has earned, to spend on the Force powers
//   screen at the end of that level.
// - A level played with cheats (when the host allows them) still gives what the story gives (its
//   scripts' rank, stars and powers), not what the cheats gave.
// - The innocents killed (a dark side's mark) are counted over the campaign, as in single player; but,
//   as the game counts them on the host for the host, only the host's kills count.

#ifndef _MAIN_JKCOOPFORCE_H
#define _MAIN_JKCOOPFORCE_H

#include "types.h"
#include "Main/jkCoop.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef QOL_IMPROVEMENTS

void jkCoopForce_Startup(void);

// The local player's multiplayer character as a co-op level loads (in place of jkPlayer_MpcInitBins):
// its looks only
void jkCoopForce_MpcInitLooks(SithPlayer* pPlayer);
// The rank to pick a multiplayer character by for a game (jkGuiBuildMulti_ShowLoad): co-op's characters
// give no Force, so any rank (-1), else the host's Max Rank
int jkCoopForce_CharacterRank(int maxRank, int multiModeFlags);
// Mysteries of the Sith's jkSyncForcePowers by the multiplayer classes' rules (not in co-op: the story's)
int jkCoopForce_MultiRules(void);
// A cog asks IsMulti() (sithCogFunction_IsMulti): the answer in co-op, if not the game's own (-1). The
// Force's energy in Mysteries of the Sith (the rank's cog, force_well.cog) is filled as in single player,
// up to the rank's as it is now: by its multiplayer rule its most is the rank's as the level starts, none
// if the level gives the rank as it begins (the story's first Kyle level)
int jkCoopForce_CogIsMulti(sithCog* pCog);

// A co-op level opened, the Force carried in given (jkCoop_LevelOpened)
void jkCoopForce_LevelOpened(SithThing* pMe);
// Once a frame in a co-op level (jkCoop_Tick)
void jkCoopForce_Tick(SithThing* pMe);
// One of the level's own scripts (pCog) about to set one of the local player's Force bins
// (jkCoop_CogSetsInv: SetInv: amount; SetInvAvailable: bAvailable, else -1; jkCoop_CogChangesInv: ChangeInv,
// bChange: amount, what it will then have)
void jkCoopForce_LevelSetsForce(sithCog* pCog, SithThing* pMe, int bin, flex_t amount, int bAvailable, int bChange);
// The kit just taken in a level (jkCoop): a level played with cheats keeps the story's Force in it
void jkCoopForce_CleanKit(jkCoopKitBin aKit[SITHBIN_NUMBINS]);
// This level's starting kit about to be taken (jkCoop_Tick)
void jkCoopForce_StartKitDue(void);
// A kit from the host given in the level (jkCoop_SetKit)
void jkCoopForce_KitFromHost(SithThing* pMe);

// The level's end (jkMain_EndLevel, its Force power given): where the paths split, the other side's too
void jkCoopForce_LevelEnding(void);
// The level ended, in it (jkCoop_LevelEnded)
void jkCoopForce_LevelEnded(void);
// The level just closed (jkMain_GameplayLeave, its scripts' end run), and again after the screens
// between levels (jkCoop_LevelEnded): the Force as it is now kept with the kit
void jkCoopForce_LevelClosed(void);
void jkCoopForce_BetweenLevels(void);
// The host about to end the level for everyone (jkDSS_SendEndLevel): its secrets sent first
void jkCoopForce_HostEndingLevel(void);

// The host, a guest new to the campaign (in no save, nothing carried in): their kit caught up with the
// host's Force (1 if so: aKit then to be sent back to them)
int jkCoopForce_CatchUp(jkCoopKitBin aKit[SITHBIN_NUMBINS]);
// The host, a guest's kit come (jkCoopSave, after any kit sent back): what the host's level did to the
// Force as it began, and its secrets so far, told to that guest
void jkCoopForce_TellGuest(int idTo);

void jkCoopForce_SessionEnded(void);

#endif // QOL_IMPROVEMENTS

#ifdef __cplusplus
}
#endif

#endif // _MAIN_JKCOOPFORCE_H
