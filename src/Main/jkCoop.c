// Added: entire file. See jkCoop.h.

#include "Main/jkCoop.h"

#ifdef QOL_IMPROVEMENTS

#include "jk.h"
#include "stdPlatform.h"
#include "Dss/sithMulti.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithTime.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "Cog/sithCog.h"
#include "World/sithSector.h"
#include "World/sithWorld.h"
#include "Primitives/rdVector.h"
#include "Main/jkEpisode.h"
#include "Main/jkMain.h"
#include "General/stdString.h"
#include "Win95/stdComm.h"
#include "Main/jkDev.h"
#include "Dss/jkDSS.h"
#include "Main/jkCoopSave.h"
#include "Main/jkCoopForce.h"
#include <string.h>

int jkCoop_IsActive(void)
{
    return sithNet_isMulti && (sithNet_MultiModeFlags & MULTIMODEFLAG_COOP);
}

int jkCoop_HostKeepsProtagonist(void)
{
    return jkCoop_IsActive() && sithNet_isServer;
}

// ------------------------------------------------------------------ gear between levels

static char jkCoop_aHostLevel[32]; // (the host's level, below)
static int jkCoop_ownDifficulty = -1; // (a guest's own, while it plays at the host's: below)
static int jkCoop_pickedEntry = -1; // (the level the host picked, below)

// (a kit older than this is from another game: a level, its cutscene and its
// screens take less)
#define JKCOOP_KIT_MAX_AGE_MS (15 * 60 * 1000)
// (in a new level, the rest of it given, and this level's starting kit taken, once the player is in
// and the level's own start done: a guest joins after loading)
#define JKCOOP_KIT_DELAY_MS (2000)

static jkCoopKitBin jkCoop_aKit[SITHBIN_NUMBINS];
static int jkCoop_bKitValid = 0;      // jkCoop_aKit holds a kit
static uint32_t jkCoop_kitMs = 0;     // when it was taken
static int jkCoop_bKitToGive = 0;     // this level: its Force, and the like, still to be given
static int jkCoop_bGearToGive = 0;    // this level: its gear still to be given (not as the level opened)
static int jkCoop_bForceToGive = 0;   // this level: its Force still to be given (not as the level opened)
static int jkCoop_bKitTaken = 0;      // this level: its starting kit taken
static uint32_t jkCoop_levelOpenMs = 0;
static int jkCoop_bKitCameWith = 0;   // this level: began with a kit carried in (not the level's own start)
static int jkCoop_bOpening = 0;       // the kit being given as the level opens (before its scripts start)

// What this level's own scripts took from the local player (the gear only), and, the host's, what they
// took as the level began (as its starting kit was taken), for the guests
static jkCoopTaken jkCoop_aTaken[SITHBIN_NUMBINS];
static jkCoopTaken jkCoop_aStartTaken[SITHBIN_NUMBINS];
static int jkCoop_bStartTaken = 0;

// The local player coming in (jkCoop_SpawnBegin/End): the gear before the scripts' "newplayer", and as
// the class cog's one-player start left it (before its multiplayer spawn kit)
static jkCoopKitBin jkCoop_aSpawnBefore[SITHBIN_NUMBINS];
static jkCoopKitBin jkCoop_aSpawnOwn[SITHBIN_NUMBINS];
static int jkCoop_spawnStage = 0;     // 0 none, 1 coming in, 2 the class cog's own start seen
static int jkCoop_bOpeningSpawn = 0;  // the level's opening: its first one is coming in, not back
static int jkCoop_bSpawnBack = 0;     // (this one: back after dying)
// (weapons that coming back took, kept from the hand for a moment after: a class cog's spawn kit picks
// one again a little later, kyle_m.cog's)
static uint8_t jkCoop_aSpawnTookWeapon[SITHBIN_NUMBINS];
static uint32_t jkCoop_spawnTookMs = 0;
#define JKCOOP_SPAWN_HOLD_MS (1500)

static SithThing* jkCoop_LocalPlayer(void)
{
    SithThing* pMe = sithPlayer_g_pLocalPlayerThing;
    if (!pMe || pMe->type != SITH_THING_PLAYER || !pMe->actorParams.pPlayer)
        return NULL;
    return pMe;
}

// The bins a player carries from one level to the next
#define JKCOOP_BIN_MOTS_WEAPONS_SUPPRESSED 93
static int jkCoop_BinCarried(int bin)
{
    const SithInventoryType* pType = &sithInventory_g_aTypes[bin];
    if (!(pType->flags & SITHINVENTORY_TYPE_REGISTERED))
        return 0;
    if (pType->flags & SITHINVENTORY_TYPE_NOT_CARRIED_BETWEEN_LEVELS)
        return 0;
    // (what each level counts for itself; not the innocents killed, which one player's game counts over
    // the whole campaign)
    if (bin == SITHBIN_SECRETS || bin == SITHBIN_MAXSECRETS)
        return 0;
    // (where the level's goals' text starts: the level's own)
    if (bin == SITHBIN_STRINGOFFS)
        return 0;
    // (Mysteries of the Sith's "weapons are useless" mark: the Sith levels' own, each setting it as it
    // begins, 00_noweapons.cog, on every player's game; carried, a level picked after them would have
    // weapons that don't work)
    if (Main_bMotsCompat && bin == JKCOOP_BIN_MOTS_WEAPONS_SUPPRESSED)
        return 0;
    return 1;
}

// The gear among them: weapons, their ammo, items, shields and the battery (not the Force: its rank,
// powers, stars and mana, which this leaves as it was)
static int jkCoop_BinIsGear(int bin)
{
    if (bin <= 0 || bin >= SITHBIN_NUMBINS || !jkCoop_BinCarried(bin))
        return 0;
    if (sithInventory_g_aTypes[bin].flags & (ITEMINFO_WEAPON | ITEMINFO_ITEM | SITHINVENTORY_TYPE_BACKPACKITEM))
        return 1;
    return bin == SITHBIN_BATTERY || bin == SITHBIN_SHIELDS || (Main_bMotsCompat && bin == SITHBIN_EWEB_ROUNDS);
}

int jkCoop_BinIsForce(int bin)
{
    if (bin <= 0 || bin >= SITHBIN_NUMBINS || !jkCoop_BinCarried(bin) || jkCoop_BinIsGear(bin))
        return 0;
    return bin == SITHBIN_FORCEMANA || bin == SITHBIN_NEW_STARS || bin == SITHBIN_SPEND_STARS || bin == SITHBIN_CHOICE
           || (bin >= SITHBIN_FP_START && bin <= SITHBIN_FP_END) || (bin >= SITHBIN_PEDS_KILLED && bin <= SITHBIN_ALIGNMENT);
}

static int jkCoop_HasWeapon(SithThing* pMe, int bin)
{
    return bin > 0 && bin < SITHBIN_NUMBINS && sithInventory_GetInventory(pMe, bin) > 0.0
           && sithInventory_IsInventoryAvailable(pMe, bin);
}

// A weapon in hand (or on its way) the player no longer has, one of aTook: the game's best pick instead
// (as a saber only a cheat gave goes, jkDev_Custom_TakeCheatSaber)
static void jkCoop_HoldOwnedWeapon(SithThing* pMe, const uint8_t aTook[SITHBIN_NUMBINS])
{
    const int cur = sithInventory_GetCurrentWeapon(pMe);
    const int next = sithWeapon_8BD024;
    if (next > 0 && next < SITHBIN_NUMBINS && aTook[next] && !jkCoop_HasWeapon(pMe, next))
        sithWeapon_8BD024 = -1; // (no switch to it any more)
    else if (next != -1 || cur <= 0 || cur >= SITHBIN_NUMBINS || !aTook[cur] || jkCoop_HasWeapon(pMe, cur))
        return;
    const int other = sithWeapon_AutoSelect(pMe, 1);
    if (other > 0)
        sithWeapon_SelectWeapon(pMe, other, 0); // (not if it is the one in hand)
}

// Jedi Knight's story gives the lightsaber in its level 4 (04EscapeHouse), and from then on the player
// has it (but while 17asarris has it taken); Mysteries of the Sith's players always have theirs. A level's
// number in the episode (as it numbers them), 0 if it isn't one of its levels
#define JKCOOP_STORY_SABER_LEVEL "04escapehouse.jkl"
static int jkCoop_LevelNumber(const char* pFile)
{
    if (!pFile || !jkEpisode_mLoad.paEntries)
        return 0;
    for (int i = 0; i < jkEpisode_mLoad.numSeq; i++) {
        const jkEpisodeEntry* pEnt = &jkEpisode_mLoad.paEntries[i];
        if (pEnt->type == 0 && !__strcmpi(pEnt->fileName, pFile)) // (0: a level)
            return pEnt->level;
    }
    return 0;
}

// (a kit in an episode that isn't that story: as it is)
void jkCoop_StoryKit(jkCoopKitBin aKit[SITHBIN_NUMBINS])
{
    if (Main_bMotsCompat || !sithWorld_g_pCurrentWorld)
        return;
    const int from = jkCoop_LevelNumber(JKCOOP_STORY_SABER_LEVEL);
    const int level = jkCoop_LevelNumber(sithWorld_g_pCurrentWorld->map_jkl_fname);
    if (from <= 0 || level <= 0)
        return;
    jkCoopKitBin* pSaber = &aKit[SITHBIN_LIGHTSABER];
    if (level < from) {
        pSaber->amount = 0.0;
        pSaber->state = 0;
    }
    else if (pSaber->amount < 1.0) {
        pSaber->amount = 1.0;
        pSaber->state |= SITHINVENTORY_ITEM_AVAILABLE;
    }
}

// The player's bins as they are (amounts, and every state flag)
static void jkCoop_Snapshot(SithThing* pMe, jkCoopKitBin aOut[SITHBIN_NUMBINS])
{
    const SithInventoryItem* aItems = pMe->actorParams.pPlayer->aItems;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        aOut[i].amount = aItems[i].amount;
        aOut[i].state = (int32_t)aItems[i].state;
    }
}

// The gear put back as aBack has it: amounts, whether available, a cheat's mark (not whether in use: the
// class cog's start turns the goggles and the light off, as one player's does). The weapons it took
// noted in aTook, if given; 1 if it took any
static int jkCoop_GearBack(SithThing* pMe, const jkCoopKitBin aBack[SITHBIN_NUMBINS], uint8_t* aTook)
{
    SithInventoryItem* aItems = pMe->actorParams.pPlayer->aItems;
    const uint32_t keep = SITHINVENTORY_ITEM_AVAILABLE | SITHINVENTORY_ITEM_CHEAT_GIVEN;
    int bTook = 0;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        if (!jkCoop_BinIsGear(i))
            continue;
        if (aItems[i].amount != aBack[i].amount) {
            const int bWeaponGone = (sithInventory_g_aTypes[i].flags & ITEMINFO_WEAPON) && aBack[i].amount <= 0.0;
            sithInventory_SetInventory(pMe, i, aBack[i].amount);
            if (bWeaponGone) {
                bTook = 1;
                if (aTook) aTook[i] = 1;
            }
        }
        aItems[i].state = (aItems[i].state & ~keep) | ((uint32_t)aBack[i].state & keep);
    }
    return bTook;
}

static void jkCoop_TakeKit(SithPlayer* pPlayer)
{
    SithInventoryItem* aItems = pPlayer->aItems;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        jkCoop_aKit[i].amount = 0.0;
        jkCoop_aKit[i].state = 0;
        if (!jkCoop_BinCarried(i) || (aItems[i].state & SITHINVENTORY_ITEM_CHEAT_GIVEN))
            continue;
        jkCoop_aKit[i].amount = aItems[i].amount;
        jkCoop_aKit[i].state = aItems[i].state & (SITHINVENTORY_ITEM_AVAILABLE | SITHINVENTORY_ITEM_FOUND);
    }
    jkCoopForce_CleanKit(jkCoop_aKit); // (a level played with cheats: the story's Force)
    jkCoop_bKitValid = 1;
    jkCoop_kitMs = stdPlatform_GetTimeMsec();
}

// The kit given (its gear, its Force, its rest, or some: which): nothing less than the level gave (its own starting
// weapons and ammo); what can go below zero (the Jedi's alignment) as it was, but only between levels
// (bLevel), not undoing this level's after dying. The gear this level's scripts took isn't given back
#define JKCOOP_GIVE_GEAR 1
#define JKCOOP_GIVE_REST 2
#define JKCOOP_GIVE_FORCE 4
static void jkCoop_GiveKit(SithThing* pMe, const jkCoopKitBin aKitIn[SITHBIN_NUMBINS], int bLevel, int which)
{
    jkCoopKitBin aKit[SITHBIN_NUMBINS];
    memcpy(aKit, aKitIn, sizeof(aKit));
    jkCoop_StoryKit(aKit);
    SithInventoryItem* aItems = pMe->actorParams.pPlayer->aItems;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        if (!jkCoop_BinCarried(i))
            continue;
        const int bGear = jkCoop_BinIsGear(i);
        if (!(which & (bGear ? JKCOOP_GIVE_GEAR : jkCoop_BinIsForce(i) ? JKCOOP_GIVE_FORCE : JKCOOP_GIVE_REST)))
            continue;
        flex_t amount = aKit[i].amount;
        int32_t state = aKit[i].state & ~JKCOOP_KIT_PEDS_CARRIED;
        const jkCoopTaken* pTaken = &jkCoop_aTaken[i];
        if (bGear && (pTaken->flags & JKCOOP_TAKEN_CAP) && amount > pTaken->cap)
            amount = pTaken->cap;
        if (bGear && ((pTaken->flags & JKCOOP_TAKEN_OFF) || ((pTaken->flags & JKCOOP_TAKEN_CAP) && amount <= 0.0)))
            state &= ~SITHINVENTORY_ITEM_AVAILABLE;
        if (sithInventory_g_aTypes[i].min < 0.0) {
            if (bLevel)
                sithInventory_SetInventory(pMe, i, amount);
        }
        else if (i == SITHBIN_PEDS_TOTAL && bLevel) {
            // (the innocents counted: those carried in, with what this level has counted of its own)
            amount = jkCoopForce_PedsGiven(pMe, amount, (aKit[i].state & JKCOOP_KIT_PEDS_CARRIED) != 0, jkCoop_bOpening);
            if (amount != aItems[i].amount)
                sithInventory_SetInventory(pMe, i, amount);
        }
        else if (amount > aItems[i].amount)
            sithInventory_SetInventory(pMe, i, amount);
        aItems[i].state |= (uint32_t)state;
    }
}

void jkCoop_LevelEnded(void)
{
    jkCoopSave_Flush();
    if (!jkCoop_IsActive())
        return;
    if (!sithPlayer_g_pLocalPlayer) {
        // (the level closed, between levels: the Force as the screens after it left it, what its end gave,
        // the stars just spent, the side chosen)
        jkCoopForce_BetweenLevels();
        return;
    }
    jkCoopForce_LevelEnded();
    if (!jkDev_Custom_CheatedThisLevel() || !jkCoop_bKitValid) {
        jkCoop_TakeKit(sithPlayer_g_pLocalPlayer);
        return;
    }
    // A level played with cheats: the kit it began with kept (its Force then as the story has it, the
    // level's end's included, once the level has closed: jkCoopForce_LevelClosed)
    jkCoop_kitMs = stdPlatform_GetTimeMsec();
}

void jkCoop_SessionEnded(void)
{
    if (jkCoop_ownDifficulty >= 0) {
        jkPlayer_setDiff = jkCoop_ownDifficulty; // (the guest's own back)
        jkCoop_ownDifficulty = -1;
    }
    jkCoopSave_Flush();
    jkCoopForce_SessionEnded();
    jkCoop_pickedEntry = -1;
    jkCoop_bKitValid = 0;
    jkCoop_bKitToGive = 0;
    jkCoop_bGearToGive = 0;
    jkCoop_bForceToGive = 0;
    jkCoop_bOpeningSpawn = 0;
    jkCoop_spawnStage = 0;
    jkCoop_spawnTookMs = 0;
    jkCoop_bStartTaken = 0;
    memset(jkCoop_aTaken, 0, sizeof(jkCoop_aTaken));
    jkCoop_aHostLevel[0] = 0;
}

void jkCoop_LevelOpened(void)
{
    if (!sithNet_isMulti)
        return; // (single player: none of this)
    jkCoop_levelOpenMs = stdPlatform_GetTimeMsec();
    jkCoop_bKitTaken = 0;
    jkCoop_bOpeningSpawn = 1;
    jkCoop_spawnStage = 0;
    jkCoop_spawnTookMs = 0;
    jkCoop_bStartTaken = 0;
    memset(jkCoop_aTaken, 0, sizeof(jkCoop_aTaken));
    memset(jkCoop_aStartTaken, 0, sizeof(jkCoop_aStartTaken));
    if (!jkCoop_IsActive() || (jkCoop_bKitValid && jkCoop_levelOpenMs - jkCoop_kitMs > JKCOOP_KIT_MAX_AGE_MS))
        jkCoop_bKitValid = 0;
    jkCoop_bKitToGive = jkCoop_bKitValid;
    jkCoop_bGearToGive = jkCoop_bKitValid;
    jkCoop_bForceToGive = jkCoop_bKitValid;
    jkCoop_bKitCameWith = jkCoop_bKitValid;
    jkCoopSave_LevelOpened();

    // The gear and the Force carried in (none: a player's first level, or one who joins anew), given
    // before the level's scripts start, which then give and take from them as one player's do (the rest
    // once the player is in: as ever, below)
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!jkCoop_IsActive() || !pMe)
        return;
    jkCoopKitBin aNone[SITHBIN_NUMBINS];
    memset(aNone, 0, sizeof(aNone));
    jkCoop_bOpening = 1;
    jkCoop_GiveKit(pMe, jkCoop_bGearToGive ? jkCoop_aKit : aNone, 1, JKCOOP_GIVE_GEAR);
    if (jkCoop_bForceToGive)
        jkCoop_GiveKit(pMe, jkCoop_aKit, 1, JKCOOP_GIVE_FORCE);
    jkCoop_bOpening = 0;
    jkCoop_bGearToGive = 0;
    jkCoop_bForceToGive = 0;
    jkCoopForce_LevelOpened(pMe);
}

int jkCoop_StartKitTaken(void)
{
    return jkCoop_IsActive() && jkCoop_bKitTaken;
}

void jkCoop_KeepForce(const jkCoopKitBin aForce[SITHBIN_NUMBINS])
{
    if (!jkCoop_bKitValid)
        return;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        if (!jkCoop_BinIsForce(i))
            continue;
        jkCoop_aKit[i].amount = aForce[i].amount;
        jkCoop_aKit[i].state = aForce[i].state & (SITHINVENTORY_ITEM_AVAILABLE | SITHINVENTORY_ITEM_FOUND | JKCOOP_KIT_PEDS_CARRIED);
    }
    jkCoop_kitMs = stdPlatform_GetTimeMsec();
}

int jkCoop_GetKit(jkCoopKitBin aOut[SITHBIN_NUMBINS])
{
    if (!jkCoop_bKitValid)
        return 0;
    memcpy(aOut, jkCoop_aKit, sizeof(jkCoop_aKit));
    return 1;
}

void jkCoop_SetKit(const jkCoopKitBin aKit[SITHBIN_NUMBINS])
{
    memcpy(jkCoop_aKit, aKit, sizeof(jkCoop_aKit));
    jkCoop_bKitValid = 1;
    jkCoop_kitMs = stdPlatform_GetTimeMsec();
    SithThing* pMe = jkCoop_LocalPlayer();
    if (jkCoop_bKitTaken && pMe && jkCoop_IsActive() && !(pMe->flags & SITH_TF_DEAD)) {
        // (already in the level: given now, but not what the level took, and this level's starting kit
        // from it)
        jkCoop_GiveKit(pMe, jkCoop_aKit, 1, JKCOOP_GIVE_GEAR | JKCOOP_GIVE_FORCE | JKCOOP_GIVE_REST);
        jkCoop_TakeKit(pMe->actorParams.pPlayer);
        jkCoop_bKitToGive = 0;
        jkCoop_bGearToGive = 0;
        jkCoop_bForceToGive = 0;
        jkCoopForce_KitFromHost(pMe);
    }
    else {
        jkCoop_bKitToGive = 1;
        jkCoop_bGearToGive = 1;
        jkCoop_bForceToGive = 1;
        jkCoop_bKitCameWith = 1;
    }
}

void jkCoop_Tick(void)
{
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!jkCoop_IsActive() || !pMe)
        return;
    jkCoopForce_Tick(pMe);
    const uint32_t now = stdPlatform_GetTimeMsec();
    if (jkCoop_spawnTookMs) {
        if (now - jkCoop_spawnTookMs > JKCOOP_SPAWN_HOLD_MS)
            jkCoop_spawnTookMs = 0;
        else
            jkCoop_HoldOwnedWeapon(pMe, jkCoop_aSpawnTookWeapon);
    }
    if (now - jkCoop_levelOpenMs < JKCOOP_KIT_DELAY_MS)
        return;

    const int bDead = (pMe->flags & SITH_TF_DEAD) || pMe->actorParams.health <= 0.0;
    if (bDead)
        return;

    if (jkCoop_bKitToGive || jkCoop_bGearToGive || jkCoop_bForceToGive) {
        // (the last level's kit, on top of what this one starts with)
        jkCoop_GiveKit(pMe, jkCoop_aKit, 1, (jkCoop_bKitToGive ? JKCOOP_GIVE_REST : 0) | (jkCoop_bGearToGive ? JKCOOP_GIVE_GEAR : 0)
                                                | (jkCoop_bForceToGive ? JKCOOP_GIVE_FORCE : 0));
        jkCoop_bKitToGive = 0;
        jkCoop_bGearToGive = 0;
        jkCoop_bForceToGive = 0;
    }
    if (!jkCoop_bKitTaken) {
        // (this level's starting kit, for coming back after dying; and for the co-op save. The host:
        // what its level took as it began, for the guests')
        jkCoopForce_StartKitDue();
        jkCoop_TakeKit(pMe->actorParams.pPlayer);
        jkCoop_bKitTaken = 1;
        if (sithNet_isServer) {
            memcpy(jkCoop_aStartTaken, jkCoop_aTaken, sizeof(jkCoop_aStartTaken));
            jkCoop_bStartTaken = 0;
            for (int i = 0; i < SITHBIN_NUMBINS; i++)
                jkCoop_bStartTaken |= jkCoop_aStartTaken[i].flags != 0;
        }
        jkCoopSave_KitTaken(jkCoop_bKitCameWith);
        jkCoop_bKitCameWith = 0;
    }
}

// ------------------------------------------------------------------ coming in, and back after dying

void jkCoop_SpawnBegin(SithThing* pPlayer)
{
    jkCoop_spawnStage = 0;
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!jkCoop_IsActive() || !pMe || pPlayer != pMe)
        return;
    jkCoop_bSpawnBack = !jkCoop_bOpeningSpawn;
    jkCoop_bOpeningSpawn = 0;
    jkCoop_Snapshot(pMe, jkCoop_aSpawnBefore);
    jkCoop_spawnStage = 1;
}

void jkCoop_CogAsksIsMulti(sithCog* pCog)
{
    // (the player's class cog: "call init_kyle; if(IsMulti()) call init_multi_kyle;", and the same in
    // Mysteries of the Sith's kyle_m, mara_m and luke_m)
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!jkCoop_spawnStage || !pMe || !pCog || pCog != pMe->pCog)
        return;
    jkCoop_Snapshot(pMe, jkCoop_aSpawnOwn);
    jkCoop_spawnStage = 2;
}

void jkCoop_SpawnEnd(SithThing* pPlayer)
{
    const int stage = jkCoop_spawnStage;
    jkCoop_spawnStage = 0;
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!stage || !pMe || pPlayer != pMe || !jkCoop_IsActive())
        return;
    const int bBack = jkCoop_bSpawnBack;
    // Coming in: the gear as the class cog's one-player start left it (its multiplayer spawn kit, a
    // deathmatch start, undone). Coming back after dying: this level's starting kit (what was picked up
    // since is in the backpack left behind), but not what the level has taken; and a saber only a cheat
    // gave, still the cheat's. Before that kit is taken (the level's first moments): as it was
    jkCoopKitBin aBack[SITHBIN_NUMBINS];
    if (bBack && jkCoop_bKitTaken) {
        for (int i = 0; i < SITHBIN_NUMBINS; i++) {
            aBack[i] = jkCoop_aKit[i];
            const jkCoopTaken* pTaken = &jkCoop_aTaken[i];
            if (jkCoop_aSpawnBefore[i].state & SITHINVENTORY_ITEM_CHEAT_GIVEN)
                aBack[i] = jkCoop_aSpawnBefore[i];
            else if ((pTaken->flags & JKCOOP_TAKEN_CAP) && aBack[i].amount > pTaken->cap)
                aBack[i].amount = pTaken->cap;
            if ((pTaken->flags & JKCOOP_TAKEN_OFF) || ((pTaken->flags & JKCOOP_TAKEN_CAP) && aBack[i].amount <= 0.0))
                aBack[i].state &= ~SITHINVENTORY_ITEM_AVAILABLE;
        }
    }
    else if (bBack)
        memcpy(aBack, jkCoop_aSpawnBefore, sizeof(aBack));
    else if (stage == 2)
        memcpy(aBack, jkCoop_aSpawnOwn, sizeof(aBack));
    else
        return; // (a class cog with no spawn kit of its own)
    memset(jkCoop_aSpawnTookWeapon, 0, sizeof(jkCoop_aSpawnTookWeapon));
    const int bTook = jkCoop_GearBack(pMe, aBack, jkCoop_aSpawnTookWeapon);
    if (bBack && jkCoop_bKitTaken)
        jkCoop_GiveKit(pMe, jkCoop_aKit, 0, JKCOOP_GIVE_FORCE | JKCOOP_GIVE_REST); // (the Force and the rest, as ever)
    if (bTook) {
        jkCoop_HoldOwnedWeapon(pMe, jkCoop_aSpawnTookWeapon);
        jkCoop_spawnTookMs = stdPlatform_GetTimeMsec();
        if (!jkCoop_spawnTookMs) jkCoop_spawnTookMs = 1;
    }
}

// ------------------------------------------------------------------ what a level takes

// One of the level's own scripts (not a class cog: a player's, an item's, a weapon's, an actor's)
static int jkCoop_IsLevelCog(sithCog* pCog)
{
    const SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    return pWorld && pWorld->aCogs && pCog >= pWorld->aCogs && pCog < pWorld->aCogs + pWorld->numCogs
           && !(pCog->flags & SITH_COG_CLASS);
}

void jkCoop_CogSetsInv(sithCog* pCog, SithThing* pThing, int bin, flex_t amount, int bAvailable)
{
    SithThing* pMe = jkCoop_LocalPlayer();
    if (jkCoop_IsActive() && pMe && pThing == pMe && jkCoop_BinIsForce(bin) && jkCoop_IsLevelCog(pCog))
        jkCoopForce_LevelSetsForce(pCog, pMe, bin, amount, bAvailable, 0); // (the story's Force)
    if (jkCoop_IsActive() && pMe && pThing && pThing != pMe && bAvailable < 0 && jkCoop_BinIsForce(bin))
        jkCoopForce_CogSetsOtherForce(pCog, pThing, bin, amount, 0); // (a guest's own innocents)
    if (!jkCoop_IsActive() || !pMe || pThing != pMe || !jkCoop_BinIsGear(bin) || !jkCoop_IsLevelCog(pCog))
        return;
    const SithInventoryType* pType = &sithInventory_g_aTypes[bin];
    const SithInventoryItem* pItem = &pMe->actorParams.pPlayer->aItems[bin];
    jkCoopTaken* pTaken = &jkCoop_aTaken[bin];
    // (a bin set to none, or made unavailable, is taken whether the player has it now or not: a guest's
    // gear can come after the level's start has run, from the host or a save)
    if (bAvailable < 0) {
        if (amount < pType->min) amount = pType->min;
        if (amount > pType->max) amount = pType->max;
        if (amount < pItem->amount || amount <= 0.0) {
            pTaken->flags |= JKCOOP_TAKEN_CAP;
            pTaken->cap = amount;
        }
        else if (amount > pItem->amount) {
            pTaken->flags &= ~JKCOOP_TAKEN_CAP; // (given back)
        }
    }
    else if (!bAvailable) {
        pTaken->flags |= JKCOOP_TAKEN_OFF;
    }
    else {
        pTaken->flags &= ~JKCOOP_TAKEN_OFF;
    }
}

void jkCoop_CogChangesInv(sithCog* pCog, SithThing* pThing, int bin, flex_t amount)
{
    SithThing* pMe = jkCoop_LocalPlayer();
    if (jkCoop_IsActive() && pMe && pThing == pMe && jkCoop_BinIsForce(bin) && jkCoop_IsLevelCog(pCog))
        jkCoopForce_LevelSetsForce(pCog, pMe, bin, amount, -1, 1); // (the story's Force)
    if (jkCoop_IsActive() && pMe && pThing && pThing != pMe && jkCoop_BinIsForce(bin))
        jkCoopForce_CogSetsOtherForce(pCog, pThing, bin, amount, 1); // (a guest's own innocents)
}

int jkCoop_GetStartTaken(jkCoopTaken aOut[SITHBIN_NUMBINS])
{
    if (!jkCoop_IsActive() || !sithNet_isServer || !jkCoop_bKitTaken || !jkCoop_bStartTaken)
        return 0;
    memcpy(aOut, jkCoop_aStartTaken, sizeof(jkCoop_aStartTaken));
    return 1;
}

void jkCoop_TakeAsHost(const char* pLevel, const jkCoopTaken aTaken[SITHBIN_NUMBINS])
{
    SithThing* pMe = jkCoop_LocalPlayer();
    if (!jkCoop_IsActive() || sithNet_isServer || !pMe || !pLevel || !sithWorld_g_pCurrentWorld
        || __strcmpi(sithWorld_g_pCurrentWorld->map_jkl_fname, pLevel))
        return;
    SithInventoryItem* aItems = pMe->actorParams.pPlayer->aItems;
    uint8_t aTook[SITHBIN_NUMBINS];
    memset(aTook, 0, sizeof(aTook));
    int bAny = 0;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        const jkCoopTaken* pIn = &aTaken[i];
        if (!pIn->flags || !jkCoop_BinIsGear(i))
            continue;
        jkCoopTaken* pTaken = &jkCoop_aTaken[i];
        if (pIn->flags & JKCOOP_TAKEN_CAP) {
            if (!(pTaken->flags & JKCOOP_TAKEN_CAP) || pIn->cap < pTaken->cap)
                pTaken->cap = pIn->cap;
            pTaken->flags |= JKCOOP_TAKEN_CAP;
            if (aItems[i].amount > pTaken->cap) {
                aTook[i] = (sithInventory_g_aTypes[i].flags & ITEMINFO_WEAPON) && pTaken->cap <= 0.0;
                sithInventory_SetInventory(pMe, i, pTaken->cap);
                bAny = 1;
            }
        }
        if (pIn->flags & JKCOOP_TAKEN_OFF) {
            pTaken->flags |= JKCOOP_TAKEN_OFF;
            aItems[i].state &= ~SITHINVENTORY_ITEM_AVAILABLE;
            aTook[i] |= (sithInventory_g_aTypes[i].flags & ITEMINFO_WEAPON) != 0;
            bAny = 1;
        }
    }
    if (bAny) {
        stdPlatform_Printf("OpenJKDF2: co-op, taken as the host's level began\n");
        jkCoop_HoldOwnedWeapon(pMe, aTook);
    }
}

// ------------------------------------------------------------------ the host's difficulty

#define JKCOOP_DIFF_SHIFT 18

int jkCoop_HostGameFlags(int flags)
{
    flags &= ~MULTIMODEFLAG_COOPDIFF_MASK;
    if ((flags & MULTIMODEFLAG_COOP) && jkPlayer_setDiff >= 0 && jkPlayer_setDiff <= 2)
        flags |= ((jkPlayer_setDiff + 1) << JKCOOP_DIFF_SHIFT) & MULTIMODEFLAG_COOPDIFF_MASK;
    return flags;
}

void jkCoop_UseHostDifficulty(void)
{
    if (stdComm_bIsServer || !(sithMulti_multiModeFlags & MULTIMODEFLAG_COOP))
        return;
    const int diff = ((sithMulti_multiModeFlags & MULTIMODEFLAG_COOPDIFF_MASK) >> JKCOOP_DIFF_SHIFT) - 1;
    if (diff < 0 || diff > 2 || diff == jkPlayer_setDiff)
        return;
    if (jkCoop_ownDifficulty < 0)
        jkCoop_ownDifficulty = jkPlayer_setDiff;
    stdPlatform_Printf("OpenJKDF2: co-op, the host's difficulty (%d)\n", diff);
    jkPlayer_setDiff = diff;
}

// ------------------------------------------------------------------ player starts

int jkCoop_AddPlayerStarts(SithWorld* pWorld, int numStarts)
{
    // (a multiplayer load of a co-op game; sithNet_isMulti isn't set yet while the level loads)
    if (!pWorld || jkSmack_gameMode != 2 || !(sithMulti_multiModeFlags & MULTIMODEFLAG_COOP) || numStarts < 1)
        return numStarts;
    SithThing* pFirst = jkPlayer_playerInfos[0].pLocalPlayer;
    if (!pFirst || !pFirst->sector || !pFirst->pTemplate)
        return numStarts;
    while (numStarts < JKCOOP_MAX_PLAYERS && numStarts < JKPLAYER_NUM_INFOS) {
        rdMatrix34 orient = pFirst->orient;
        rdVector_Zero3(&orient.scale);
        SithThing* pStart = sithThing_CreateThingAtPos(pFirst->pTemplate, &pFirst->position, &orient, pFirst->sector, NULL);
        if (!pStart || pStart->type != SITH_THING_PLAYER)
            break;
        // (as sithPlayer_PlacePlayers makes one of the level's own)
        pStart->guid = pStart->idx; // (as the level's own things: the same everywhere)
        SithPlayer* pInfo = &jkPlayer_playerInfos[numStarts];
        pInfo->pLocalPlayer = pStart;
        pStart->flags |= SITH_TF_INVULN;
        pStart->actorParams.pPlayer = pInfo;
        pInfo->flags |= 2;
        rdMatrix_Copy34(&pInfo->orient, &pFirst->orient);
        rdVector_Copy3(&pInfo->orient.scale, &pFirst->position);
        pInfo->pInSector = pFirst->sector;
        numStarts++;
    }
    return numStarts;
}

// ------------------------------------------------------------------ starting spots

// (room between two players, beyond their sizes)
#define JKCOOP_SPAWN_GAP ((flex_t)0.01)
// (a spot with no floor this far below the player's middle is over a drop)
#define JKCOOP_SPAWN_FLOOR_DROP ((flex_t)0.2)

// Another player (alive, in the level) within reach of pPos for pPlayer
static int jkCoop_SpotTaken(SithThing* pPlayer, const rdVector3* pPos)
{
    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    for (int i = 0; i <= pWorld->numThings; i++) {
        SithThing* pOther = &pWorld->aThings[i];
        if (pOther == pPlayer || pOther->type != SITH_THING_PLAYER || !pOther->sector || (pOther->flags & (SITH_TF_DEAD | SITH_TF_DISABLED)))
            continue;
        const flex_t reach = pPlayer->collideSize + pOther->collideSize + JKCOOP_SPAWN_GAP;
        if (rdVector_Dist3(&pOther->position, pPos) < reach)
            return 1;
    }
    return 0;
}

void jkCoop_PlacePlayer(SithThing* pPlayer)
{
    if (!jkCoop_IsActive() || !pPlayer || !pPlayer->sector || !sithWorld_g_pCurrentWorld)
        return;
    if (!jkCoop_SpotTaken(pPlayer, &pPlayer->position))
        return;

    // beside, behind, ahead, then aslant, a step further each round
    static const flex_t aSide[] = { 1, -1, 0, 0, 1, -1, 1, -1 };
    static const flex_t aBack[] = { 0, 0, -1, 1, -1, -1, 1, 1 };
    const flex_t step = (pPlayer->collideSize * 2.5) + JKCOOP_SPAWN_GAP;
    rdVector3 start = pPlayer->position;
    // (the way there tried a little above the floor, by a narrower ball, so the
    // floor itself isn't in the way)
    const flex_t wayRadius = pPlayer->collideSize * 0.5;
    rdVector3 startUp = start;
    startUp.z += wayRadius;
    for (int round = 1; round <= 3; round++) {
        for (int k = 0; k < (int)(sizeof(aSide) / sizeof(aSide[0])); k++) {
            rdVector3 spot = start;
            rdVector_ScaleAdd3Acc(&spot, &pPlayer->orient.rvec, aSide[k] * step * round);
            rdVector_ScaleAdd3Acc(&spot, &pPlayer->orient.lvec, aBack[k] * step * round);
            if (jkCoop_SpotTaken(pPlayer, &spot))
                continue;
            rdVector3 spotUp = spot;
            spotUp.z += wayRadius;
            if (!sithCollision_CheckPathClearOfWalls(pPlayer->sector, &startUp, &spotUp, wayRadius))
                continue;
            SithSector* pSector = sithCollision_FindSectorInRadius(pPlayer->sector, &start, &spot, 0.0);
            if (!pSector)
                continue;
            rdVector3 below = spot;
            below.z -= JKCOOP_SPAWN_FLOOR_DROP;
            if (sithCollision_CheckPathClearOfWalls(pSector, &spot, &below, 0.0))
                continue; // (no floor near)

            sithThing_ExitSector(pPlayer);
            pPlayer->position = spot;
            sithThing_EnterSector(pPlayer, pSector, 1, 0);
            sithPhysics_FindFloor(pPlayer, 1);
            return;
        }
    }
    // (nowhere free: where the game put them)
}

// ------------------------------------------------------------------ the host's level

int jkCoop_HostMayPickLevel(void)
{
    return jkCoop_IsActive() && sithNet_isServer && jkDev_Custom_CheatsAllowed();
}

static char jkCoop_aPickedOn[32]; // (the level that pick ends)

static void jkCoop_PickOnThisLevel(int entryIdx)
{
    jkCoop_pickedEntry = entryIdx;
    jkCoop_aPickedOn[0] = 0;
    if (sithWorld_g_pCurrentWorld)
        stdString_SafeStrCopy(jkCoop_aPickedOn, sithWorld_g_pCurrentWorld->map_jkl_fname, sizeof(jkCoop_aPickedOn));
}

int jkCoop_HostPickLevel(int entryIdx)
{
    if (!jkCoop_HostMayPickLevel() || entryIdx < 0 || entryIdx >= jkEpisode_mLoad.numSeq || !jkEpisode_mLoad.paEntries)
        return 0;
    jkCoop_PickOnThisLevel(entryIdx);
    jkDSS_SendEndLevel(); // (everyone's level ends in a moment, the guests told where to)
    return 1;
}

int jkCoop_PickedLevelLine(void)
{
    if (!jkCoop_HostMayPickLevel() || jkCoop_pickedEntry < 0 || jkCoop_pickedEntry >= jkEpisode_mLoad.numSeq || !jkEpisode_mLoad.paEntries)
        return -1;
    return jkEpisode_mLoad.paEntries[jkCoop_pickedEntry].lineNum;
}

void jkCoop_HostPickedLevel(int lineNum)
{
    // (the host hears its own message too: its pick stays)
    if (!jkCoop_IsActive() || sithNet_isServer)
        return;
    jkCoop_pickedEntry = -1;
    for (int i = 0; lineNum >= 0 && jkEpisode_mLoad.paEntries && i < jkEpisode_mLoad.numSeq; i++) {
        if (jkEpisode_mLoad.paEntries[i].lineNum == lineNum && jkEpisode_mLoad.paEntries[i].type == 0) { // (0: a level)
            jkCoop_PickOnThisLevel(i);
            stdPlatform_Printf("OpenJKDF2: co-op, the host picked %s (line %d)\n", jkEpisode_mLoad.paEntries[i].fileName, lineNum);
            break;
        }
    }
}

int jkCoop_TakePickedLevel(void)
{
    const int idx = jkCoop_pickedEntry;
    jkCoop_pickedEntry = -1;
    // (only at the end of the level it was picked on: not one left before it ended)
    if (idx < 0 || !jkCoop_IsActive() || idx >= jkEpisode_mLoad.numSeq || !sithWorld_g_pCurrentWorld
        || __strcmpi(sithWorld_g_pCurrentWorld->map_jkl_fname, jkCoop_aPickedOn))
        return -1;
    return idx;
}

int jkCoop_HostLevelFromJoining(SithMessage* pMsg)
{
    jkCoop_aHostLevel[0] = 0;
    // (code, 0, then the level's file name: 32 bytes)
    if (!pMsg || !jkCoop_IsActive() || stdComm_bIsServer || pMsg->netMsg.msg_size < 8 + 32)
        return 0;
    NETMSG_IN_START(pMsg);
    NETMSG_POPS32();
    NETMSG_POPS32();
    NETMSG_POPSTR(jkCoop_aHostLevel, 32);
    int bKnown = 0;
    for (int i = 0; i < jkEpisode_mLoad.numSeq && jkEpisode_mLoad.paEntries && !bKnown; i++)
        bKnown = !__strcmpi(jkEpisode_mLoad.paEntries[i].fileName, jkCoop_aHostLevel);
    // (not a level of this episode: the game's own "wrong level"; nor the one just
    // loaded, a late answer to an earlier try)
    if (!jkCoop_aHostLevel[0] || !bKnown
        || (sithWorld_g_pCurrentWorld && !__strcmpi(sithWorld_g_pCurrentWorld->map_jkl_fname, jkCoop_aHostLevel))) {
        jkCoop_aHostLevel[0] = 0;
        return 0;
    }
    stdPlatform_Printf("OpenJKDF2: co-op, the host is on %s\n", jkCoop_aHostLevel);
    return 1;
}

int jkCoop_FollowHostLevel(void)
{
    if (!jkCoop_aHostLevel[0] || !jkCoop_IsActive())
        return 0;
    // (only a level of this episode)
    if (!jkEpisode_idk4(&jkEpisode_mLoad, jkCoop_aHostLevel)) {
        jkCoop_aHostLevel[0] = 0;
        return 0;
    }
    stdString_SafeStrCopy(jkMain_aLevelJklFname, jkCoop_aHostLevel, 128);
    jkPlayer_bLoadingSomething = 1; // (the next level worked out from this one)
    jkSmack_gameMode = 2;
    jkCoop_aHostLevel[0] = 0;
    return 1;
}

// ------------------------------------------------------------------ menus that go on by themselves

static jkGuiMenu* jkCoop_pAutoMenu = NULL;
static void (*jkCoop_pfAutoMenuOwn)(jkGuiMenu*) = NULL;
static uint32_t jkCoop_autoMenuStartMs = 0;
static uint32_t jkCoop_autoMenuWaitMs = 0;

static void jkCoop_AutoMenuIdle(jkGuiMenu* pMenu)
{
    if (jkCoop_pfAutoMenuOwn)
        jkCoop_pfAutoMenuOwn(pMenu);
    if (pMenu == jkCoop_pAutoMenu && stdPlatform_GetTimeMsec() - jkCoop_autoMenuStartMs > jkCoop_autoMenuWaitMs)
        pMenu->lastClicked = 1; // (OK)
}

void jkCoop_MenuAutoContinue(jkGuiMenu* pMenu, uint32_t msWait)
{
    if (!pMenu || !jkCoop_IsActive() || jkCoop_pAutoMenu)
        return;
    jkCoop_pAutoMenu = pMenu;
    jkCoop_pfAutoMenuOwn = pMenu->idkFunc;
    jkCoop_autoMenuStartMs = stdPlatform_GetTimeMsec();
    jkCoop_autoMenuWaitMs = msWait;
    pMenu->idkFunc = jkCoop_AutoMenuIdle;
}

void jkCoop_MenuDone(jkGuiMenu* pMenu)
{
    if (!pMenu || pMenu != jkCoop_pAutoMenu)
        return;
    pMenu->idkFunc = jkCoop_pfAutoMenuOwn;
    jkCoop_pAutoMenu = NULL;
    jkCoop_pfAutoMenuOwn = NULL;
}

#endif // QOL_IMPROVEMENTS
