// Added: entire file (iosSaberThrowJk.h)

#ifdef JK_ENHANCED

#include "Enhanced/iosSaberThrowJk.h"
#include "Enhanced/iosSaber.h"

#include "Cog/sithCog.h"
#include "Devices/sithSound.h"
#include "Devices/sithSoundMixer.h"
#include "Dss/sithMulti.h"
#include "Engine/sithPuppet.h"
#include "Gameplay/jkSaber.h"
#include "Gameplay/sithEvent.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithTime.h"
#include "General/stdMath.h"
#include "Main/Main.h"
#include "Main/jkDev.h"
#include "World/jkPlayer.h"
#include "World/sithTemplate.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithWorld.h"
#include "jk.h"

#include <string.h>

#define IOSSABERTHROWJK_BIN_SABER SITHBIN_LIGHTSABER
#define IOSSABERTHROWJK_BIN_FISTS SITHBIN_FISTS
#define IOSSABERTHROWJK_POWER_FLAGS (SITHINVENTORY_TYPE_AUTOAIM | ITEMINFO_HOTKEYABLE) // (items.dat's 0x108, as the game's powers have)

// The thrown saber: the level's own (Jedi Knight's cut one), flown as Mysteries
// of the Sith's +force_saber is (its vel, timer, angvel, damage, light),
// striking with the game's saber sparks. (Its size stays Jedi Knight's, a
// bolt's: Mysteries of the Sith's, ten times that, strikes what Kyle stands
// beside -- level 4's first table -- the moment it is thrown.)
#define IOSSABERTHROWJK_TEMPLATE "+force_saber"
#define IOSSABERTHROWJK_SPARKS_WALL "+ssparks_wall"
#define IOSSABERTHROWJK_SPARKS_FLESH "+ssparks_blood"
#define IOSSABERTHROWJK_SPEED 1.1f        // (units a second)
#define IOSSABERTHROWJK_OUT_SECS 2.0      // (out this long, if it strikes nothing)
#define IOSSABERTHROWJK_SPIN -400.0f      // (degrees a second, round its up axis)
#define IOSSABERTHROWJK_DAMAGE 80.0f
#define IOSSABERTHROWJK_LIGHT 0.5f
#define IOSSABERTHROWJK_WEAPON_FLAGS (SITH_WF_NOSHOOTERDAMAGE | SITH_WF_FACEHITEXPLODE | SITH_WF_THINGHITEXPLODE)
// Back: as fast as Mysteries of the Sith's returning saber, and caught this near
// Kyle; 5 s after the throw (the game's own give-back) it is back however it went
#define IOSSABERTHROWJK_BACK_SPEED 3.0f
#define IOSSABERTHROWJK_CATCH_DIST 0.15f
#define IOSSABERTHROWJK_BACK_SECS 5.0
// Two sabers: the left one at least this long after the right (as iosSaberThrow's),
// from a little to the left, its path turned a little
#define IOSSABERTHROWJK_STAGGER 0.25
#define IOSSABERTHROWJK_LEFT_OFFSET -0.03f
#define IOSSABERTHROWJK_TURN_DEG 6.0f
// The game's own sounds: a swing as it is thrown, the hum as it flies
#define IOSSABERTHROWJK_THROW_SOUND "LtSaberSwing01.wav"
#define IOSSABERTHROWJK_HUM_SOUND "LtSaberLp01.wav"
// The saber cog's timers (weap_saber.cog): 0 lights the blade once it is drawn, 1
// puts its autoblock back on after a swing
#define IOSSABERTHROWJK_SABER_TIMERS 2
#define IOSSABERTHROWJK_AUTOBLOCK 0x2000 // (the actor flag the saber cog sets and clears)

// Mysteries of the Sith's prices, by the stars
static const flex_t iosSaberThrowJk_aCost[5] = { 0.0, 100.0, 80.0, 60.0, 40.0 };

// Where each saber is
enum {
    IOSSABERTHROWJK_IN_HAND = 0,
    IOSSABERTHROWJK_OUT,  // the thrown saber on its way out
    IOSSABERTHROWJK_BACK, // on its way back
};

typedef struct {
    int state;
    int bDual;          // thrown from two sabers (else the one saber, and none is left in hand)
    int thingIdx;       // the saber flying
    uint32_t signature; // (its, so another thing in its place isn't taken for it)
    rdVector3 lastPos;  // where it was last seen
    SithSector* pLastSector;
    double secThrown;
    int colour;         // the SABER wheel's id it was thrown in
    int style;          // and the style (iosSaber's)
} iosSaberThrowJkHand;

static int iosSaberThrowJk_bOn = 0;          // THROW
static int iosSaberThrowJk_bRegistered = 0;  // the power registered by this (not by a mod's items)
static int iosSaberThrowJk_lastOtherPower = -1;
static iosSaberThrowJkHand iosSaberThrowJk_aHand[2]; // 0: the right (or the one saber), 1: the left
static double iosSaberThrowJk_secLast = -100.0;      // the last throw
static int iosSaberThrowJk_pendingHand = -1;         // a second saber due (pressed again within the stagger)
static double iosSaberThrowJk_secPending = 0.0;
static SithThing* iosSaberThrowJk_pPlayer = NULL;    // the player it is all for (another: everything back)
static int iosSaberThrowJk_bSweep = 0;               // after a reset: a loaded game's sabers of ours taken away
static SithWorld* iosSaberThrowJk_pSoundWorld = NULL;
static sithSound* iosSaberThrowJk_pThrowSound = NULL;
static sithSound* iosSaberThrowJk_pHumSound = NULL;

int iosSaberThrowJk_Get(void)
{
    return iosSaberThrowJk_bOn;
}

void iosSaberThrowJk_Set(int bOn)
{
    iosSaberThrowJk_bOn = bOn ? 1 : 0;
}

void iosSaberThrowJk_Toggle(void)
{
    iosSaberThrowJk_Set(!iosSaberThrowJk_bOn);
}

// Whether a mod's own items have a bin 37: then it is theirs, and none of this
static int iosSaberThrowJk_Foreign(void)
{
    return !iosSaberThrowJk_bRegistered && (sithInventory_g_aTypes[IOSSABERTHROWJK_BIN].flags & SITHINVENTORY_TYPE_REGISTERED);
}

int iosSaberThrowJk_Offered(void)
{
    return !Main_bMotsCompat && !sithNet_isMulti && !iosSaberThrowJk_Foreign();
}

int iosSaberThrowJk_OnWheel(void)
{
    return iosSaberThrowJk_Offered() && (iosSaberThrowJk_bOn || jkDev_Custom_SithEwokIsOn());
}

// The local player in a Jedi Knight level, single player; NULL elsewhere
static SithThing* iosSaberThrowJk_Player(void)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    if (Main_bMotsCompat || sithNet_isMulti || !sithWorld_g_pCurrentWorld || !p || p->type != SITH_THING_PLAYER
        || !p->actorParams.pPlayer || p->actorParams.pPlayer == (SithPlayer*)-136)
        return NULL;
    return p;
}

// ------------------------------------------------------------ the power

static void iosSaberThrowJk_Register(void)
{
    if (iosSaberThrowJk_bRegistered || (sithInventory_g_aTypes[IOSSABERTHROWJK_BIN].flags & SITHINVENTORY_TYPE_REGISTERED)) return;
    sithInventory_RegisterType(IOSSABERTHROWJK_BIN, NULL, "f_saberthrow", 0.0, 4.0, IOSSABERTHROWJK_POWER_FLAGS);
    iosSaberThrowJk_bRegistered = 1;
}

static void iosSaberThrowJk_Unregister(void)
{
    if (!iosSaberThrowJk_bRegistered) return;
    SithInventoryType* t = &sithInventory_g_aTypes[IOSSABERTHROWJK_BIN];
    t->flags = 0;
    t->cog = NULL;
    t->min = t->max = 0.0;
    iosSaberThrowJk_bRegistered = 0;
}

void iosSaberThrowJk_ItemsLoading(void)
{
    iosSaberThrowJk_Unregister();
}

// Its stars now: Kyle's Jedi rank halved, rounded up (4 with the cheat)
static int iosSaberThrowJk_Stars(SithThing* p)
{
    if (jkDev_Custom_SithEwokIsOn()) return 4;
    const int rank = (int)sithInventory_GetInventory(p, SITHBIN_JEDI_RANK);
    const int stars = (rank + 1) / 2;
    return stars < 0 ? 0 : (stars > 4 ? 4 : stars);
}

// Whether the player has a lightsaber
static int iosSaberThrowJk_HasSaber(SithThing* p)
{
    return sithInventory_GetInventory(p, IOSSABERTHROWJK_BIN_SABER) > 0.0 && sithInventory_IsInventoryAvailable(p, IOSSABERTHROWJK_BIN_SABER);
}

// The power picked before it, picked again (else the next there is; else none)
static void iosSaberThrowJk_PowerBack(SithThing* p)
{
    SithPlayer* pl = p->actorParams.pPlayer;
    int other = iosSaberThrowJk_lastOtherPower;
    if (other <= 0 || other == IOSSABERTHROWJK_BIN || other >= SITHBIN_NUMBINS || !(sithInventory_g_aTypes[other].flags & SITHINVENTORY_TYPE_AUTOAIM)
        || !sithInventory_IsInventoryAvailable(p, other))
        other = sithInventory_FindNextTypeID(p, IOSSABERTHROWJK_BIN, SITHINVENTORY_TYPE_AUTOAIM);
    if (other == IOSSABERTHROWJK_BIN) other = -1;
    if (other > 0) sithInventory_SelectPower(p, other);
    else pl->curPower = 0; // (none, as at the start of the game)
}

// The power as THROW and the cheat say: on the wheel (registered) or not; usable
// (available: picked, used) with the saber and a star; its stars
static void iosSaberThrowJk_SyncPower(SithThing* p)
{
    SithPlayer* pl = p->actorParams.pPlayer;
    if (pl->curPower > 0 && pl->curPower != IOSSABERTHROWJK_BIN) iosSaberThrowJk_lastOtherPower = pl->curPower; // (the one to go back to)
    const int bShown = iosSaberThrowJk_OnWheel();
    if (bShown) iosSaberThrowJk_Register();
    if (!iosSaberThrowJk_bRegistered) {
        // (not on the wheel: not picked either, as in a game saved with it picked)
        if (pl->curPower == IOSSABERTHROWJK_BIN && !iosSaberThrowJk_Foreign()) iosSaberThrowJk_PowerBack(p);
        return;
    }
    const int stars = bShown ? iosSaberThrowJk_Stars(p) : 0;
    const int bUsable = bShown && stars > 0 && iosSaberThrowJk_HasSaber(p);
    SithInventoryItem* it = &pl->aItems[IOSSABERTHROWJK_BIN];
    if (it->amount != (flex_t)stars) it->amount = (flex_t)stars;
    if (bUsable) it->state |= SITHINVENTORY_ITEM_AVAILABLE;
    else it->state &= ~(SITHINVENTORY_ITEM_AVAILABLE | SITHINVENTORY_ITEM_ACTIVATED);
    if (pl->curPower == IOSSABERTHROWJK_BIN && !bUsable) iosSaberThrowJk_PowerBack(p);
    if (!bShown) iosSaberThrowJk_Unregister();
}

// ------------------------------------------------------------ the sabers

static void iosSaberThrowJk_Sounds(void)
{
    if (iosSaberThrowJk_pSoundWorld == sithWorld_g_pCurrentWorld) return;
    iosSaberThrowJk_pSoundWorld = sithWorld_g_pCurrentWorld;
    iosSaberThrowJk_pThrowSound = sithSound_Load(IOSSABERTHROWJK_THROW_SOUND, 0);
    iosSaberThrowJk_pHumSound = sithSound_Load(IOSSABERTHROWJK_HUM_SOUND, 0);
}

int iosSaberThrowJk_HandsInHand(void)
{
    if (!iosSaberThrowJk_Player()) return 3;
    int mask = 0;
    for (int k = 0; k < 2; k++) {
        const iosSaberThrowJkHand* h = &iosSaberThrowJk_aHand[k];
        if (h->state == IOSSABERTHROWJK_IN_HAND) mask |= 1 << k;
        else if (!h->bDual) return 0; // (the one saber out: none in hand)
    }
    return mask;
}

static int iosSaberThrowJk_AnyOut(void)
{
    return iosSaberThrowJk_HandsInHand() != 3;
}

static void iosSaberThrowJk_Forget(void)
{
    memset(iosSaberThrowJk_aHand, 0, sizeof(iosSaberThrowJk_aHand));
    iosSaberThrowJk_aHand[0].thingIdx = iosSaberThrowJk_aHand[1].thingIdx = -1;
    iosSaberThrowJk_pendingHand = -1;
    iosSaberThrowJk_secLast = -100.0;
}

void iosSaberThrowJk_Reset(void)
{
    iosSaberThrowJk_Forget();
    iosSaberThrowJk_pPlayer = NULL;
    iosSaberThrowJk_bSweep = 1;
}

// A saber of ours still there (the same one, not another made in its place)
static SithThing* iosSaberThrowJk_Thing(const iosSaberThrowJkHand* h)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    if (!w || h->state == IOSSABERTHROWJK_IN_HAND || h->thingIdx < 0 || h->thingIdx > w->numThings) return NULL;
    SithThing* t = &w->aThings[h->thingIdx];
    if (!t->type || t->signature != h->signature || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED))) return NULL;
    return t;
}

int iosSaberThrowJk_WeaponHeldBack(SithThing* pThing, int bin)
{
    return bin == IOSSABERTHROWJK_BIN_SABER && pThing && pThing == iosSaberThrowJk_Player() && iosSaberThrowJk_HandsInHand() == 0;
}

// The saber cog's timers (lighting the blade once drawn, the autoblock back on)
// dropped, as its put-away's own would have them
static void iosSaberThrowJk_KillSaberTimers(void)
{
    sithCog* pCog = sithInventory_GetType(IOSSABERTHROWJK_BIN_SABER) ? sithInventory_GetType(IOSSABERTHROWJK_BIN_SABER)->cog : NULL;
    if (!pCog) return;
    SithEvent* prev = NULL;
    SithEvent* e = sithEvent_g_pFirstQueuedEvent;
    while (e) {
        SithEvent* next = e->pNextEvent;
        if (e->taskNum == 4 && e->params.idx == pCog->idx && e->params.timerIdx >= 0 && e->params.timerIdx < IOSSABERTHROWJK_SABER_TIMERS) {
            if (prev) prev->pNextEvent = next;
            else sithEvent_g_pFirstQueuedEvent = next;
            sithEvent_FreeEvent(e);
        }
        else prev = e;
        e = next;
    }
}

// No saber left in hand: as Mysteries of the Sith's throw leaves Mara, the fists
// in hand at once, without the saber's put-away (its blade put out, its sound and
// moves): what the put-away would tidy, tidied here -- the saber's first-person
// moves stopped, its hum faded, its autoblock and blade's damage off, its
// timers dropped. The saber stays in the inventory.
static void iosSaberThrowJk_PutAway(SithThing* p)
{
    rdPuppet* pPov = p->playerInfo ? p->playerInfo->povModel.puppet : NULL;
    for (int k = 0; pPov && k < 4; k++) sithPuppet_StopKey(pPov, k, 0.0);
    if (iosSaberThrowJk_pHumSound) {
        sithPlayingSound* s = sithSoundMixer_GetSoundFromIdx(sithSoundMixer_GetThingSoundIdx(p, iosSaberThrowJk_pHumSound));
        if (s && (s->sound || s->pSoundBuf)) {
            sithSoundMixer_FadeVolume(s, 0.0, 0.5);
            s->flags |= SITHSOUNDFLAG_FADING;
        }
    }
    iosSaberThrowJk_KillSaberTimers();
    p->actorParams.flags &= ~IOSSABERTHROWJK_AUTOBLOCK;
    jkSaber_Disable(p);
    p->jkFlags &= ~(JKFLAG_SABERON | JKFLAG_SABERDAMAGE | JKFLAG_SABEREXTEND | JKFLAG_SABERRETRACT);
    sithWeapon_SetFireWait(p, -1.0);
    sithWeapon_SetMountWait(p, 0.0);
    sithInventory_SetCurrentWeapon(p, 0);
    sithWeapon_SelectWeapon(p, IOSSABERTHROWJK_BIN_FISTS, 0);
}

// The saber flying: harmless (on its way back: no damage, nothing made where
// it touches), alive until it is caught
static void iosSaberThrowJk_Harmless(SithThing* t)
{
    t->weaponParams.damage = 0.0;
    t->weaponParams.pExplosionTemplate = NULL;
    t->weaponParams.fleshHitTemplate = NULL;
    t->weaponParams.flags = SITH_WF_NOSHOOTERDAMAGE;
    t->msecLifeLeft = (int32_t)((IOSSABERTHROWJK_BACK_SECS + 1.0) * 1000.0);
}

static void iosSaberThrowJk_Hum(SithThing* t)
{
    if (iosSaberThrowJk_pHumSound)
        sithSoundMixer_PlaySoundThing(iosSaberThrowJk_pHumSound, t, 1.0, 0.5, 2.5, SITHSOUNDFLAG_LOOP | SITHSOUNDFLAG_FOLLOWSTHING);
}

// The rules for a throw now (Mysteries of the Sith's): alive; the saber in hand,
// not being drawn or put away, nor another weapon on its way; the power usable,
// with Force enough or a full meter
static int iosSaberThrowJk_CanThrow(SithThing* p)
{
    SithPlayer* pl = p->actorParams.pPlayer;
    if ((p->flags & SITH_TF_DEAD) || p->actorParams.health <= 0.0) return 0;
    if (pl->curWeaponID != IOSSABERTHROWJK_BIN_SABER || sithWeapon_8BD024 != -1 || sithTime_g_secGameTime < sithWeapon_secMountWait) return 0;
    if (!iosSaberThrowJk_HasSaber(p) || !sithInventory_IsInventoryAvailable(p, IOSSABERTHROWJK_BIN)) return 0;
    const int stars = (int)sithInventory_GetInventory(p, IOSSABERTHROWJK_BIN);
    if (stars < 1 || stars > 4) return 0;
    if (!sithTemplate_GetTemplate(IOSSABERTHROWJK_TEMPLATE)) return 0; // (a mod's level without one)
    const flex_t mana = sithInventory_GetInventory(p, SITHBIN_FORCEMANA);
    const flex_t maxMana = sithInventory_GetInventory(p, SITHBIN_JEDI_RANK) * (flex_t)50.0; // (force_well.cog's top)
    if (mana < iosSaberThrowJk_aCost[stars] && mana < maxMana) return 0;
    return 1;
}

// Kyle's throwing move: his "magic" one (Mysteries of the Sith's throw), or the
// toss where his model has no such move now
static int iosSaberThrowJk_Move(SithThing* p)
{
    SithPuppetClass* c = p->pPuppetClass;
    if (!c || !p->puppet || p->puppet->majorMode < 0) return -1;
    if (c->modes[p->puppet->majorMode].keyframe[SITH_ANIM_MAGIC].keyframe) return SITH_ANIM_MAGIC;
    if (c->modes[p->puppet->majorMode].keyframe[SITH_ANIM_TOSS].keyframe) return SITH_ANIM_TOSS;
    return -1;
}

// Throws this hand's saber (bDual: one of two), by the rules (checked first)
static int iosSaberThrowJk_Throw(SithThing* p, int hand, int bDual)
{
    if (!iosSaberThrowJk_CanThrow(p)) return 0;
    SithThing* pTpl = sithTemplate_GetTemplate(IOSSABERTHROWJK_TEMPLATE);
    iosSaberThrowJk_Sounds();

    // (the left saber from a little to the left, its path turned a little)
    rdVector3 offset = { hand ? IOSSABERTHROWJK_LEFT_OFFSET : 0.0f, 0.0f, 0.0f }, zero = { 0.0f, 0.0f, 0.0f };
    SithThing* t = sithWeapon_FireProjectile(p, pTpl, iosSaberThrowJk_pThrowSound, iosSaberThrowJk_Move(p), &offset, &zero, 1.0, 0, 10.0, 10.0, 0);
    if (!t) return 0;

    // the cost, unless the game's surges or the cheat's endless Force are on (as for
    // the game's own neutral powers)
    const int stars = (int)sithInventory_GetInventory(p, IOSSABERTHROWJK_BIN);
    if (sithInventory_GetInventory(p, SITHBIN_S_DARK) != 1.0 && sithInventory_GetInventory(p, SITHBIN_S_LIGHT) != 1.0 && !jkDev_Custom_EndlessForceIsOn())
        sithInventory_ChangeInventory(p, SITHBIN_FORCEMANA, -iosSaberThrowJk_aCost[stars]);

    // flown as Mysteries of the Sith's: its speed, spin, size, light and damage; out
    // for its time; sparks where it strikes, never a saber to pick up
    rdVector3* v = &t->physicsParams.vel;
    if (hand) {
        flex_t s, c;
        stdMath_SinCos(IOSSABERTHROWJK_TURN_DEG, &s, &c);
        const flex_t x = v->x * c - v->y * s, y = v->x * s + v->y * c;
        v->x = x;
        v->y = y;
    }
    const flex_t speed = rdVector_Len3(v);
    if (speed > 0.0) rdVector_Scale3Acc(v, IOSSABERTHROWJK_SPEED / speed);
    t->physicsParams.angularVelocity.x = 0.0;
    t->physicsParams.angularVelocity.y = IOSSABERTHROWJK_SPIN;
    t->physicsParams.angularVelocity.z = 0.0;
    if (t->flags & SITH_TF_EMITLIGHT) t->light = IOSSABERTHROWJK_LIGHT;
    t->weaponParams.damage = IOSSABERTHROWJK_DAMAGE;
    t->weaponParams.pExplosionTemplate = sithTemplate_GetTemplate(IOSSABERTHROWJK_SPARKS_WALL);
    t->weaponParams.fleshHitTemplate = sithTemplate_GetTemplate(IOSSABERTHROWJK_SPARKS_FLESH);
    t->weaponParams.flags = IOSSABERTHROWJK_WEAPON_FLAGS;
    t->msecLifeLeft = (int32_t)((IOSSABERTHROWJK_BACK_SECS + 1.0) * 1000.0);
    iosSaberThrowJk_Hum(t);

    iosSaberThrowJkHand* h = &iosSaberThrowJk_aHand[hand];
    const int style = iosSaber_GetStyle();
    h->state = IOSSABERTHROWJK_OUT;
    h->bDual = bDual;
    h->thingIdx = t->idx;
    h->signature = t->signature;
    h->lastPos = t->position;
    h->pLastSector = t->sector;
    h->secThrown = sithTime_g_secGameTime;
    h->style = bDual ? IOSSABER_STYLE_DUAL : (iosSaber_StyleNumBlades(style) == 1 ? style : IOSSABER_STYLE_SINGLE);
    h->colour = iosSaber_GetShownColour(h->style, bDual ? hand : 0);
    iosSaberThrowJk_secLast = sithTime_g_secGameTime;
    iosSaberThrowJk_pPlayer = p;

    if (iosSaberThrowJk_HandsInHand() == 0) iosSaberThrowJk_PutAway(p);
    return 1;
}

static int iosSaberThrowJk_IsDual(void)
{
    return iosSaber_GetStyle() == IOSSABER_STYLE_DUAL && iosSaber_StyleAvailable(IOSSABER_STYLE_DUAL);
}

int iosSaberThrowJk_Activate(SithThing* pPlayer)
{
    SithThing* p = iosSaberThrowJk_Player();
    if (!p || pPlayer != p || !iosSaberThrowJk_bRegistered) return 0;
    const int mask = iosSaberThrowJk_HandsInHand();
    if (!mask || iosSaberThrowJk_pendingHand >= 0) return 1;
    if (!iosSaberThrowJk_IsDual()) {
        // the one saber in hand (that of two still in hand, the style changed meanwhile)
        iosSaberThrowJk_Throw(p, (mask & 1) ? 0 : 1, 0);
        return 1;
    }
    // two: the right, then the left, never more than one due
    const int hand = (mask & 1) ? 0 : 1;
    if (sithTime_g_secGameTime - iosSaberThrowJk_secLast < IOSSABERTHROWJK_STAGGER) {
        if (iosSaberThrowJk_CanThrow(p)) {
            iosSaberThrowJk_pendingHand = hand;
            iosSaberThrowJk_secPending = iosSaberThrowJk_secLast + IOSSABERTHROWJK_STAGGER;
        }
        return 1;
    }
    iosSaberThrowJk_Throw(p, hand, 1);
    return 1;
}

// A saber back in its hand: in hand again from the fists (if it was all he had)
static void iosSaberThrowJk_Back(SithThing* p, int hand)
{
    iosSaberThrowJkHand* h = &iosSaberThrowJk_aHand[hand];
    SithThing* t = iosSaberThrowJk_Thing(h);
    if (t) sithThing_DestroyThing(t);
    h->state = IOSSABERTHROWJK_IN_HAND;
    h->thingIdx = -1;
    if (iosSaberThrowJk_pendingHand == hand) iosSaberThrowJk_pendingHand = -1;
    SithPlayer* pl = p->actorParams.pPlayer;
    if (iosSaberThrowJk_HandsInHand() && pl->curWeaponID == IOSSABERTHROWJK_BIN_FISTS && sithWeapon_8BD024 == -1 && iosSaberThrowJk_HasSaber(p)
        && !(p->flags & SITH_TF_DEAD))
        sithWeapon_SelectWeapon(p, IOSSABERTHROWJK_BIN_SABER, 0);
}

// After a reset: sabers of ours a loaded game had flying, taken away, the saber
// in hand again if the game was saved with it out
static void iosSaberThrowJk_Sweep(SithThing* p)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    SithThing* pTpl = sithTemplate_GetTemplate(IOSSABERTHROWJK_TEMPLATE);
    int n = 0;
    for (int i = 0; pTpl && i <= w->numThings; i++) {
        SithThing* t = &w->aThings[i];
        if (!t->type || t->pTemplate != pTpl || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED)) || (t->pParent && t->pParent != p)) continue;
        sithThing_DestroyThing(t);
        n++;
    }
    SithPlayer* pl = p->actorParams.pPlayer;
    if (n && pl->curWeaponID == IOSSABERTHROWJK_BIN_FISTS && sithWeapon_8BD024 == -1 && iosSaberThrowJk_HasSaber(p))
        sithWeapon_SelectWeapon(p, IOSSABERTHROWJK_BIN_SABER, 0);
}

// One saber on its way: out until it strikes or has flown its time, then back
// to Kyle, caught near him; back however it went 5 s after the throw
static void iosSaberThrowJk_Fly(SithThing* p, int hand, double now)
{
    iosSaberThrowJkHand* h = &iosSaberThrowJk_aHand[hand];
    SithThing* t = iosSaberThrowJk_Thing(h);
    if (now - h->secThrown >= IOSSABERTHROWJK_BACK_SECS) {
        iosSaberThrowJk_Back(p, hand);
        return;
    }
    if (h->state == IOSSABERTHROWJK_OUT) {
        if (t && now - h->secThrown < IOSSABERTHROWJK_OUT_SECS) {
            h->lastPos = t->position;
            h->pLastSector = t->sector;
            return;
        }
        if (t) {
            // flown its time: it turns back
            iosSaberThrowJk_Harmless(t);
        }
        else {
            // it struck (its sparks there): it comes back from where it was last seen
            SithThing* pTpl = sithTemplate_GetTemplate(IOSSABERTHROWJK_TEMPLATE);
            if (pTpl && h->pLastSector) {
                rdVector3 dir;
                rdVector_Sub3(&dir, &p->position, &h->lastPos);
                if (rdVector_Normalize3Acc(&dir) <= 0.0) dir = p->orient.lvec;
                rdMatrix34 m;
                rdMatrix_BuildFromLook34(&m, &dir);
                t = sithThing_CreateThingAtPos(pTpl, &h->lastPos, &m, h->pLastSector, p);
            }
            if (!t) {
                iosSaberThrowJk_Back(p, hand);
                return;
            }
            iosSaberThrowJk_Harmless(t);
            t->physicsParams.angularVelocity.x = t->physicsParams.angularVelocity.z = 0.0;
            t->physicsParams.angularVelocity.y = IOSSABERTHROWJK_SPIN;
            if (t->flags & SITH_TF_EMITLIGHT) t->light = IOSSABERTHROWJK_LIGHT;
            iosSaberThrowJk_Sounds();
            iosSaberThrowJk_Hum(t);
            h->thingIdx = t->idx;
            h->signature = t->signature;
        }
        h->state = IOSSABERTHROWJK_BACK;
    }
    if (!t) return; // (gone on its way back: given back at the time)
    h->lastPos = t->position;
    h->pLastSector = t->sector;
    rdVector3 to;
    rdVector_Sub3(&to, &p->position, &t->position);
    const flex_t dist = rdVector_Len3(&to);
    if (dist <= IOSSABERTHROWJK_CATCH_DIST) {
        iosSaberThrowJk_Back(p, hand);
        return;
    }
    rdVector_Scale3(&t->physicsParams.vel, &to, IOSSABERTHROWJK_BACK_SPEED / dist);
}

void iosSaberThrowJk_Tick(void)
{
    SithThing* p = iosSaberThrowJk_Player();
    if (!p) {
        // no level, multiplayer, the other game: nothing out, the power not ours to keep
        if (iosSaberThrowJk_AnyOut() || iosSaberThrowJk_pendingHand >= 0 || iosSaberThrowJk_pPlayer) iosSaberThrowJk_Forget();
        iosSaberThrowJk_pPlayer = NULL;
        if (Main_bMotsCompat || sithNet_isMulti) iosSaberThrowJk_Unregister();
        return;
    }
    if (iosSaberThrowJk_Foreign()) return;
    if (iosSaberThrowJk_pPlayer && p != iosSaberThrowJk_pPlayer) iosSaberThrowJk_Reset(); // (another player: a new level)
    iosSaberThrowJk_pPlayer = p;
    if (iosSaberThrowJk_bSweep) {
        iosSaberThrowJk_bSweep = 0;
        iosSaberThrowJk_Sweep(p);
    }
    iosSaberThrowJk_SyncPower(p);

    const double now = sithTime_g_secGameTime;
    if (now < iosSaberThrowJk_secLast) { // (the clock went back: a load)
        iosSaberThrowJk_Reset();
        return;
    }
    // the second saber due: thrown if it still can be (else not at all)
    if (iosSaberThrowJk_pendingHand >= 0 && now >= iosSaberThrowJk_secPending) {
        const int hand = iosSaberThrowJk_pendingHand;
        iosSaberThrowJk_pendingHand = -1;
        if (iosSaberThrowJk_IsDual() && iosSaberThrowJk_aHand[hand].state == IOSSABERTHROWJK_IN_HAND) iosSaberThrowJk_Throw(p, hand, 1);
    }
    for (int k = 0; k < 2; k++) {
        if (iosSaberThrowJk_aHand[k].state != IOSSABERTHROWJK_IN_HAND) iosSaberThrowJk_Fly(p, k, now);
    }
    // (no saber in hand: its autoblock stays off, should a timer of the saber's put it on)
    if (iosSaberThrowJk_HandsInHand() == 0 && p->actorParams.pPlayer->curWeaponID != IOSSABERTHROWJK_BIN_SABER)
        p->actorParams.flags &= ~IOSSABERTHROWJK_AUTOBLOCK;
}

// ------------------------------------------------------------ drawing

static const iosSaberThrowJkHand* iosSaberThrowJk_HandOf(SithThing* pThing)
{
    if (!pThing || pThing->type != SITH_THING_WEAPON || !iosSaberThrowJk_Player()) return NULL;
    for (int k = 0; k < 2; k++) {
        const iosSaberThrowJkHand* h = &iosSaberThrowJk_aHand[k];
        if (h->state != IOSSABERTHROWJK_IN_HAND && h->thingIdx == pThing->idx && h->signature == pThing->signature) return h;
    }
    return NULL;
}

int iosSaberThrowJk_ThrownColour(SithThing* pThing)
{
    const iosSaberThrowJkHand* h = iosSaberThrowJk_HandOf(pThing);
    return h ? h->colour : -1;
}

int iosSaberThrowJk_ThrownStyle(SithThing* pThing)
{
    const iosSaberThrowJkHand* h = iosSaberThrowJk_HandOf(pThing);
    return h ? h->style : -1;
}

#endif // JK_ENHANCED
