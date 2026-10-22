// Added: entire file. See jkCoopForce.h.

#include "Main/jkCoopForce.h"

#ifdef QOL_IMPROVEMENTS

#include "jk.h"
#include "stdPlatform.h"
#include "Dss/jkNetExt.h"
#include "Dss/sithMulti.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithTime.h"
#include "World/sithWorld.h"
#include "World/jkPlayer.h"
#include "Main/jkEpisode.h"
#include "Main/jkDev.h"
#include "Main/jkCoopSave.h"
#include "Cog/sithCogParse.h"
#include "Cog/sithCogFunctionThing.h"
#include "General/stdString.h"
#include "Win95/stdComm.h"
#include <string.h>

#define JKCOOPFORCE_VERSION 1
// (how long a level's start is, in its own time and the player's: what its scripts do to the Force by
// then is the level's start)
#define JKCOOPFORCE_START_MS (2000)
#define JKCOOPFORCE_MAX_SETS 64

// What the Force bins' states keep (as a power is had: found by the story, and usable)
#define JKCOOPFORCE_STATE (SITHINVENTORY_ITEM_AVAILABLE | SITHINVENTORY_ITEM_FOUND)

// The story's Force this level (the Force bins as only the level's scripts and the story have set them,
// not a cheat), for a level played with cheats
static jkCoopKitBin jkCoopForce_aStory[SITHBIN_NUMBINS];
static int jkCoopForce_bStory = 0;

// The level's start: when it opened, still in it, and (the host) which Force bins its scripts set
static uint32_t jkCoopForce_openMs = 0;
static int jkCoopForce_bInStart = 0;
static uint8_t jkCoopForce_aStartSet[SITHBIN_NUMBINS];

// What the host's level set as it began: the host's (sent), or a guest's from the host (to do)
typedef struct jkCoopForceSet
{
    uint8_t bin;
    uint8_t state;
    float amount;
} jkCoopForceSet;
static char jkCoopForce_aStartLevel[32];
static jkCoopForceSet jkCoopForce_aStart[JKCOOPFORCE_MAX_SETS];
static int jkCoopForce_numStart = 0;
static int jkCoopForce_bStartDone = 0; // (a guest: done this level)

// The host's secrets as last sent
static flex_t jkCoopForce_sentSecrets = -1.0;
static flex_t jkCoopForce_sentMaxSecrets = -1.0;

// The level has ended (in it), and whether cheats were used in it
static int jkCoopForce_bEnded = 0;
static int jkCoopForce_bEndedCheated = 0;

enum
{
    JKCOOPFORCE_MSG_START = 1,   // [level: 32][count: u8][count x (bin: u8, amount: f32, state: u8)]
    JKCOOPFORCE_MSG_SECRETS = 2, // [level: 32][found: f32][of: f32]
    JKCOOPFORCE_MSG_GIVE = 3,    // [level: 32][bin: u8][by: u8, 1 an amount more, 0 the amount][amount: f32]
};

static SithThing* jkCoopForce_LocalPlayer(void)
{
    SithThing* pMe = sithPlayer_g_pLocalPlayerThing;
    if (!pMe || pMe->type != SITH_THING_PLAYER || !pMe->actorParams.pPlayer)
        return NULL;
    return pMe;
}

static const char* jkCoopForce_LevelName(void)
{
    return sithWorld_g_pCurrentWorld ? sithWorld_g_pCurrentWorld->map_jkl_fname : "";
}

// The Force bins the story keeps in the player's own: not the Force's energy, which fills as each level
// begins
static int jkCoopForce_BinIsStory(int bin)
{
    return jkCoop_BinIsForce(bin) && bin != SITHBIN_FORCEMANA;
}

static void jkCoopForce_SetBin(SithPlayer* pPlayer, int bin, flex_t amount, uint32_t state)
{
    const SithInventoryType* pType = &sithInventory_g_aTypes[bin];
    if (amount < pType->min) amount = pType->min;
    if (amount > pType->max) amount = pType->max;
    pPlayer->aItems[bin].amount = amount;
    pPlayer->aItems[bin].state = (pPlayer->aItems[bin].state & ~JKCOOPFORCE_STATE) | (state & JKCOOPFORCE_STATE);
}

// The powers had are usable (as jkPlayer_InitForceBins makes them as a level loads)
static void jkCoopForce_PowersUsable(SithPlayer* pPlayer)
{
    for (int i = SITHBIN_FP_START; i <= SITHBIN_FP_END; i++) {
        if (i == SITHBIN_JEDI_RANK)
            continue;
        if (pPlayer->aItems[i].amount > 0.0 && (pPlayer->aItems[i].state & SITHINVENTORY_ITEM_FOUND))
            pPlayer->aItems[i].state |= SITHINVENTORY_ITEM_AVAILABLE;
        else
            pPlayer->aItems[i].state &= ~SITHINVENTORY_ITEM_AVAILABLE;
    }
}

// In a level played with cheats
static int jkCoopForce_Cheated(void)
{
    return jkCoopForce_bStory && jkDev_Custom_CheatedThisLevel();
}

// The player's Force as the story has it: a level played with cheats, the story's; else the player's own
static void jkCoopForce_CleanForce(const SithPlayer* pPlayer, jkCoopKitBin aOut[SITHBIN_NUMBINS])
{
    const int bCheated = jkCoopForce_Cheated();
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        aOut[i].amount = pPlayer->aItems[i].amount;
        aOut[i].state = (int32_t)(pPlayer->aItems[i].state & JKCOOPFORCE_STATE);
        if (bCheated && jkCoopForce_BinIsStory(i))
            aOut[i] = jkCoopForce_aStory[i];
    }
}

// ------------------------------------------------------------------ the character

void jkCoopForce_MpcInitLooks(SithPlayer* pPlayer)
{
    // (as jkPlayer_MpcInitBins, without the character's Force bins)
    jkPlayerMpcInfo info;
    jkPlayer_MPCParse(&info, pPlayer, jkPlayer_playerShortName, jkPlayer_name, 0);
    jkPlayer_InitForceBins();
    const int choice = (int)sithPlayer_GetInvItemAmount(SITHBIN_CHOICE);
    if (choice != 1 && choice != 2) {
        flex_t alignment = jkPlayer_CalcStarsAlign();
        if (alignment > 100.0) alignment = 100.0;
        if (alignment < -100.0) alignment = -100.0;
        sithPlayer_SetInvItemAmount(SITHBIN_ALIGNMENT, alignment);
    }
}

int jkCoopForce_CharacterRank(int maxRank, int multiModeFlags)
{
    return (multiModeFlags & MULTIMODEFLAG_COOP) ? -1 : maxRank;
}

int jkCoopForce_MultiRules(void)
{
    return sithNet_isMulti && !jkCoop_IsActive();
}

int jkCoopForce_CogIsMulti(sithCog* pCog)
{
    if (!jkCoop_IsActive() || !Main_bMotsCompat || !pCog || pCog != sithInventory_g_aTypes[SITHBIN_JEDI_RANK].cog)
        return -1;
    return 0;
}

// ------------------------------------------------------------------ in a level

void jkCoopForce_LevelOpened(SithThing* pMe)
{
    jkCoopForce_openMs = stdPlatform_GetTimeMsec();
    jkCoopForce_bInStart = 1;
    jkCoopForce_bStartDone = 0;
    jkCoopForce_bEnded = 0;
    jkCoopForce_sentSecrets = -1.0;
    jkCoopForce_sentMaxSecrets = -1.0;
    memset(jkCoopForce_aStartSet, 0, sizeof(jkCoopForce_aStartSet));
    // (the host's level starts its own; a guest's from the host are kept for the level they are of)
    if (sithNet_isServer || __strcmpi(jkCoopForce_aStartLevel, jkCoopForce_LevelName()))
        jkCoopForce_numStart = 0;

    // The powers carried in usable, as one player's are as a level loads; and the story's Force
    SithPlayer* pPlayer = pMe->actorParams.pPlayer;
    jkCoopForce_PowersUsable(pPlayer);
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        jkCoopForce_aStory[i].amount = pPlayer->aItems[i].amount;
        jkCoopForce_aStory[i].state = (int32_t)(pPlayer->aItems[i].state & JKCOOPFORCE_STATE);
    }
    jkCoopForce_bStory = 1;
}

static void jkCoopForce_SendGive(int idTo, int bin, int bBy, flex_t amount)
{
    uint8_t aMsg[2 + 32 + 2 + 4];
    int n = 0;
    aMsg[n++] = JKCOOPFORCE_VERSION;
    aMsg[n++] = JKCOOPFORCE_MSG_GIVE;
    memset(aMsg + n, 0, 32);
    stdString_SafeStrCopy((char*)aMsg + n, jkCoopForce_LevelName(), 32);
    n += 32;
    aMsg[n++] = (uint8_t)bin;
    aMsg[n++] = (uint8_t)(bBy != 0);
    const float f = (float)amount;
    memcpy(aMsg + n, &f, 4); n += 4;
    jkNetExt_Send(JKNETEXT_COOPFORCE, aMsg, n, idTo);
}

void jkCoopForce_LevelSetsForce(sithCog* pCog, SithThing* pMe, int bin, flex_t amount, int bAvailable, int bChange)
{
    if (!jkCoopForce_bStory || bin <= 0 || bin >= SITHBIN_NUMBINS)
        return;
    const SithInventoryType* pType = &sithInventory_g_aTypes[bin];
    // Stars the host's level gives it in the level, by a part of it the game runs on the host only (not
    // its start or its end, nor a script every game runs: 16aEscapeShip's all-secrets star), given to the
    // guests too (the same, an amount more or the amount)
    if (sithNet_isServer && !jkCoopForce_bInStart && !jkCoopForce_bEnded && bAvailable < 0 && pCog
        && !(pCog->flags & SITH_COG_LOCAL) && (bin == SITHBIN_NEW_STARS || bin == SITHBIN_SPEND_STARS)) {
        const flex_t now = pMe->actorParams.pPlayer->aItems[bin].amount;
        flex_t given = amount;
        if (given < pType->min) given = pType->min;
        if (given > pType->max) given = pType->max;
        if (given != now) {
            stdPlatform_Printf("OpenJKDF2: co-op, the level's stars given to everyone (%d)\n", bin);
            jkCoopForce_SendGive(-1, bin, bChange, bChange ? given - now : given);
        }
    }
    jkCoopKitBin* pStory = &jkCoopForce_aStory[bin];
    if (bAvailable < 0) {
        if (amount < pType->min) amount = pType->min;
        if (amount > pType->max) amount = pType->max;
        pStory->amount = amount;
    }
    else if (bAvailable)
        pStory->state |= SITHINVENTORY_ITEM_AVAILABLE;
    else
        pStory->state &= ~SITHINVENTORY_ITEM_AVAILABLE;
    if (jkCoopForce_bInStart && sithNet_isServer)
        jkCoopForce_aStartSet[bin] = 1;
}

void jkCoopForce_CleanKit(jkCoopKitBin aKit[SITHBIN_NUMBINS])
{
    if (!jkCoop_IsActive() || !jkCoopForce_Cheated() || !jkCoopForce_LocalPlayer())
        return;
    for (int i = 0; i < SITHBIN_NUMBINS; i++)
        if (jkCoopForce_BinIsStory(i))
            aKit[i] = jkCoopForce_aStory[i];
}

// ------------------------------------------------------------------ the host's level's start, everyone's

static void jkCoopForce_SendStart(int idTo)
{
    if (!jkCoopForce_numStart)
        return;
    uint8_t aMsg[JKNETEXT_MAX_LEN];
    int n = 0;
    aMsg[n++] = JKCOOPFORCE_VERSION;
    aMsg[n++] = JKCOOPFORCE_MSG_START;
    memset(aMsg + n, 0, 32);
    stdString_SafeStrCopy((char*)aMsg + n, jkCoopForce_aStartLevel, 32);
    n += 32;
    aMsg[n++] = (uint8_t)jkCoopForce_numStart;
    for (int k = 0; k < jkCoopForce_numStart; k++) {
        aMsg[n++] = jkCoopForce_aStart[k].bin;
        memcpy(aMsg + n, &jkCoopForce_aStart[k].amount, 4); n += 4;
        aMsg[n++] = jkCoopForce_aStart[k].state;
    }
    jkNetExt_Send(JKNETEXT_COOPFORCE, aMsg, n, idTo);
}

static void jkCoopForce_SendSecrets(int idTo)
{
    SithThing* pMe = jkCoopForce_LocalPlayer();
    if (!pMe || !sithWorld_g_pCurrentWorld)
        return;
    const float found = (float)sithInventory_GetInventory(pMe, SITHBIN_SECRETS);
    const float of = (float)sithInventory_GetInventory(pMe, SITHBIN_MAXSECRETS);
    uint8_t aMsg[2 + 32 + 8];
    int n = 0;
    aMsg[n++] = JKCOOPFORCE_VERSION;
    aMsg[n++] = JKCOOPFORCE_MSG_SECRETS;
    memset(aMsg + n, 0, 32);
    stdString_SafeStrCopy((char*)aMsg + n, jkCoopForce_LevelName(), 32);
    n += 32;
    memcpy(aMsg + n, &found, 4); n += 4;
    memcpy(aMsg + n, &of, 4); n += 4;
    jkNetExt_Send(JKNETEXT_COOPFORCE, aMsg, n, idTo);
    if (idTo == -1) {
        jkCoopForce_sentSecrets = found;
        jkCoopForce_sentMaxSecrets = of;
    }
}

// A guest: the Force as the host's level set it as it began, done here too (once a level, and again after
// a kit from the host, which only raises what it gives); 1 if it changed anything. bTell: the host told
// what this level's starting kit now is, if it has been taken
static int jkCoopForce_DoStart(SithThing* pMe, int bTell)
{
    if (!jkCoopForce_numStart || __strcmpi(jkCoopForce_aStartLevel, jkCoopForce_LevelName()))
        return 0;
    jkCoopForce_bStartDone = 1;
    SithPlayer* pPlayer = pMe->actorParams.pPlayer;
    int bChanged = 0;
    for (int k = 0; k < jkCoopForce_numStart; k++) {
        const int bin = jkCoopForce_aStart[k].bin;
        if (!jkCoop_BinIsForce(bin))
            continue;
        const uint32_t state = jkCoopForce_aStart[k].state & JKCOOPFORCE_STATE;
        const flex_t before = pPlayer->aItems[bin].amount;
        const uint32_t stateBefore = pPlayer->aItems[bin].state & JKCOOPFORCE_STATE;
        if (jkCoopForce_aStart[k].amount != before)
            sithInventory_SetInventory(pMe, bin, jkCoopForce_aStart[k].amount); // (as the level's script)
        pPlayer->aItems[bin].state = (pPlayer->aItems[bin].state & ~JKCOOPFORCE_STATE) | state;
        if (pPlayer->aItems[bin].amount != before || state != stateBefore)
            bChanged = 1;
        if (jkCoopForce_BinIsStory(bin)) {
            jkCoopForce_aStory[bin].amount = pPlayer->aItems[bin].amount;
            jkCoopForce_aStory[bin].state = (int32_t)state;
        }
    }
    if (!bChanged)
        return 0;
    stdPlatform_Printf("OpenJKDF2: co-op, the Force as the host's level began\n");
    // (this level's starting kit as the level made it, and the host told)
    jkCoopKitBin aForce[SITHBIN_NUMBINS];
    jkCoopForce_CleanForce(pPlayer, aForce);
    jkCoop_KeepForce(aForce);
    if (bTell && jkCoop_StartKitTaken())
        jkCoopSave_KitTaken(1);
    return 1;
}

static void jkCoopForce_Arrived(int idFrom, const uint8_t* pData, int len)
{
    if (!jkCoop_IsActive() || stdComm_bIsServer || idFrom != (int)sithNet_serverNetId || len < 2 + 32
        || pData[0] != JKCOOPFORCE_VERSION)
        return;
    char level[33];
    memcpy(level, pData + 2, 32);
    level[32] = 0;
    SithThing* pMe = jkCoopForce_LocalPlayer();
    const int bHere = pMe && jkCoopForce_bStory && !__strcmpi(level, jkCoopForce_LevelName());
    if (pData[1] == JKCOOPFORCE_MSG_START && len >= 2 + 32 + 1) {
        const int bNew = __strcmpi(level, jkCoopForce_aStartLevel) != 0;
        stdString_SafeStrCopy(jkCoopForce_aStartLevel, level, sizeof(jkCoopForce_aStartLevel));
        jkCoopForce_numStart = 0;
        const int count = pData[34];
        int n = 35;
        for (int k = 0; k < count && n + 6 <= len && jkCoopForce_numStart < JKCOOPFORCE_MAX_SETS; k++, n += 6) {
            jkCoopForceSet* pSet = &jkCoopForce_aStart[jkCoopForce_numStart++];
            pSet->bin = pData[n];
            memcpy(&pSet->amount, pData + n + 1, 4);
            pSet->state = pData[n + 5];
        }
        if (bNew)
            jkCoopForce_bStartDone = 0;
        if (bHere && !jkCoopForce_bStartDone)
            jkCoopForce_DoStart(pMe, 1);
    }
    else if (pData[1] == JKCOOPFORCE_MSG_GIVE && len >= 2 + 32 + 6 && bHere && jkCoop_BinIsForce(pData[34])) {
        // A Force bin the host's game gave this player (an amount more, or the amount)
        const int bin = pData[34];
        float f;
        memcpy(&f, pData + 36, 4);
        if (pData[35])
            sithInventory_ChangeInventory(pMe, bin, f);
        else
            sithInventory_SetInventory(pMe, bin, f);
        if (jkCoopForce_BinIsStory(bin))
            jkCoopForce_aStory[bin].amount = pMe->actorParams.pPlayer->aItems[bin].amount;
        stdPlatform_Printf("OpenJKDF2: co-op, given by the host's game (%d)\n", bin);
    }
    else if (pData[1] == JKCOOPFORCE_MSG_SECRETS && len >= 2 + 32 + 8 && bHere) {
        // The host's count, everyone's (the same level's: the same number of them)
        float found, of;
        memcpy(&found, pData + 34, 4);
        memcpy(&of, pData + 38, 4);
        if (of == (float)sithInventory_GetInventory(pMe, SITHBIN_MAXSECRETS)
            && (float)sithInventory_GetInventory(pMe, SITHBIN_SECRETS) != found)
            sithInventory_SetInventory(pMe, SITHBIN_SECRETS, found);
    }
}

void jkCoopForce_Tick(SithThing* pMe)
{
    if (!jkCoopForce_bStory)
        return;
    // The level's start over (its own time, and the player's: a cutscene may stop the first): the host's
    // level's own Force kept, and sent to the guests; a guest does what came from the host while it loaded
    if (jkCoopForce_bInStart && sithTime_g_msecGameTime >= JKCOOPFORCE_START_MS
        && stdPlatform_GetTimeMsec() - jkCoopForce_openMs >= JKCOOPFORCE_START_MS) {
        jkCoopForce_bInStart = 0;
        if (sithNet_isServer) {
            // (as the story has it: not what a cheat of the host's gave)
            SithPlayer* pPlayer = pMe->actorParams.pPlayer;
            jkCoopKitBin aForce[SITHBIN_NUMBINS];
            jkCoopForce_CleanForce(pPlayer, aForce);
            stdString_SafeStrCopy(jkCoopForce_aStartLevel, jkCoopForce_LevelName(), sizeof(jkCoopForce_aStartLevel));
            jkCoopForce_numStart = 0;
            for (int i = 0; i < SITHBIN_NUMBINS && i < 256 && jkCoopForce_numStart < JKCOOPFORCE_MAX_SETS; i++) {
                if (!jkCoopForce_aStartSet[i])
                    continue;
                jkCoopForceSet* pSet = &jkCoopForce_aStart[jkCoopForce_numStart++];
                pSet->bin = (uint8_t)i;
                pSet->amount = (float)aForce[i].amount;
                pSet->state = (uint8_t)(aForce[i].state & JKCOOPFORCE_STATE);
            }
            if (jkCoopForce_numStart) {
                // (this level's starting kit as its scripts made it, even if taken before, and saved)
                jkCoop_KeepForce(aForce);
                if (jkCoop_StartKitTaken())
                    jkCoopSave_KitTaken(0);
                jkCoopForce_SendStart(-1);
            }
        }
        else if (!jkCoopForce_bStartDone)
            jkCoopForce_DoStart(pMe, 1);
    }
    // The host's secrets (it counts everyone's finds), everyone's
    if (sithNet_isServer
        && (sithInventory_GetInventory(pMe, SITHBIN_SECRETS) != jkCoopForce_sentSecrets
            || sithInventory_GetInventory(pMe, SITHBIN_MAXSECRETS) != jkCoopForce_sentMaxSecrets))
        jkCoopForce_SendSecrets(-1);
}

void jkCoopForce_StartKitDue(void)
{
    // (a guest: what the host's level did as it began, if already here, in this level's starting kit)
    SithThing* pMe = jkCoopForce_LocalPlayer();
    if (jkCoop_IsActive() && !sithNet_isServer && pMe && jkCoopForce_bStory && !jkCoopForce_bStartDone)
        jkCoopForce_DoStart(pMe, 0);
}

void jkCoopForce_KitFromHost(SithThing* pMe)
{
    if (!jkCoopForce_bStory)
        return;
    // (the story's Force raised as the kit raised it)
    SithPlayer* pPlayer = pMe->actorParams.pPlayer;
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        if (!jkCoopForce_BinIsStory(i))
            continue;
        if (pPlayer->aItems[i].amount > jkCoopForce_aStory[i].amount || sithInventory_g_aTypes[i].min < 0.0)
            jkCoopForce_aStory[i].amount = pPlayer->aItems[i].amount;
        jkCoopForce_aStory[i].state |= (int32_t)(pPlayer->aItems[i].state & SITHINVENTORY_ITEM_FOUND);
    }
    jkCoopForce_PowersUsable(pPlayer);
    jkCoopKitBin aForce[SITHBIN_NUMBINS];
    jkCoopForce_CleanForce(pPlayer, aForce);
    jkCoop_KeepForce(aForce);
    // (the host's level's start again on top, as it was; and the host told what this player now has)
    if (!jkCoopForce_DoStart(pMe, 1) && jkCoop_StartKitTaken())
        jkCoopSave_KitTaken(1);
}

// ------------------------------------------------------------------ a level's end

// The powers the story gives at the end of the level, by the player's side: the level's own, and, where
// the paths split (the levels of the same number), the other path's level's (each player keeps their own
// side, but plays the host's path)
void jkCoopForce_LevelEnding(void)
{
    if (!jkCoop_IsActive() || Main_bMotsCompat || !jkEpisode_mLoad.numSeq || !jkEpisode_mLoad.paEntries)
        return;
    SithThing* pMe = jkCoopForce_LocalPlayer();
    if (!pMe)
        return;
    const jkEpisodeEntry* pCur = jkEpisode_GetCurrentEpisodeEntry(&jkEpisode_mLoad);
    const int choice = jkPlayer_GetChoice();
    for (int i = 0; i < jkEpisode_mLoad.numSeq; i++) {
        const jkEpisodeEntry* pEnt = &jkEpisode_mLoad.paEntries[i];
        if (pEnt->type != 0 || pEnt->level != pCur->level) // (0: a level)
            continue;
        const int aPow[2] = { choice != 2 ? pEnt->lightpow : 0, choice != 1 ? pEnt->darkpow : 0 };
        for (int k = 0; k < 2; k++) {
            const int bin = aPow[k];
            if (bin < SITHBIN_FP_START || bin > SITHBIN_FP_END)
                continue;
            if (pEnt != pCur) {
                stdPlatform_Printf("OpenJKDF2: co-op, %s's power too (%d)\n", pEnt->fileName, bin);
                sithInventory_SetCarries(pMe, bin, 1);
            }
            jkCoopForce_aStory[bin].state |= SITHINVENTORY_ITEM_FOUND;
        }
    }
}

void jkCoopForce_LevelEnded(void)
{
    if (!jkCoop_IsActive() || !jkCoopForce_bStory)
        return;
    jkCoopForce_bEnded = 1;
    jkCoopForce_bEndedCheated = jkCoopForce_Cheated();
    if (sithNet_isServer)
        jkCoopForce_HostEndingLevel();
}

void jkCoopForce_BetweenLevels(void)
{
    jkCoopKitBin aForce[SITHBIN_NUMBINS];
    if (!jkCoop_IsActive() || !jkCoop_GetKit(aForce))
        return;
    const SithPlayer* pPlayer = &jkPlayer_playerInfos[playerThingIdx];
    for (int i = 0; i < SITHBIN_NUMBINS; i++) {
        aForce[i].amount = pPlayer->aItems[i].amount;
        aForce[i].state = (int32_t)(pPlayer->aItems[i].state & JKCOOPFORCE_STATE);
    }
    jkCoop_KeepForce(aForce);
}

void jkCoopForce_LevelClosed(void)
{
    if (!jkCoopForce_bEnded || !jkCoop_IsActive())
        return;
    jkCoopForce_bEnded = 0;
    jkCoopForce_bStory = 0;
    if (jkCoopForce_bEndedCheated) {
        // A level played with cheats: the story's Force (what its scripts gave, to its end), not the
        // cheats', for the screens after the level, and kept
        SithPlayer* pPlayer = &jkPlayer_playerInfos[playerThingIdx];
        for (int i = 0; i < SITHBIN_NUMBINS; i++)
            if (jkCoopForce_BinIsStory(i))
                jkCoopForce_SetBin(pPlayer, i, jkCoopForce_aStory[i].amount, (uint32_t)jkCoopForce_aStory[i].state);
        stdPlatform_Printf("OpenJKDF2: co-op, a level with cheats: the story's Force kept\n");
    }
    jkCoopForce_BetweenLevels();
}

void jkCoopForce_HostEndingLevel(void)
{
    // (the secrets' last count before the guests' levels end, which give their star by it)
    if (jkCoop_IsActive() && sithNet_isServer && jkCoopForce_bStory && jkCoopForce_LocalPlayer())
        jkCoopForce_SendSecrets(-1);
}

// ------------------------------------------------------------------ a guest's own innocents

// The guest whose kill of an actor is being told to its cogs (the host's), NULL if none
static SithThing* jkCoopForce_pKiller = NULL;

// Whether a cog's script counts innocents killed: its "killed" handler calls ChangeInv with the bin 72
// (the stock innocents' class cogs: ChangeInv(player, 72, ...) if the local player killed them). Found
// in the script's code once, from the handler's start to the next handler's
#define JKCOOPFORCE_MAX_SCRIPTS 64
static SithCogScript* jkCoopForce_apScript[JKCOOPFORCE_MAX_SCRIPTS];
static uint8_t jkCoopForce_aCounts[JKCOOPFORCE_MAX_SCRIPTS];
static int jkCoopForce_numScripts = 0;

static int jkCoopForce_ScriptCountsInnocents(sithCog* pCog)
{
    const SithCogScript* pScript = pCog->pScript;
    if (!pScript->pCode)
        return 0;
    uint32_t start = 0xFFFFFFFF, end = pScript->codeSize;
    for (uint32_t i = 0; i < pScript->numHandlers; i++)
        if (pScript->aHandlers[i].trigId == SITH_MESSAGE_KILLED)
            start = pScript->aHandlers[i].trigPc;
    if (start >= end)
        return 0;
    for (uint32_t i = 0; i < pScript->numHandlers; i++)
        if (pScript->aHandlers[i].trigPc > start && pScript->aHandlers[i].trigPc < end)
            end = pScript->aHandlers[i].trigPc;
    const int32_t* pCode = pScript->pCode;
    int b72 = 0;        // (72 pushed since the last call)
    int lastSym = -1;   // (the symbol pushed just before, a call's function)
    for (uint32_t pc = start; pc < end; ) {
        const int32_t op = pCode[pc++];
        const int bWasSym = lastSym >= 0;
        int32_t arg = 0;
        if (op == COG_OPCODE_PUSHINT || op == COG_OPCODE_PUSHFLOAT || op == COG_OPCODE_PUSHSYMBOL || op == COG_OPCODE_GOFALSE
            || op == COG_OPCODE_GOTRUE || op == COG_OPCODE_GO || op == COG_OPCODE_CALL) {
            if (pc >= end)
                break;
            arg = pCode[pc++];
        }
        else if (op == COG_OPCODE_PUSHVECTOR)
            pc += 3;
        lastSym = -1;
        if (op == COG_OPCODE_PUSHINT && arg == SITHBIN_PEDS_KILLED)
            b72 = 1;
        else if (op == COG_OPCODE_PUSHFLOAT) {
            float f;
            memcpy(&f, &arg, 4);
            if (f == (float)SITHBIN_PEDS_KILLED)
                b72 = 1;
        }
        else if (op == COG_OPCODE_PUSHSYMBOL)
            lastSym = arg;
        else if (op == COG_OPCODE_CALLFUNC) {
            if (b72 && bWasSym) {
                SithCogSymbol* pSym = sithCogParse_GetSymbolByID(pCog->pSymbolTable, (unsigned int)pCode[pc - 2]);
                if (pSym && pSym->val.type == SITHCOG_VALUE_POINTER && pSym->val.dataAsFunc == sithCogFunctionThing_ChangeInventory)
                    return 1;
            }
            b72 = 0;
        }
    }
    return 0;
}

static int jkCoopForce_CountsInnocents(sithCog* pCog)
{
    if (!pCog || !pCog->pScript || !(pCog->flags & SITH_COG_CLASS))
        return 0;
    for (int i = 0; i < jkCoopForce_numScripts; i++)
        if (jkCoopForce_apScript[i] == pCog->pScript)
            return jkCoopForce_aCounts[i];
    const int bCounts = jkCoopForce_ScriptCountsInnocents(pCog);
    if (jkCoopForce_numScripts < JKCOOPFORCE_MAX_SCRIPTS) {
        jkCoopForce_apScript[jkCoopForce_numScripts] = pCog->pScript;
        jkCoopForce_aCounts[jkCoopForce_numScripts++] = (uint8_t)bCounts;
    }
    return bCounts;
}

SithThing* jkCoopForce_KilledBegin(SithThing* pKiller)
{
    SithThing* pBefore = jkCoopForce_pKiller;
    SithThing* pMe = jkCoopForce_LocalPlayer();
    jkCoopForce_pKiller = NULL;
    if (jkCoop_IsActive() && sithNet_isServer && jkCoopForce_bStory && pMe && pKiller && pKiller != pMe
        && pKiller->type == SITH_THING_PLAYER && pKiller->actorParams.pPlayer)
        jkCoopForce_pKiller = pKiller;
    return pBefore;
}

void jkCoopForce_KilledEnd(SithThing* pBefore)
{
    jkCoopForce_pKiller = pBefore;
}

SithThing* jkCoopForce_LocalPlayerFor(sithCog* pCog)
{
    if (!jkCoopForce_pKiller || !jkCoopForce_CountsInnocents(pCog))
        return NULL;
    return jkCoopForce_pKiller;
}

void jkCoopForce_CogSetsOtherForce(sithCog* pCog, SithThing* pThing, int bin, flex_t amount, int bChange)
{
    if (!jkCoopForce_pKiller || pThing != jkCoopForce_pKiller || !jkCoopForce_CountsInnocents(pCog))
        return;
    const SithInventoryType* pType = &sithInventory_g_aTypes[bin];
    if (amount < pType->min) amount = pType->min;
    if (amount > pType->max) amount = pType->max;
    const flex_t now = pThing->actorParams.pPlayer->aItems[bin].amount;
    stdPlatform_Printf("OpenJKDF2: co-op, an innocent killed by a guest, theirs (%d)\n", bin);
    jkCoopForce_SendGive((int)pThing->actorParams.pPlayer->playerNetId, bin, bChange, bChange ? amount - now : amount);
}

// ------------------------------------------------------------------ a guest's kit, at the host

// The stars a Force earned (unspent, new, and those in the powers; in Mysteries of the Sith Defense's
// count twice, and Jedi Knight's Protection and Deadly Sight are free, by the side chosen)
static int jkCoopForce_StarsEarned(const jkCoopKitBin aKit[SITHBIN_NUMBINS])
{
    int stars = (int)aKit[SITHBIN_SPEND_STARS].amount + (int)aKit[SITHBIN_NEW_STARS].amount;
    for (int i = SITHBIN_FP_START; i <= SITHBIN_FP_END; i++) {
        if (i == SITHBIN_JEDI_RANK || (!Main_bMotsCompat && (i == SITHBIN_F_PROTECTION || i == SITHBIN_F_DEADLYSIGHT)))
            continue;
        stars += (int)aKit[i].amount * ((Main_bMotsCompat && i == SITHBIN_F_DEFENSE) ? 2 : 1);
    }
    return stars;
}

int jkCoopForce_CatchUp(jkCoopKitBin aKit[SITHBIN_NUMBINS])
{
    int bCaughtUp = 0;
    jkCoopKitBin aHost[SITHBIN_NUMBINS];
    if (jkCoop_IsActive() && sithNet_isServer && jkCoop_GetKit(aHost)) {
        // A player new to the campaign: the host's rank, the powers the story has offered it (its side,
        // after the path's choice), and its stars earned to spend
        const int stars = jkCoopForce_StarsEarned(aHost);
        const flex_t rank = aHost[SITHBIN_JEDI_RANK].amount;
        int bFound = 0;
        for (int i = SITHBIN_FP_START; i <= SITHBIN_FP_END; i++) {
            if (i != SITHBIN_JEDI_RANK && (aHost[i].state & SITHINVENTORY_ITEM_FOUND)) {
                aKit[i].state |= SITHINVENTORY_ITEM_FOUND;
                bFound = 1;
            }
        }
        if (rank > aKit[SITHBIN_JEDI_RANK].amount)
            aKit[SITHBIN_JEDI_RANK].amount = rank;
        if ((flex_t)stars > aKit[SITHBIN_SPEND_STARS].amount)
            aKit[SITHBIN_SPEND_STARS].amount = (flex_t)stars;
        if (rank * 50.0 > aKit[SITHBIN_FORCEMANA].amount)
            aKit[SITHBIN_FORCEMANA].amount = rank * 50.0;
        const int choice = (int)aHost[SITHBIN_CHOICE].amount;
        if (!Main_bMotsCompat && (choice == 1 || choice == 2)) {
            aKit[SITHBIN_CHOICE].amount = (flex_t)choice;
            aKit[SITHBIN_ALIGNMENT].amount = aHost[SITHBIN_ALIGNMENT].amount;
        }
        bCaughtUp = rank > 0.0 || stars > 0 || bFound;
        if (bCaughtUp)
            stdPlatform_Printf("OpenJKDF2: co-op, a new player caught up: rank %d, %d stars\n", (int)rank, stars);
    }
    return bCaughtUp;
}

void jkCoopForce_TellGuest(int idTo)
{
    if (!jkCoop_IsActive() || !sithNet_isServer)
        return;
    jkCoopForce_SendStart(idTo);
    if (jkCoopForce_bStory && jkCoopForce_LocalPlayer())
        jkCoopForce_SendSecrets(idTo);
}

// ------------------------------------------------------------------ the game's side

void jkCoopForce_SessionEnded(void)
{
    jkCoopForce_bStory = 0;
    jkCoopForce_bInStart = 0;
    jkCoopForce_bEnded = 0;
    jkCoopForce_numStart = 0;
    jkCoopForce_bStartDone = 0;
    jkCoopForce_aStartLevel[0] = 0;
}

void jkCoopForce_Startup(void)
{
    jkNetExt_Register(JKNETEXT_COOPFORCE, jkCoopForce_Arrived);
}

#endif // QOL_IMPROVEMENTS
