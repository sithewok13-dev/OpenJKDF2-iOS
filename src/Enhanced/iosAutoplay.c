// Added: entire file. See iosAutoplay.h.
//
// AUTOPLAY, the self-playing show (from Dark Forces' port, made over for
// Jedi Knight and Mysteries of the Sith).
//
// START saves the game ("Before AUTOPLAY", autoplay.jks among the player's
// saves), puts the sithewok cheat on for the show (god mode, every weapon
// and power, ammo and Force kept full, falls caught) without a word, sets
// AUTOPLAY's difficulty and loads a level of the episode (the one picked, or
// the next of a shuffled round, never the one just played) straight in: its
// mission text doesn't wait for a tap, and the episode's cutscenes are never
// on the way (each level loads as its own entry; a level finished, a boss
// down or its exit reached, goes on to the next of the show's own). Its
// enemies are grouped by where they stand (close together, in sight of each
// other) and ranked by size and light; one of the top few is picked at
// random, so each show differs. Kyle (Mara) is placed near it, somewhere
// safe (a level floor with room round him, no pit, water or lift) with a
// clear view of several of them and open space ahead, facing them; the group
// gets copies of its own kinds around it (CROWD, only of kinds the level has
// more than one of: never a second boss), within a cap for the phone's
// speed and the game's own limits; everyone near is woken (ALARM). Once the
// group is down, or no one has been in sight for a while, more of its kinds
// come in from out of his sight; with none to copy he goes after the nearest
// left. Lost (fallen somewhere, stuck, wandered off) he is put back near the
// group. A double tap is a new fight: another level for SHUFFLE ALL, another
// group in this one for PICK ONE. A long press ends it: everything put back
// (the cheat as it was, the difficulty, the saber's look) and the save
// loaded.
//
// The autopilot plays as a person would, through the game's own input: its
// keys go in with the touch overlay's (stdControl), its turning as mouse
// movement (Window_lastXRel), its weapon picks as the weapon wheel's
// (iosGame_SelectWeapon), its powers as the Force wheel's and FORCE's
// (iosGame_SelectPower, the use-force key). The view turns on a spring with
// a moment to react to a new face, the aim wanders a little and settles
// before it fires; it keeps a fighting distance for the weapon in hand with
// varied footwork on an eased stick (the keys pulsed, as a thumb part way
// over), fires in bursts with a short tail, glances about when no one is in
// sight, changes weapon only in lulls, throws thermal detonators at bunches,
// keeps off ledges, pits and water, and when it isn't getting anywhere tries
// other footwork, or turns away and hops. Now and then it uses a Force power
// that fits the moment (push, pull, grip, lightning, destruction, blinding,
// throw, jump, speed: what the game has). With the lightsaber it closes in
// and fights in melee, with varied swings, and now and then throws the saber
// at someone (Saber Throw: Jedi Knight's iosSaberThrowJk, Mysteries of the
// Sith's own) and it comes back. Each fight starts with a new look for the
// saber: one or two, a random hilt each, random colours (the saber wheel's
// RANDOM), kept for the whole fight.
//
// Lengths below are in feet as Dark Forces' were (the show's numbers came
// from there), turned into the level's units by the player's own height
// (s_ft: units a foot, measured as each fight starts); the world's Z is up.

#include "Enhanced/iosAutoplay.h"

#ifdef JK_ENHANCED

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "Enhanced/iosGame.h"
#include "Enhanced/iosSaber.h"
#include "Enhanced/iosSaberWheel.h"
#include "Enhanced/iosSaberThrow.h"
#include "Enhanced/iosSaberThrowJk.h"
#include "AI/sithAI.h"
#include "AI/sithAIAwareness.h"
#include "Dss/sithGamesave.h"
#include "Engine/sithCamera.h"
#include "Engine/sithCollision.h"
#include "Engine/sithIntersect.h"
#include "Engine/sithPhysics.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "General/stdString.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUITitle.h"
#include "Main/jkDev.h"
#include "Main/jkMain.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdVector.h"
#include "Win95/Window.h"
#include "World/jkPlayer.h"
#include "World/sithSurface.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithWorld.h"
#include "stdPlatform.h"
#include "jk.h"

extern int stdControl_bControlsActive;
extern int jkCutscene_isRendering;
extern uint32_t sithAI_maxActors; // (sithAI.c: the enemies' brains still free)

// (the rig's log: the Linux test build, -DIOSAUTOPLAY_RIG)
#ifdef IOSAUTOPLAY_RIG
#define APLOG(...) do { fprintf(stderr, "AUTOPLAY: " __VA_ARGS__); fflush(stderr); } while (0)
#else
#define APLOG(...) do { } while (0)
#endif

#define AP_SAVE_FNAME "autoplay.jks"
#define AP_PI 3.14159265f

// The keys the autopilot presses: the game's defaults, as the touch
// overlay's buttons are (SDL scancodes)
#define AP_KEY_W 26
#define AP_KEY_A 4
#define AP_KEY_S 22
#define AP_KEY_D 7
#define AP_KEY_RUN 225   // left shift
#define AP_KEY_FIRE 224  // left ctrl
#define AP_KEY_ALT 29    // Z
#define AP_KEY_DUCK 6    // C
#define AP_KEY_JUMP 27   // X
#define AP_KEY_FORCE 9   // F
#define AP_NUM_KEYS 512

// In feet (s_ft turns them into the level's units)
#define AP_EYE_FT 5.8f           // the eyes above the floor, Kyle's own
#define AP_ALARM_FT 70.0f        // woken round a group, and counted for CROWD's cap
#define AP_DOWN_FT 55.0f         // a group is down when no one is left this close to where it was
#define AP_CROWD_CAP 24          // CROWD and the waves add copies only while fewer enemies than this are about
#define AP_COPIES_PER_LEVEL 160  // ...and no more than this many in one level (most long dead)
#define AP_AI_SPARE 24           // ...and only while the game has this many enemies' brains to spare
#define AP_MAX_FOES 384

static inline float ap_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float ap_minf(float a, float b) { return a < b ? a : b; }
static inline float ap_maxf(float a, float b) { return a > b ? a : b; }
static inline rdVector3 ap_v3(float x, float y, float z) { rdVector3 v; v.x = x; v.y = y; v.z = z; return v; }
static inline rdVector3 ap_add(rdVector3 a, rdVector3 b) { return ap_v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline rdVector3 ap_sub(rdVector3 a, rdVector3 b) { return ap_v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline rdVector3 ap_mul(rdVector3 a, float k) { return ap_v3(a.x * k, a.y * k, a.z * k); }
static inline float ap_len(rdVector3 a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }
static inline float ap_lenXY(rdVector3 a) { return sqrtf(a.x * a.x + a.y * a.y); }
// Headings: radians, 0 along +Y, growing to the right (toward +X)
static inline float ap_headingTo(rdVector3 from, rdVector3 to) { return atan2f(to.x - from.x, to.y - from.y); }
static inline float ap_wrap(float a)
{
    while (a > AP_PI) a -= 2.0f * AP_PI;
    while (a < -AP_PI) a += 2.0f * AP_PI;
    return a;
}

// The show's own dice (never the C library's rand(), which the game has)
static unsigned int s_rand = 1;
static unsigned int ap_randNext(void) { s_rand ^= s_rand << 13; s_rand ^= s_rand >> 17; s_rand ^= s_rand << 5; return s_rand; }
static int ap_randInt(int n) { return n > 0 ? (int)(ap_randNext() % (unsigned int)n) : 0; }
static float ap_rand01(void) { return (float)(ap_randNext() & 0xffffff) / (float)0xffffff; }
static float ap_randRange(float lo, float hi) { return lo + (hi - lo) * ap_rand01(); }

// ------------------------------------------------------------ the game

static int ap_mots(void) { return Main_bMotsCompat ? 1 : 0; }

static SithThing* ap_player(void)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    if (!sithWorld_g_pCurrentWorld || !p || p->type != SITH_THING_PLAYER || !p->sector || !p->actorParams.pPlayer)
        return NULL;
    return p;
}

// A level being played (not a menu, a loading screen or a cutscene over it,
// nor one of the level's own scenes, which take the player's controls away)
static int ap_inPlay(void)
{
    SithThing* p = ap_player();
    return jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY && p && stdControl_bControlsActive
           && !jkCutscene_isRendering && !jkGuiRend_IsMenuActive() && !(p->actorParams.flags & SITH_AF_CONTROLSDISABLED);
}

// The weapons, by bin: Jedi Knight's 1-10, Mysteries of the Sith's 121-140
#define AP_W_FISTS (ap_mots() ? 121 : 1)
#define AP_W_SABER (ap_mots() ? 131 : 10)
int iosAutoplay_WeaponBit(int bin)
{
    if (Main_bMotsCompat)
        return (bin >= 121 && bin <= 140) ? bin - 120 : -1;
    return (bin >= 1 && bin <= 10) ? bin : -1;
}
static int ap_isMelee(int bin) { return bin == AP_W_FISTS || bin == AP_W_SABER; }
static int ap_isThrown(int bin) { return ap_mots() ? (bin == 124 || bin == 134) : bin == 4; }
static int ap_isMine(int bin) { return ap_mots() ? (bin == 128 || bin == 138) : bin == 8; }
static int ap_hasAlt(int bin)
{
    // (a second trigger worth using now and then: the rifle's spread, the
    // bowcaster's charged burst, the repeater's spread, the rail detonator's
    // sticky charge, the concussion rifle's... Bryar's slower shot)
    if (ap_mots()) return bin == 123 || bin == 126 || bin == 135 || bin == 129 || bin == 132;
    return bin == 3 || bin == 5 || bin == 6 || bin == 9;
}
// How far from its target it fights with each weapon (feet)
static float ap_fightRangeFt(int bin)
{
    if (bin == AP_W_FISTS) return 2.5f;
    if (bin == AP_W_SABER) return 3.2f;
    if (ap_isThrown(bin)) return 26.0f;
    if (ap_isMine(bin)) return 12.0f;
    if (ap_mots()) {
        switch (bin) {
            case 122: case 132: return 20.0f; // Bryar, Blastech
            case 123: return 22.0f;           // rifle
            case 133: return 34.0f;           // scope rifle
            case 135: case 126: return 22.0f; // bowcaster, repeater
            case 127: case 137: return 34.0f; // rail detonators
            case 129: return 32.0f;           // concussion
            case 130: return 24.0f;           // E-Web
            case 140: return 20.0f;           // carbonite
        }
        return 22.0f;
    }
    switch (bin) {
        case 2: return 20.0f;       // Bryar
        case 3: return 22.0f;       // rifle
        case 5: case 6: return 22.0f; // bowcaster, repeater
        case 7: return 34.0f;       // rail detonator
        case 9: return 32.0f;       // concussion
    }
    return 22.0f;
}

// ------------------------------------------------------------ the world

#define AP_SIGHT_FLAGS (RAYCAST_2000 | RAYCAST_100 | RAYCAST_20 | RAYCAST_2)
#define AP_WALK_FLAGS (RAYCAST_2000 | RAYCAST_20 | RAYCAST_2)

// Whether nothing (walls, doors, lifts; not the actors) is between two
// points, the first in sector sec
static int ap_clear(SithSector* sec, rdVector3 from, rdVector3 to, float radius, int flags)
{
    rdVector3 dir = ap_sub(to, from);
    float dist = ap_len(dir);
    if (!sec) return 0;
    if (dist < 1e-5f) return 1;
    dir = ap_mul(dir, 1.0f / dist);
    sithCollision_SearchForCollisions(sec, NULL, &from, &dir, dist, radius, flags);
    int bClear = 1;
    SithCollision* c;
    while ((c = sithCollision_PopStack()) != NULL) {
        if ((c->type & SITHCOLLISION_WORLD) || ((c->type & SITHCOLLISION_THING) && c->pThingCollided && c->pThingCollided->type == SITH_THING_COG)) {
            bClear = 0;
            break;
        }
    }
    sithCollision_DecreaseStackLevel();
    return bClear;
}
static int ap_sees(SithSector* sec, rdVector3 from, rdVector3 to) { return ap_clear(sec, from, to, 0.0f, AP_SIGHT_FLAGS); }

// The sector a point is in, found from a point in sector sec (NULL if it
// isn't in the one the search ends in: behind a wall)
static SithSector* ap_sectorAt(SithSector* sec, rdVector3 from, rdVector3 to)
{
    if (!sec) return NULL;
    SithSector* s = sithCollision_FindSectorInRadius(sec, &from, &to, 0.0f);
    if (!s || !sithIntersect_IsSphereInSector(&to, 0.0f, s)) return NULL;
    return s;
}

// The floor under a point (within maxDrop): its height, and whether it is
// the level's own floor (not a lift, a crate or another thing)
typedef struct { float z; SithSector* sector; int bWorld; int bSafe; } apFloor;
static int ap_floorBelow(SithSector* sec, rdVector3 p, float maxDrop, apFloor* pOut)
{
    rdVector3 down = ap_v3(0.0f, 0.0f, -1.0f);
    if (!sec) return 0;
    sithCollision_SearchForCollisions(sec, NULL, &p, &down, maxDrop, 0.0f, AP_WALK_FLAGS);
    int bFound = 0;
    SithCollision* c;
    while ((c = sithCollision_PopStack()) != NULL) {
        if (c->type & SITHCOLLISION_WORLD) {
            if (!c->surface || !(c->surface->flags & SITH_SURFACE_FLOOR)) break; // (a slope too steep, a wall's foot)
            pOut->z = p.z - c->distance;
            pOut->sector = c->surface->pSector;
            pOut->bWorld = 1;
            pOut->bSafe = !(c->surface->flags & (SITH_SURFACE_ICY | SITH_SURFACE_VERYICY | SITH_SURFACE_WATER | SITH_SURFACE_VERYDEEPWATER))
                          && pOut->sector && !(pOut->sector->flags & (SITH_SECTOR_FALLDEATH | SITH_SECTOR_UNDERWATER));
            bFound = 1;
            break;
        }
        if ((c->type & SITHCOLLISION_THING) && c->pThingCollided && (c->pThingCollided->flags & SITH_TF_STANDABLE)) {
            pOut->z = p.z - c->distance;
            pOut->sector = c->pThingCollided->sector;
            pOut->bWorld = 0;
            pOut->bSafe = 0;
            bFound = 1;
            break;
        }
    }
    sithCollision_DecreaseStackLevel();
    return bFound;
}

// Feet in the level's units (s_ft: a foot)
static float s_ft = 0.033f;
static inline float FT(float f) { return f * s_ft; }

// A straight walk on the floor from feet (in sec) to (tx, ty): no wall or
// door, and the floor all along neither dropping further than maxDrop nor
// stepping up more than maxStep, nor a pit or water. *pEnd: the floor there.
static int ap_walk(rdVector3 feet, SithSector* sec, float tx, float ty, float maxDrop, float maxStep, apFloor* pEnd)
{
    const float knee = FT(1.6f);
    rdVector3 from = ap_v3(feet.x, feet.y, feet.z + knee);
    rdVector3 to = ap_v3(tx, ty, feet.z + knee);
    if (!ap_clear(sec, from, to, FT(0.6f), AP_WALK_FLAGS)) return 0;
    const float d = ap_lenXY(ap_sub(to, from));
    const int steps = (int)ap_clampf(d / FT(2.5f), 1.0f, 24.0f);
    float z = feet.z;
    apFloor f = { 0 };
    for (int i = 1; i <= steps; i++) {
        const float k = (float)i / (float)steps;
        rdVector3 p = ap_v3(feet.x + (tx - feet.x) * k, feet.y + (ty - feet.y) * k, z + FT(3.5f));
        SithSector* s = ap_sectorAt(sec, from, p);
        if (!s) return 0;
        if (!ap_floorBelow(s, p, FT(3.5f) + maxDrop, &f)) return 0;
        if (!f.bSafe && f.bWorld) return 0;
        if (f.z > z + maxStep) return 0;
        z = f.z;
    }
    if (pEnd) *pEnd = f;
    return 1;
}

// ------------------------------------------------------------ the enemies

typedef struct {
    SithThing* thing;
    uint32_t sig;
    SithThing* kind;  // its template
    rdVector3 pos;    // its middle
    rdVector3 head;
} apFoe;
static apFoe s_foes[AP_MAX_FOES];
static int s_numFoes = 0;

// (a level whose only enemies wait for its script, a boss duel's: those
// are woken for the show, and count)
static int s_bWokeDisabled = 0;
static int ap_livingFoe(SithThing* t)
{
    if (t->type != SITH_THING_ACTOR || (t->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED | SITH_TF_DISABLED)) || !t->sector)
        return 0;
    if (t->controlType != SITH_CT_AI || !t->actor || !t->actor->pClass || t->actorParams.health <= 0.0f)
        return 0;
    if ((t->actor->flags & SITHAI_MODE_DISABLED) && !s_bWokeDisabled) return 0;
    return t->actor->pClass->alignment < 0.0f; // (the enemies: droids, mice and people are 0, friends above)
}

static void ap_collectFoes(void)
{
    SithWorld* w = sithWorld_g_pCurrentWorld;
    s_numFoes = 0;
    if (!w) return;
    for (uint32_t i = 0; i < w->numThingsLoaded && s_numFoes < AP_MAX_FOES; i++) {
        SithThing* t = &w->aThings[i];
        if (!ap_livingFoe(t)) continue;
        apFoe* f = &s_foes[s_numFoes++];
        f->thing = t;
        f->sig = t->signature;
        f->kind = t->pTemplate;
        f->pos = t->position;
        f->head = ap_v3(t->position.x, t->position.y, t->position.z + ap_maxf(t->actorParams.eyeOffset.z, FT(1.0f)));
    }
}

static int ap_foesAround(rdVector3 at, float range)
{
    int n = 0;
    for (int i = 0; i < s_numFoes; i++)
        if (ap_lenXY(ap_sub(s_foes[i].pos, at)) <= range && fabsf(s_foes[i].pos.z - at.z) <= FT(40.0f)) n++;
    return n;
}

// The kinds the level has more than one of, as it opened (copies are only
// ever made of those: never a second boss)
#define AP_MAX_KINDS 96
static SithThing* s_aKind[AP_MAX_KINDS];
static int s_aKindCount[AP_MAX_KINDS];
static int s_numKinds = 0;
static void ap_countKinds(void)
{
    s_numKinds = 0;
    for (int i = 0; i < s_numFoes; i++) {
        SithThing* k = s_foes[i].kind;
        if (!k) continue;
        int j = 0;
        while (j < s_numKinds && s_aKind[j] != k) j++;
        if (j == s_numKinds) {
            if (s_numKinds >= AP_MAX_KINDS) continue;
            s_aKind[s_numKinds] = k;
            s_aKindCount[s_numKinds++] = 0;
        }
        s_aKindCount[j]++;
    }
}
static int ap_copiable(SithThing* kind)
{
    if (!kind) return 0;
    for (int j = 0; j < s_numKinds; j++)
        if (s_aKind[j] == kind) return s_aKindCount[j] >= 2;
    return 0;
}

// ------------------------------------------------------------ groups

typedef struct {
    rdVector3 center;   // (on the floor of its middle one)
    SithSector* sector; // its middle one's
    rdVector3 anchor;   // its middle one's feet
    int count;
    float light;
    float rank;
} apGroup;
#define AP_MAX_GROUPS 96

static float ap_feetOf(const apFoe* f, SithSector** pSec)
{
    apFloor fl;
    *pSec = f->thing->sector;
    if (ap_floorBelow(f->thing->sector, f->pos, FT(30.0f), &fl)) {
        if (fl.sector) *pSec = fl.sector;
        return fl.z;
    }
    return f->pos.z - FT(3.0f);
}

static int s_parent[AP_MAX_FOES];
static int ap_root(int i)
{
    while (s_parent[i] != i) { s_parent[i] = s_parent[s_parent[i]]; i = s_parent[i]; }
    return i;
}

// Enemies close together and in sight of each other make a group; biggest
// first, the better lit first among those near in size
static int ap_makeGroups(apGroup* aGroups)
{
    const int n = s_numFoes;
    for (int i = 0; i < n; i++) s_parent[i] = i;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            const apFoe* a = &s_foes[i];
            const apFoe* b = &s_foes[j];
            if (ap_lenXY(ap_sub(a->pos, b->pos)) > FT(34.0f) || fabsf(a->pos.z - b->pos.z) > FT(14.0f)) continue;
            if (ap_root(i) == ap_root(j)) continue;
            if (!ap_sees(a->thing->sector, a->pos, b->pos)) continue;
            s_parent[ap_root(i)] = ap_root(j);
        }
    }
    int numGroups = 0;
    int rootGroup[AP_MAX_FOES];
    rdVector3 sums[AP_MAX_GROUPS];
    for (int i = 0; i < n; i++) rootGroup[i] = -1;
    for (int i = 0; i < n; i++) {
        const int r = ap_root(i);
        if (rootGroup[r] < 0) {
            if (numGroups >= AP_MAX_GROUPS) continue;
            rootGroup[r] = numGroups;
            memset(&aGroups[numGroups], 0, sizeof(apGroup));
            sums[numGroups] = ap_v3(0, 0, 0);
            numGroups++;
        }
        apGroup* g = &aGroups[rootGroup[r]];
        g->count++;
        SithSector* s = s_foes[i].thing->sector;
        g->light += ap_clampf(s->ambientLight + s->extraLight, 0.0f, 1.0f);
        sums[rootGroup[r]] = ap_add(sums[rootGroup[r]], s_foes[i].pos);
    }
    for (int g = 0; g < numGroups; g++) aGroups[g].center = ap_mul(sums[g], 1.0f / (float)aGroups[g].count);
    // the middle one of each (the nearest the middle), standing on a floor
    float best[AP_MAX_GROUPS];
    for (int g = 0; g < numGroups; g++) best[g] = 1e9f;
    for (int i = 0; i < n; i++) {
        const int gi = rootGroup[ap_root(i)];
        if (gi < 0) continue;
        const float d = ap_lenXY(ap_sub(s_foes[i].pos, aGroups[gi].center));
        if (d < best[gi]) {
            best[gi] = d;
            SithSector* fs;
            const float fz = ap_feetOf(&s_foes[i], &fs);
            aGroups[gi].sector = fs;
            aGroups[gi].anchor = ap_v3(s_foes[i].pos.x, s_foes[i].pos.y, fz);
        }
    }
    for (int g = 0; g < numGroups; g++) {
        apGroup* gr = &aGroups[g];
        gr->center.z = gr->anchor.z;
        gr->light /= (float)gr->count;
        gr->rank = (float)gr->count + 2.0f * gr->light;
    }
    // (biggest first; a stable insertion sort)
    for (int i = 1; i < numGroups; i++) {
        apGroup t = aGroups[i];
        int j = i - 1;
        while (j >= 0 && aGroups[j].rank < t.rank) { aGroups[j + 1] = aGroups[j]; j--; }
        aGroups[j + 1] = t;
    }
    return numGroups;
}

// ------------------------------------------------------------ where he stands

typedef struct { rdVector3 feet; SithSector* sector; float yaw; } apSpot;

// A point off the group's anchor (dx, dy away), on a floor near its height
static int ap_floorNear(const apGroup* g, float x, float y, float strictDrop, apFloor* pOut)
{
    rdVector3 from = ap_v3(g->anchor.x, g->anchor.y, g->anchor.z + FT(4.0f));
    rdVector3 p = ap_v3(x, y, g->anchor.z + FT(4.0f));
    SithSector* s = ap_sectorAt(g->sector, from, p);
    if (!s) return 0;
    return ap_floorBelow(s, p, FT(4.0f) + strictDrop, pOut);
}

// Room around: the floor level and walkable a few steps every way
static int ap_roomAround(rdVector3 feet, SithSector* sec)
{
    for (int k = 0; k < 8; k++) {
        const float a = (float)k * AP_PI * 0.25f;
        apFloor end;
        if (!ap_walk(feet, sec, feet.x + sinf(a) * FT(5.0f), feet.y + cosf(a) * FT(5.0f), FT(1.0f), FT(1.0f), &end)) return 0;
        if (fabsf(end.z - feet.z) > FT(1.0f) || !end.bWorld) return 0;
    }
    return 1;
}

// Somewhere near the group, a straight walk from it, standing safe, facing
// them with a clear view: of several of them (not of a whole crowd at once),
// of the middle of the group, and open space in front
static int ap_findSpot(const apGroup* g, apSpot* out)
{
    float bestScore = 1e9f;
    static const float aR[] = { 12.0f, 16.0f, 20.0f, 25.0f, 30.0f, 36.0f, 44.0f };
    if (!g->sector) return 0;
    for (int ri = 0; ri < (int)(sizeof(aR) / sizeof(aR[0])); ri++) {
        const float r = FT(aR[ri]);
        for (int k = 0; k < 20; k++) {
            const float a = ((float)k + ap_rand01() * 0.6f) * (2.0f * AP_PI / 20.0f);
            const float x = g->center.x + sinf(a) * r, y = g->center.y + cosf(a) * r;
            apFloor fl;
            if (!ap_floorNear(g, x, y, FT(10.0f), &fl) || !fl.bWorld || !fl.bSafe) continue;
            rdVector3 feet = ap_v3(x, y, fl.z);
            if (fabsf(feet.z - g->center.z) > FT(8.0f)) continue;
            if (!ap_roomAround(feet, fl.sector)) continue;
            // (a straight walk to them, if with a jump or a drop: the same place, no wall between)
            if (!ap_walk(feet, fl.sector, g->anchor.x, g->anchor.y, FT(10.0f), FT(6.0f), NULL)) continue;
            rdVector3 eye = ap_v3(feet.x, feet.y, feet.z + FT(AP_EYE_FT));
            SithSector* eyeSec = ap_sectorAt(fl.sector, ap_v3(feet.x, feet.y, feet.z + FT(1.0f)), eye);
            if (!eyeSec) continue;
            int inView = 0, seen = 0;
            float nearest = 1e9f;
            for (int i = 0; i < s_numFoes; i++) {
                const float d = ap_lenXY(ap_sub(s_foes[i].pos, feet));
                if (d > FT(80.0f)) continue;
                nearest = ap_minf(nearest, d);
                if (!ap_sees(eyeSec, eye, s_foes[i].head)) continue;
                seen++;
                if (ap_lenXY(ap_sub(s_foes[i].pos, g->center)) < FT(40.0f)) inView++;
            }
            if (nearest < FT(7.0f)) continue;
            const float yaw = ap_headingTo(feet, g->center);
            const int open = ap_sees(eyeSec, eye, ap_add(eye, ap_v3(sinf(yaw) * FT(10.0f), cosf(yaw) * FT(10.0f), 0.0f)));
            const int middle = ap_sees(eyeSec, eye, ap_v3(g->center.x, g->center.y, g->center.z + FT(4.0f)));
            const float score = -3.0f * (float)(inView < 4 ? inView : 4) + 1.5f * (float)(seen > 6 ? seen - 6 : 0)
                                + (open ? 0.0f : 8.0f) + (middle ? 0.0f : 4.0f) + fabsf(aR[ri] - 22.0f) * 0.2f + ap_rand01();
            if (score < bestScore) {
                bestScore = score;
                out->feet = feet;
                out->sector = fl.sector;
                out->yaw = yaw;
            }
        }
    }
    return bestScore < 1e8f;
}

// Kyle put there, standing, facing yaw (as the levels' own teleports do)
static void ap_place(SithThing* p, const apSpot* spot)
{
    apFloor here;
    float lift = FT(3.0f);
    if (ap_floorBelow(p->sector, p->position, FT(20.0f), &here)) lift = ap_clampf(p->position.z - here.z, FT(0.5f), FT(6.0f));
    if (p->attach_flags) sithThing_DetachThing(p);
    rdVector3 pyr = ap_v3(0.0f, -spot->yaw * (180.0f / AP_PI), 0.0f);
    rdMatrix_BuildRotate34(&p->orient, &pyr);
    p->position = ap_v3(spot->feet.x, spot->feet.y, spot->feet.z + lift);
    sithThing_SetSector(p, spot->sector, 0);
    // (the eyes level)
    p->actorParams.headPYR.x = 0.0f;
    rdVector_Zero3(&p->physicsParams.vel);
    if (p->moveType == SITH_MT_PHYSICS && (p->physicsParams.flags & SITH_PF_FLOORSTICK))
        sithPhysics_FindFloor(p, 1);
    sithCamera_Update(sithCamera_g_pCurCamera);
}

// ------------------------------------------------------------ the show's state

enum { ST_OFF = 0, ST_SAVING, ST_TRAVEL, ST_ARRIVE, ST_SCENE };
static int s_stage = ST_OFF;
static iosAutoplayOptions s_opt;
static double s_tStage = 0.0, s_tPrev = 0.0, s_tStart = 0.0;
static int s_bWantNext = 0;
static int s_bOpened = 0;            // a level opened (jkDev_Custom_LevelOpened -> iosAutoplay's own note)
static int s_curRow = -1;            // the level list's row being played
static int s_aBag[64], s_numBag = 0; // SHUFFLE ALL's rows still to come
static int s_copies = 0;             // copies made in this level

// What the show changes, to put back
static int s_bWasSithEwok = 0, s_diffBefore = 1, s_bSaved = 0;
static int s_saberStyle = 0, s_aHilt[2], s_aColour[2][2], s_aLinked[2], s_bSameColour = 0, s_bSameHilt = 0, s_bSaberKept = 0;

// The fight
static apGroup s_group;
static rdVector3 s_center;
static double s_tScene = 0.0, s_tDown = 0.0, s_tCheck = 0.0, s_tWave = 0.0, s_tReplace = 0.0, s_tMovedFar = 0.0;
static rdVector3 s_farPos;
static SithThing* s_aKin[32];
static int s_numKin = 0;

// The controls
typedef struct {
    float forward, strafe;   // the stick, -1..1
    int fire, alt, jump, duck, force;
    float lookX, lookY;      // mouse counts not handed over yet
} apControls;
static apControls s_ctl;
static float s_pwmX = 0.0f, s_pwmY = 0.0f;
static unsigned char s_aKeys[AP_NUM_KEYS];
static double s_tShowSwitch = 0.0, s_tShowWheel = 0.0, s_tShowJump = 0.0;

// The autopilot
static SithThing* s_target = NULL;
static uint32_t s_targetSig = 0;
static rdVector3 s_aim, s_aimFrom;
static double s_tScan = 0.0, s_tSeen = 0.0, s_tBurst = 0.0, s_tPause = 0.0;
static int s_bBurstAlt = 0;
static rdVector3 s_lastPos, s_prevFeet;
static double s_tMoved = 0.0, s_tEvade = 0.0;
static float s_evadeYaw = 0.0f;
static int s_stuck = 0;
static int s_bTargetInSight = 0;
static SithThing* s_noticed = NULL;
static double s_tReact = 0.0, s_tSettled = 0.0, s_tFireTail = 0.0, s_tGlance = 0.0, s_tCrouch = 0.0;
static float s_pitchBias = 0.0f, s_glance = 0.0f;
static int s_bTailAlt = 0;
static float s_yawVel = 0.0f, s_pitchVel = 0.0f;
enum { FW_STRAFE = 0, FW_CIRCLE, FW_DIAGONAL, FW_PAUSE, FW_JUKE };
static int s_footwork = FW_STRAFE;
static double s_tFootwork = 0.0;
static float s_footDir = 1.0f, s_footStrength = 0.8f, s_footIn = 0.0f;
static float s_axisX = 0.0f, s_axisY = 0.0f;
static int s_aSeen[AP_MAX_FOES], s_numSeen = 0;
static float s_drift = 0.0f, s_driftVel = 0.0f, s_tremor = 0.0f;
static double s_tSearch = 0.0;
static float s_searchOff = 0.0f; // (no one in sight: turned off the way to them, round a wall)

// Weapons
static int s_aRotation[24], s_numRotation = 0;
static int s_bDets = 0, s_bSaber = 0, s_bSaberOnly = 0;
static int s_weapon = 2, s_before = 2, s_thrownBin = 0;
static double s_tSwitch = 0.0, s_tAsked = 0.0, s_tDetsNext = 0.0, s_tThrow = 0.0, s_tRelease = 0.0;
static int s_throws = 0;
static rdVector3 s_bunch;
static int s_bBunchSeen = 0;

// The Force
static int s_aPowers[20], s_numPowers = 0;
static int s_power = -1, s_lastPower = -1;      // the power being used (-1: none)
static double s_tForceNext = 0.0, s_tPowerPicked = 0.0, s_tForceUntil = 0.0, s_tSaberThrowNext = 0.0;
static int s_bForcePressed = 0;
enum { FP_NONE = 0, FP_PICKING, FP_USING, FP_DONE };
static int s_forceStage = FP_NONE;

static void ap_wake(SithThing* t, SithThing* p);

static void ap_clearControls(void)
{
    memset(&s_ctl, 0, sizeof(s_ctl));
    memset(s_aKeys, 0, sizeof(s_aKeys));
    s_pwmX = s_pwmY = 0.0f;
}

int iosAutoplay_IsOn(void) { return s_stage != ST_OFF; }
int iosAutoplay_OwnsKeys(void) { return s_stage != ST_OFF; }
int iosAutoplay_HidesHud(void) { return s_stage != ST_OFF && !s_opt.bHud; }
static int s_bCheatOffLater = 0; // (the cheat to go off as the next level opens: the show ended with none running)
static double s_tRestoring = 0.0; // (the save being loaded back: its loading screen doesn't wait either, until then)
int iosAutoplay_SkipsLoadingWait(void) { return s_stage != ST_OFF || s_tRestoring > 0.0; }

int iosAutoplay_IsScancodeDown(int scancode)
{
    if (s_stage != ST_SCENE || scancode < 0 || scancode >= AP_NUM_KEYS) return 0;
    return s_aKeys[scancode];
}

// ------------------------------------------------------------ levels

// The episode's levels as the cheat's list has them (the Mysteries of the
// Sith secret level's row, its own episode, left out)
int iosAutoplay_LevelRows(void)
{
    if (sithNet_isMulti || !ap_player()) return 0;
    return jkDev_Custom_NumEpisodeLevelRows();
}

// SHUFFLE ALL: each level once, in a shuffled order, then again; never the
// one being played (unless it's the only one)
static int ap_pickRow(void)
{
    const int count = iosAutoplay_LevelRows();
    if (count <= 0) return -1;
    if (s_opt.levelRow >= 0 && s_opt.levelRow < count) return s_opt.levelRow;
    for (int pass = 0; pass < 2; pass++) {
        if (s_numBag <= 0) {
            s_numBag = 0;
            for (int i = 0; i < count && s_numBag < 64; i++) if (i != s_curRow || count == 1) s_aBag[s_numBag++] = i;
            for (int i = s_numBag - 1; i > 0; i--) { const int j = ap_randInt(i + 1), t = s_aBag[i]; s_aBag[i] = s_aBag[j]; s_aBag[j] = t; }
        }
        const int row = s_aBag[--s_numBag];
        if (row != s_curRow || count == 1) return row;
    }
    return (s_curRow + 1) % count;
}

static void ap_travel(double now)
{
    const int row = ap_pickRow();
#ifdef IOSAUTOPLAY_RIG
    {
        char file[64] = "";
        jkDev_Custom_LevelRowInfo(row, NULL, NULL, 0, file, sizeof(file));
        APLOG("LEVEL row %d (%s) at %.1f s\n", row, file, now - s_tStart);
    }
#endif
    ap_clearControls();
    s_bWantNext = 0;
    s_bOpened = 0;
    s_stage = ST_TRAVEL;
    s_tStage = now;
    s_curRow = row;
    jkPlayer_setDiff = s_opt.difficulty;
    if (row < 0 || !jkDev_Custom_GoToLevelRow(row)) {
        APLOG("level row %d could not load\n", row);
    }
}

int iosAutoplay_LevelEnded(void)
{
    if (s_stage == ST_OFF) return -1;
    // (the level's exit reached, or a boss down: the show goes on elsewhere)
    const int row = ap_pickRow();
    const int entry = row >= 0 ? jkDev_Custom_LevelRowEntry(row) : -1;
    APLOG("level ended: row %d (entry %d)\n", row, entry);
    if (entry < 0) return -1;
    ap_clearControls();
    s_curRow = row;
    s_bOpened = 0;
    s_stage = ST_TRAVEL;
    s_tStage = s_tPrev;
    jkPlayer_setDiff = s_opt.difficulty;
    return entry;
}

void iosAutoplay_LevelOpened(void)
{
    s_bOpened = 1;
    if (s_stage != ST_OFF) return;
    s_tRestoring = 0.0; // (the save loaded back: in play)
    if (s_bCheatOffLater) {
        s_bCheatOffLater = 0;
        if (jkDev_Custom_SithEwokIsOn()) jkDev_Custom_CmdSithEwok(NULL, "off");
    }
}

static int s_bLoaded = 0; // (a game loaded during the show: the player's, from the menu)
void iosAutoplay_GameLoaded(void)
{
    if (s_stage != ST_OFF) s_bLoaded = 1;
}

// ------------------------------------------------------------ the saber's look

static void ap_keepSaber(void)
{
    s_saberStyle = iosSaber_GetStyle();
    for (int h = 0; h < 2; h++) {
        s_aHilt[h] = iosSaber_GetHandHilt(h);
        s_aLinked[h] = iosSaber_GetBladeLinked(h);
        for (int b = 0; b < 2; b++) s_aColour[h][b] = iosSaber_GetBladeColour(h, b);
    }
    s_bSameColour = iosSaberWheel_GetSameColour();
    s_bSameHilt = iosSaberWheel_GetSameHilt();
    s_bSaberKept = 1;
}

static void ap_restoreSaber(void)
{
    if (!s_bSaberKept) return;
    s_bSaberKept = 0;
    iosSaber_SetStyle(s_saberStyle);
    for (int h = 0; h < 2; h++) {
        iosSaber_SetHandHilt(h, s_aHilt[h]);
        for (int b = 0; b < 2; b++) iosSaber_SetBladeColour(h, b, s_aColour[h][b]);
        iosSaber_SetBladeLinked(h, s_aLinked[h]);
    }
    if (s_saberStyle == IOSSABER_STYLE_DUAL) {
        iosSaberWheel_SetSameColour(s_bSameColour);
        iosSaberWheel_SetSameHilt(s_bSameHilt);
    }
}

// A new look for the fight: one saber or two, a random hilt each (of those
// the saber wheel has), random colours (the wheel's RANDOM)
static void ap_newSaberLook(void)
{
    if (!s_bSaber) return;
    const int bTwo = iosSaber_StyleAvailable(IOSSABER_STYLE_DUAL) && ap_randInt(2);
    if (bTwo && iosSaberWheel_NumSabers() < 2) iosSaberWheel_AddSecond();
    else if (!bTwo && iosSaberWheel_NumSabers() > 1) iosSaberWheel_RemoveSecond();
    if (iosSaberWheel_NumSabers() > 1) {
        iosSaberWheel_SetSameColour(0);
        iosSaberWheel_SetSameHilt(0);
    }
    for (int h = 0; h < iosSaberWheel_NumSabers(); h++) {
        int aHilts[IOSSABER_NUM_HILTS], n = 0;
        for (int k = 0; k < IOSSABER_NUM_HILTS; k++) if (iosSaber_HiltAvailable(k)) aHilts[n++] = k;
        if (n > 0) iosSaberWheel_SetHilt(h, aHilts[ap_randInt(n)]);
    }
    iosSaberWheel_Random(1); // (the colours: RANDOM held)
    APLOG("saber: %d, hilts %d %d\n", iosSaberWheel_NumSabers(), iosSaber_GetHandHilt(0), iosSaber_GetHandHilt(1));
}

// ------------------------------------------------------------ start, stop

int iosAutoplay_CanStart(void)
{
    return s_stage == ST_OFF && !sithNet_isMulti && ap_inPlay() && !(ap_player()->flags & SITH_TF_DEAD)
           && iosAutoplay_LevelRows() > 0;
}

static void ap_makeRotation(void)
{
    const unsigned int ticked = s_opt.weapons;
    const int first = ap_mots() ? 121 : 1, last = ap_mots() ? 140 : 10;
    s_numRotation = 0;
    s_bSaber = s_bDets = 0;
    int bDets = 0, bMines = 0, bFists = 0;
    for (int bin = first; bin <= last; bin++) {
        const int bit = iosAutoplay_WeaponBit(bin);
        if (bit < 0 || !(ticked & (1u << bit))) continue;
        if (ap_mots() && (bin == 125 || bin == 136 || bin == 139)) continue; // (not given by the all-weapons cheat)
        if (bin == AP_W_SABER) { s_bSaber = 1; continue; }
        if (bin == AP_W_FISTS) { bFists = 1; continue; }
        if (ap_isThrown(bin)) { bDets = 1; continue; }
        if (ap_isMine(bin)) { bMines = 1; continue; }
        if (s_numRotation < 24) s_aRotation[s_numRotation++] = bin;
    }
    // (detonators thrown at bunches, alongside the guns; fists only when
    // picked without ALL; the saber is a weapon of the rotation, or the only one)
    s_bDets = bDets && (s_numRotation > 0 || s_bSaber);
    if (s_bSaber) s_aRotation[s_numRotation++] = AP_W_SABER;
    if (bFists && ticked != IOSAUTOPLAY_WEAPONS_ALL) s_aRotation[s_numRotation++] = AP_W_FISTS;
    if (s_numRotation == 0) {
        if (bDets) s_aRotation[s_numRotation++] = ap_mots() ? 124 : 4;
        if (bMines) s_aRotation[s_numRotation++] = ap_mots() ? 128 : 8;
        if (s_numRotation == 0) s_aRotation[s_numRotation++] = ap_mots() ? 122 : 2;
    }
    s_bSaberOnly = s_bSaber && s_numRotation == 1;
}

static void ap_save(void)
{
    char16_t name[256];
    const char16_t* pLevel = sithWorld_g_pCurrentWorld ? jkGuiTitle_quicksave_related_func1(&jkCog_strings, sithWorld_g_pCurrentWorld->map_jkl_fname) : NULL;
    jk_snwprintf(name, 256, u"%s~Before AUTOPLAY", pLevel ? pLevel : u"");
    s_bSaved = sithGamesave_Save(AP_SAVE_FNAME, 1, 0, name) != 0;
}

int iosAutoplay_Start(const iosAutoplayOptions* pOptions)
{
    if (!pOptions || !iosAutoplay_CanStart()) return 0;
    s_opt = *pOptions;
    s_opt.difficulty = s_opt.difficulty < 0 ? 0 : (s_opt.difficulty > 2 ? 2 : s_opt.difficulty);
    s_opt.crowd = s_opt.crowd < 1 ? 1 : (s_opt.crowd > 3 ? 3 : s_opt.crowd);
    if (!s_opt.weapons) s_opt.weapons = IOSAUTOPLAY_WEAPONS_ALL;
    if (s_opt.levelRow >= iosAutoplay_LevelRows()) s_opt.levelRow = -1;
    ap_makeRotation();
    s_rand = (unsigned int)stdPlatform_GetTimeMsec() * 2654435761u + 1u;
    iosSaberWheel_SeedRandom(ap_randNext());

    s_bWasSithEwok = jkDev_Custom_SithEwokIsOn();
    s_diffBefore = jkPlayer_setDiff;
    ap_keepSaber();
    iosGame_CancelWeaponPick();
    ap_save();
    s_tStart = s_tPrev;
    s_tStage = s_tPrev;
    s_numBag = 0;
    s_curRow = -1;
    {
        // (the row being played now, so SHUFFLE ALL doesn't start with it)
        char file[64];
        const int n = iosAutoplay_LevelRows();
        for (int r = 0; r < n && sithWorld_g_pCurrentWorld; r++)
            if (jkDev_Custom_LevelRowInfo(r, NULL, NULL, 0, file, sizeof(file)) && !__strcmpi(file, sithWorld_g_pCurrentWorld->map_jkl_fname)) s_curRow = r;
    }
    ap_clearControls();
    s_bLoaded = 0;
    s_bCheatOffLater = 0;
    s_stage = ST_SAVING;
    APLOG("START (saved %d) row %d diff %d crowd %d alarm %d weapons %08x\n", s_bSaved, s_opt.levelRow, s_opt.difficulty,
          s_opt.crowd, s_opt.bAlarm, s_opt.weapons);
    return 1;
}

static void ap_end(int bLoad)
{
    if (s_stage == ST_OFF) return;
    s_stage = ST_OFF;
    ap_clearControls();
    s_target = NULL;
    // (everything as it was: the cheat, the difficulty, the saber)
    // (with no level running -- the main menu -- as the next one opens)
    if (!s_bWasSithEwok && jkDev_Custom_SithEwokIsOn() && !jkDev_Custom_CmdSithEwok(NULL, "off")) s_bCheatOffLater = 1;
    jkPlayer_setDiff = s_diffBefore;
    ap_restoreSaber();
    iosGame_CancelWeaponPick();
    if (bLoad && s_bSaved && iosGame_LoadSave(AP_SAVE_FNAME, 0)) {
        s_tRestoring = s_tPrev + 60.0;
        iosGame_ShowMessage("AUTOPLAY off: back to your game");
    }
    else
        iosGame_ShowMessage("AUTOPLAY off");
    APLOG("STOP\n");
}

void iosAutoplay_Stop(void)
{
    ap_end(1);
}

int iosAutoplay_Next(void)
{
    // (not while a level loads, nor before the fight has begun)
    if (s_stage != ST_SCENE || s_bWantNext) return 0;
    s_bWantNext = 1;
    return 1;
}

// ------------------------------------------------------------ the fight

static SithThing* ap_spawnCopy(SithThing* kind, rdVector3 feet, SithSector* sec, float lift)
{
    if ((int)sithAI_maxActors < AP_AI_SPARE || s_copies >= AP_COPIES_PER_LEVEL) return NULL;
    rdMatrix34 m;
    rdVector3 pyr = ap_v3(0.0f, ap_rand01() * 360.0f, 0.0f);
    rdMatrix_BuildRotate34(&m, &pyr);
    rdVector3 pos = ap_v3(feet.x, feet.y, feet.z + lift);
    SithThing* t = sithThing_CreateThingAtPos(kind, &pos, &m, sec, NULL);
    if (!t) return NULL;
    if (t->moveType == SITH_MT_PHYSICS && (t->physicsParams.flags & SITH_PF_FLOORSTICK)) sithPhysics_FindFloor(t, 1);
    s_copies++;
    return t;
}

// An enemy woken and after him, as when it spots him itself (LookForTarget)
static void ap_wake(SithThing* t, SithThing* p)
{
    SithAIControlBlock* a = t->actor;
    if (!a || (a->flags & SITHAI_MODE_DISABLED)) return;
    a->flags &= ~(SITHAI_MODE_SLEEPING | SITHAI_MODE_SEARCHING);
    a->flags |= SITHAI_MODE_ACTIVE | SITHAI_MODE_TOUGHSKIN | SITHAI_MODE_HASDEST | SITHAI_MODE_ATTACKING;
    a->pDistractor = p;
    a->pMoveThing = p;
    a->field_204 = sithTime_g_msecGameTime; // (seen just now)
}

static void ap_noteKin(rdVector3 center)
{
    s_numKin = 0;
    for (int i = 0; i < s_numFoes && s_numKin < 32; i++) {
        const apFoe* f = &s_foes[i];
        if (ap_lenXY(ap_sub(f->pos, center)) > FT(30.0f) || fabsf(f->pos.z - center.z) > FT(16.0f)) continue;
        if (!ap_copiable(f->kind)) continue;
        s_aKin[s_numKin++] = f->kind;
    }
}

// CROWD: copies of the group's kinds round each of them
static void ap_addCopies(rdVector3 center, rdVector3 kyle)
{
    if (s_opt.crowd <= 1) return;
    int around = ap_foesAround(center, FT(AP_ALARM_FT));
    const int originals = s_numFoes;
    for (int i = 0; i < originals; i++) {
        const apFoe f = s_foes[i];
        if (ap_lenXY(ap_sub(f.pos, center)) > FT(30.0f) || fabsf(f.pos.z - center.z) > FT(16.0f)) continue;
        if (!ap_copiable(f.kind)) continue;
        SithSector* fsec;
        const float fz = ap_feetOf(&f, &fsec);
        const rdVector3 feet = ap_v3(f.pos.x, f.pos.y, fz);
        const float lift = f.pos.z - fz;
        for (int c = 1; c < s_opt.crowd; c++) {
            if (around >= AP_CROWD_CAP) return;
            for (int tries = 0; tries < 14; tries++) {
                const float a = ap_rand01() * 2.0f * AP_PI, r = FT(ap_randRange(3.2f, 7.5f));
                const float x = feet.x + sinf(a) * r, y = feet.y + cosf(a) * r;
                apFloor end;
                if (!ap_walk(feet, fsec, x, y, FT(2.0f), FT(2.0f), &end) || !end.bWorld || !end.bSafe) continue;
                const rdVector3 p = ap_v3(x, y, end.z);
                if (fabsf(p.z - feet.z) > FT(2.0f) || ap_lenXY(ap_sub(p, kyle)) < FT(9.0f)) continue;
                int bClear = 1;
                for (int k = 0; k < s_numFoes; k++)
                    if (ap_lenXY(ap_sub(s_foes[k].pos, p)) < FT(3.0f) && fabsf(s_foes[k].pos.z - lift - p.z) < FT(8.0f)) { bClear = 0; break; }
                if (!bClear) continue;
                SithThing* t = ap_spawnCopy(f.kind, p, end.sector ? end.sector : fsec, lift);
                if (!t) return;
                if (s_numFoes < AP_MAX_FOES) {
                    apFoe* nf = &s_foes[s_numFoes++];
                    nf->thing = t; nf->sig = t->signature; nf->kind = f.kind; nf->pos = t->position; nf->head = t->position;
                }
                around++;
                break;
            }
        }
    }
}

// More of the group's kinds come in, from out of his sight round the area,
// woken and after him; as many as the group had (times CROWD), within the
// caps. How many came.
static int ap_reinforce(SithThing* pl)
{
    if (s_numKin <= 0 || !s_group.sector) return 0;
    ap_collectFoes();
    int around = ap_foesAround(s_center, FT(AP_ALARM_FT));
    int wanted = s_group.count * s_opt.crowd;
    wanted = wanted < 3 ? 3 : (wanted > 10 ? 10 : wanted);
    const rdVector3 kyle = pl->position;
    const rdVector3 eye = ap_v3(kyle.x, kyle.y, kyle.z + pl->actorParams.eyeOffset.z);
    int n = 0;
    for (int tries = 0; tries < 320 && n < wanted; tries++) {
        if (around >= AP_CROWD_CAP) break;
        const float a = ap_rand01() * 2.0f * AP_PI, r = FT(ap_randRange(12.0f, 38.0f));
        const float x = s_center.x + sinf(a) * r, y = s_center.y + cosf(a) * r;
        apFloor fl;
        if (!ap_floorNear(&s_group, x, y, FT(8.0f), &fl) || !fl.bWorld || !fl.bSafe) continue;
        const rdVector3 p = ap_v3(x, y, fl.z);
        if (fabsf(p.z - s_group.anchor.z) > FT(8.0f) || ap_lenXY(ap_sub(p, kyle)) < FT(16.0f)) continue;
        if (!ap_walk(s_group.anchor, s_group.sector, x, y, FT(10.0f), FT(6.0f), NULL)) continue;
        const int bSeen = ap_sees(pl->sector, eye, ap_v3(p.x, p.y, p.z + FT(4.0f)));
        if (bSeen && (tries < 200 || ap_lenXY(ap_sub(p, kyle)) < FT(26.0f))) continue;
        SithThing* kind = s_aKin[ap_randInt(s_numKin)];
        // (its height off the floor, as its kind stands)
        float lift = FT(3.0f);
        for (int i = 0; i < s_numFoes; i++) {
            if (s_foes[i].kind != kind) continue;
            SithSector* fs;
            lift = s_foes[i].pos.z - ap_feetOf(&s_foes[i], &fs);
            break;
        }
        SithThing* t = ap_spawnCopy(kind, p, fl.sector, lift);
        if (!t) break;
        ap_wake(t, pl);
        around++;
        n++;
    }
    return n;
}

static double s_tRally = 0.0;
static void ap_rally(SithThing* pl)
{
    ap_collectFoes();
    int n = 0;
    for (int i = 0; i < s_numFoes; i++) {
        apFoe* f = &s_foes[i];
        if (ap_lenXY(ap_sub(f->pos, s_center)) > FT(AP_ALARM_FT * 1.3f) || fabsf(f->pos.z - s_center.z) > FT(30.0f)) continue;
        SithAIControlBlock* a = f->thing->actor;
        if (!a) continue;
        SithSector* fs;
        const rdVector3 feet = ap_v3(f->pos.x, f->pos.y, ap_feetOf(f, &fs));
        rdVector3 dest = pl->position;
        if (!ap_walk(feet, fs, dest.x, dest.y, FT(10.0f), FT(6.0f), NULL)) dest = s_group.anchor;
        ap_wake(f->thing, pl);
        sithAI_SetMoveThing(a, &dest, 1.5f);
        n++;
    }
    sithAIAwareness_CreateTransmittingEvent(pl->sector, &pl->position, 0, 6.0f, pl);
    APLOG("rally: %d called\n", n);
}

// ALARM: everyone round the group woken and after him, and the alarm heard
static void ap_alarm(rdVector3 center, SithThing* pl)
{
    for (int i = 0; i < s_numFoes; i++) {
        const apFoe* f = &s_foes[i];
        if (ap_lenXY(ap_sub(f->pos, center)) <= FT(AP_ALARM_FT) && fabsf(f->pos.z - center.z) <= FT(40.0f)) ap_wake(f->thing, pl);
    }
    sithAIAwareness_CreateTransmittingEvent(pl->sector, &pl->position, 0, 3.0f, pl);
}

static void ap_pickWeapon(int notThis)
{
    int id = s_aRotation[ap_randInt(s_numRotation)];
    for (int k = 0; k < 8 && id == notThis && s_numRotation > 1; k++) id = s_aRotation[ap_randInt(s_numRotation)];
    s_weapon = id;
    s_tAsked = 0.0;
}

static int ap_saberThrowBin(void);

// The powers he has to use now and then (of those that fit a show)
static void ap_notePowers(void)
{
    static const int aJk[] = { 21, 22, 24, 27, 30, 31, 32, 33 };          // jump, speed, pull, blinding, throw, grip, lightning, destruction
    static const int aMots[] = { 21, 22, 24, 27, 31, 33, 38, 39 };        // jump, speed, pull, blinding, grip, destruction, push, chain lightning
    const int* a = ap_mots() ? aMots : aJk;
    const int n = ap_mots() ? (int)(sizeof(aMots) / sizeof(aMots[0])) : (int)(sizeof(aJk) / sizeof(aJk[0]));
    s_numPowers = 0;
    for (int i = 0; i < n; i++) if (iosGame_IsPowerAvailable(a[i])) s_aPowers[s_numPowers++] = a[i];
    APLOG("powers: %d (saber throw %d)\n", s_numPowers, iosGame_IsPowerAvailable(ap_saberThrowBin()));
}

// The autopilot starts afresh (Kyle just placed at feet)
static void ap_resetPilot(double now, rdVector3 feet)
{
    ap_clearControls();
    s_target = NULL;
    s_numSeen = 0;
    s_tScan = 0.0;
    s_tSeen = now;
    s_lastPos = s_prevFeet = feet;
    s_tMoved = s_tMovedFar = now;
    s_farPos = feet;
    s_tEvade = 0.0;
    s_stuck = 0;
    s_throws = 0;
    s_tBurst = s_tPause = now;
    s_noticed = NULL;
    s_tReact = s_tSettled = s_tFireTail = s_tCrouch = 0.0;
    s_tFootwork = now;
    s_tGlance = now + ap_randRange(0.8f, 2.0f);
    s_glance = 0.0f;
    s_axisX = s_axisY = 0.0f;
    s_yawVel = s_pitchVel = 0.0f;
    s_forceStage = FP_NONE;
    s_power = -1;
    s_bForcePressed = 0;
    s_tForceNext = now + ap_randRange(3.0f, 7.0f);
    s_tSaberThrowNext = now + ap_randRange(4.0f, 9.0f);
    s_tRally = now + 3.0;
}

static float ap_measureFt(SithThing* pl)
{
    apFloor f;
    float eye = FT(AP_EYE_FT);
    if (ap_floorBelow(pl->sector, pl->position, 2.0f, &f)) eye = pl->position.z + pl->actorParams.eyeOffset.z - f.z;
    return ap_clampf(eye / AP_EYE_FT, 0.003f, 0.2f);
}

// The show's fight: one of the level's best groups (by size and light),
// picked at random among the top few so each show differs; Kyle placed
// near it, its crowd, its alarm
static void ap_startScene(double now, int bAnother)
{
    SithThing* pl = ap_player();
    if (!pl) return;
    s_ft = ap_measureFt(pl);
    ap_collectFoes();
    if (s_numFoes == 0 && !s_bWokeDisabled) {
        // (a boss waiting for the duel's script: woken for the show)
        s_bWokeDisabled = 1;
        ap_collectFoes();
        for (int i = 0; i < s_numFoes; i++) {
            s_foes[i].thing->actor->flags &= ~SITHAI_MODE_DISABLED;
            ap_wake(s_foes[i].thing, pl);
        }
        APLOG("woke %d waiting\n", s_numFoes);
    }
    if (s_numFoes == 0) {
        APLOG("no enemies here\n");
#ifdef IOSAUTOPLAY_RIG
        for (uint32_t i = 0; i < sithWorld_g_pCurrentWorld->numThingsLoaded; i++) {
            SithThing* t = &sithWorld_g_pCurrentWorld->aThings[i];
            if (t->type != SITH_THING_ACTOR) continue;
            APLOG("  actor %d flags %x ctl %d ai %p class %p align %.1f aiflags %x health %.0f\n", (int)i, t->flags, t->controlType, (void*)t->actor,
                  (void*)(t->actor ? t->actor->pClass : NULL), t->actor && t->actor->pClass ? t->actor->pClass->alignment : 99.0f,
                  t->actor ? t->actor->flags : 0, t->actorParams.health);
        }
#endif
        if (s_opt.levelRow < 0) ap_travel(now);
        else s_stage = ST_SCENE; // (PICK ONE with none left: he stays and looks about)
        return;
    }
    static apGroup aGroups[AP_MAX_GROUPS];
    const int numGroups = ap_makeGroups(aGroups);
    APLOG("groups: %d foes, %d groups, biggest %d %d %d (a foot %.4f)\n", s_numFoes, numGroups, numGroups > 0 ? aGroups[0].count : 0,
          numGroups > 1 ? aGroups[1].count : 0, numGroups > 2 ? aGroups[2].count : 0, s_ft);
    // (the top four weighted 4, 3, 2, 1 in a random order; then the rest by rank)
    int order[AP_MAX_GROUPS], numOrder = 0;
    int top[4], numTop = numGroups < 4 ? numGroups : 4;
    for (int i = 0; i < numTop; i++) top[i] = i;
    while (numTop > 0) {
        int total = 0;
        for (int i = 0; i < numTop; i++) total += 4 - top[i];
        int r = ap_randInt(total), k = 0;
        while (r >= 4 - top[k]) { r -= 4 - top[k]; k++; }
        order[numOrder++] = top[k];
        for (int i = k; i < numTop - 1; i++) top[i] = top[i + 1];
        numTop--;
    }
    for (int i = 4; i < numGroups; i++) order[numOrder++] = i;
    // (another fight in this level: the groups where this one is go last)
    if (bAnother) {
        int here[AP_MAX_GROUPS], numHere = 0, k = 0;
        for (int i = 0; i < numOrder; i++) {
            const apGroup* g = &aGroups[order[i]];
            if (ap_lenXY(ap_sub(g->center, s_center)) < FT(60.0f) && fabsf(g->center.z - s_center.z) < FT(24.0f)) here[numHere++] = order[i];
            else order[k++] = order[i];
        }
        for (int i = 0; i < numHere; i++) order[k++] = here[i];
    }
    for (int oi = 0; oi < numOrder; oi++) {
        const apGroup* g = &aGroups[order[oi]];
        apSpot spot;
        if (!ap_findSpot(g, &spot)) continue;
        APLOG("SCENE at %.1f s: group of %d at %.2f %.2f %.2f (rank %d), Kyle at %.2f %.2f %.2f\n", now - s_tStart, g->count,
              g->center.x, g->center.y, g->center.z, order[oi], spot.feet.x, spot.feet.y, spot.feet.z);
        ap_place(pl, &spot);
        s_group = *g;
        s_center = g->center;
        ap_noteKin(g->center);
        ap_addCopies(g->center, spot.feet);
        if (s_opt.bAlarm) {
            ap_collectFoes();
            ap_alarm(g->center, pl);
        }
        s_stage = ST_SCENE;
        s_tScene = now;
        s_tDown = 0.0;
        s_tCheck = now + 1.0;
        s_tWave = s_tReplace = now;
        ap_resetPilot(now, spot.feet);
        ap_notePowers();
        ap_newSaberLook();
        ap_pickWeapon(-1);
        // (the saber, when it's one of them, more often than not to start)
        if (s_bSaber && !s_bSaberOnly && ap_randInt(3) == 0) s_weapon = AP_W_SABER;
        s_before = s_weapon;
        s_tSwitch = now + ap_randRange(7.0f, 12.0f);
        s_tDetsNext = now + 3.0;
        return;
    }
    APLOG("nowhere to stand near any group\n");
    if (s_opt.levelRow < 0) ap_travel(now);
    else s_stage = ST_SCENE;
}

// ------------------------------------------------------------ the autopilot

static SithThing* ap_targetThing(void)
{
    if (s_target && (s_target->signature != s_targetSig || !ap_livingFoe(s_target))) s_target = NULL;
    return s_target;
}

// Whether one at pos is in reach of a swing from about eye height (not up
// on the ceiling, nor down below a ledge)
static int ap_reachable(rdVector3 eye, rdVector3 pos)
{
    const float dz = pos.z - eye.z;
    return dz < FT(3.5f) && dz > -FT(9.0f);
}

// The enemies in sight, the target (the nearest, kept while it stays about
// as near), and a bunch for a detonator
static void ap_scan(rdVector3 eye, SithSector* eyeSec, double now)
{
    ap_collectFoes();
    s_numSeen = 0;
    int nearest = -1;
    float nearestD = 1e9f, targetD = 1e9f;
    SithThing* tgt = ap_targetThing();
    // (with the saber or the fists, one he can reach first: not a turret
    // up on the ceiling, nor one far below a ledge)
    const int bMelee = ap_isMelee(s_weapon);
    for (int i = 0; i < s_numFoes; i++) {
        const apFoe* f = &s_foes[i];
        float d = ap_len(ap_sub(f->pos, eye));
        if (d > FT(100.0f)) continue;
        if (!ap_sees(eyeSec, eye, f->pos) && !ap_sees(eyeSec, eye, f->head)) continue;
        s_aSeen[s_numSeen++] = i;
        if (bMelee && !ap_reachable(eye, f->pos)) d += FT(60.0f);
        if (d < nearestD) { nearestD = d; nearest = i; }
        if (f->thing == tgt) targetD = d;
    }
    s_bTargetInSight = 0;
    if (nearest >= 0) {
        if (!tgt || targetD > 1e8f || targetD > nearestD * 1.8f + FT(4.0f)) {
            s_target = s_foes[nearest].thing;
            s_targetSig = s_target->signature;
        }
        for (int k = 0; k < s_numSeen; k++) {
            const apFoe* f = &s_foes[s_aSeen[k]];
            if (f->thing == s_target) { s_aim = f->pos; s_bTargetInSight = 1; }
        }
        s_tSeen = now;
    }
    else if (now - s_tSeen > 1.5) {
        s_target = NULL;
    }
    // a bunch: three or more in sight close together, not too near
    s_bBunchSeen = 0;
    int bestCount = 0;
    for (int a = 0; a < s_numSeen; a++) {
        const apFoe* fa = &s_foes[s_aSeen[a]];
        const float d = ap_lenXY(ap_sub(fa->pos, eye));
        if (d < FT(14.0f) || d > FT(48.0f)) continue;
        int count = 0;
        rdVector3 sum = ap_v3(0, 0, 0);
        for (int b = 0; b < s_numSeen; b++) {
            const apFoe* fb = &s_foes[s_aSeen[b]];
            if (ap_lenXY(ap_sub(fb->pos, fa->pos)) < FT(9.0f)) { count++; sum = ap_add(sum, fb->pos); }
        }
        if (count >= 3 && count > bestCount) {
            bestCount = count;
            s_bunch = ap_mul(sum, 1.0f / (float)count);
            s_bBunchSeen = 1;
        }
    }
}

// Whether he can go that way (dir, flat) for a little: no wall, ledge, pit or water
static int ap_canGo(rdVector3 feet, SithSector* sec, float dx, float dy)
{
    apFloor end;
    if (!ap_walk(feet, sec, feet.x + dx * FT(6.0f), feet.y + dy * FT(6.0f), FT(4.0f), FT(2.5f), &end)) return 0;
    return !end.bWorld || end.bSafe;
}

// How far he can see along a heading from the eyes (up to maxFt feet): a
// glance or a turn away is never at a wall in his face
static float ap_openFt(SithSector* eyeSec, rdVector3 eye, float heading, float maxFt)
{
    for (float d = maxFt; d >= 2.0f; d *= 0.5f) {
        if (ap_sees(eyeSec, eye, ap_add(eye, ap_v3(sinf(heading) * FT(d), cosf(heading) * FT(d), 0.0f)))) return d;
    }
    return 0.0f;
}

// The view's heading and pitch now (radians; pitch up positive)
static float ap_yawOf(SithThing* pl) { return atan2f(pl->orient.lvec.x, pl->orient.lvec.y); }
static float ap_pitchOf(SithThing* pl) { return pl->actorParams.headPYR.x * (AP_PI / 180.0f); }

// The hand on the mouse: the view turns as a player's hand would turn it,
// speeding up and easing in, a touch past the mark and back (a spring a
// little under critical damping), never faster than a quick flick. Pitch the
// same, slower and steadier. It goes in as mouse counts, by the game's own
// mouse look settings (as the touch overlay's gyro aiming does).
static void ap_steer(SithThing* pl, float yawWanted, float pitchWanted, float dt, float quickness)
{
    float turnPerCount = 0.0f, pitchPerCount = 0.0f;
    iosGame_GetMouseLookDegrees(&turnPerCount, &pitchPerCount);
    const float w = quickness, zeta = 0.62f;
    const float yawErr = ap_wrap(yawWanted - ap_yawOf(pl));
    const float acc = ap_clampf(w * w * yawErr - 2.0f * zeta * w * s_yawVel, -70.0f, 70.0f);
    s_yawVel = ap_clampf(s_yawVel + acc * dt, -6.0f, 6.0f);
    if (fabsf(turnPerCount) > 1e-4f) s_ctl.lookX += s_yawVel * dt * (180.0f / AP_PI) / turnPerCount;

    const float wp = w * 0.7f, zp = 0.85f;
    const float pitchErr = ap_clampf(pitchWanted, -0.8f, 0.8f) - ap_pitchOf(pl);
    const float pacc = ap_clampf(wp * wp * pitchErr - 2.0f * zp * wp * s_pitchVel, -30.0f, 30.0f);
    s_pitchVel = ap_clampf(s_pitchVel + pacc * dt, -2.5f, 2.5f);
    // (pitch up: the mouse up, as counts going down are the head going down)
    if (fabsf(pitchPerCount) > 1e-4f) s_ctl.lookY -= s_pitchVel * dt * (180.0f / AP_PI) / pitchPerCount;
}

// A hand's aim is never quite still: a slow wander (about 2 degrees) and a faint tremor
static void ap_wobble(float dt)
{
    s_driftVel += (ap_randRange(-1.0f, 1.0f) * 0.35f - s_drift * 1.6f - s_driftVel * 0.8f) * dt;
    s_drift = ap_clampf(s_drift + s_driftVel * dt, -0.035f, 0.035f);
    s_tremor += (ap_randRange(-1.0f, 1.0f) * 0.004f - s_tremor) * ap_minf(1.0f, dt * 12.0f);
}

static void ap_takeWeapon(int bin)
{
    s_weapon = bin;
    s_tAsked = 0.0;
}

// Footwork: a few kinds, each for a while of its own length (never on a
// beat): strafing one way, circling the target, a diagonal step in or back,
// a short pause, a quick juke. The stick eases towards what they want.
static void ap_newFootwork(double now, int bFighting, int bMelee)
{
    const int r = ap_randInt(100);
    const float was = s_footDir;
    s_footDir = ap_randInt(3) ? -was : was; // (mostly the other way)
    s_footIn = 0.0f;
    if (!bFighting) {
        s_footwork = FW_STRAFE;
        s_footStrength = 0.0f;
        s_tFootwork = now + ap_randRange(0.5f, 1.2f);
        return;
    }
    if (bMelee) {
        // (up close with the saber: circling and side steps, a lunge in, a step back)
        if (r < 40)      { s_footwork = FW_CIRCLE;   s_footStrength = ap_randRange(0.6f, 1.0f); s_tFootwork = now + ap_randRange(0.8f, 2.2f); }
        else if (r < 65) { s_footwork = FW_STRAFE;   s_footStrength = ap_randRange(0.5f, 0.9f); s_tFootwork = now + ap_randRange(0.4f, 1.2f); }
        else if (r < 85) { s_footwork = FW_DIAGONAL; s_footStrength = ap_randRange(0.4f, 0.8f); s_tFootwork = now + ap_randRange(0.3f, 0.8f);
                           s_footIn = ap_randInt(3) ? 0.8f : -0.7f; }
        else             { s_footwork = FW_PAUSE;    s_footStrength = 0.0f; s_tFootwork = now + ap_randRange(0.2f, 0.5f); }
        if (s_footwork == FW_STRAFE && ap_randInt(7) == 0) s_ctl.jump = 1;
        return;
    }
    if (r < 34)      { s_footwork = FW_STRAFE;   s_footStrength = ap_randRange(0.55f, 1.0f); s_tFootwork = now + ap_randRange(0.5f, 2.2f); }
    else if (r < 58) { s_footwork = FW_CIRCLE;   s_footStrength = ap_randRange(0.75f, 1.0f); s_tFootwork = now + ap_randRange(1.4f, 3.4f); }
    else if (r < 78) { s_footwork = FW_DIAGONAL; s_footStrength = ap_randRange(0.5f, 0.85f); s_tFootwork = now + ap_randRange(0.5f, 1.4f);
                       s_footIn = ap_randInt(2) ? 0.7f : -0.6f; }
    else if (r < 90) { s_footwork = FW_PAUSE;    s_footStrength = 0.0f; s_tFootwork = now + ap_randRange(0.25f, 0.8f); }
    else             { s_footwork = FW_JUKE;     s_footStrength = 1.0f; s_tFootwork = now + ap_randRange(0.22f, 0.45f); }
    // now and then a hop on a sidestep, or a crouch in a pause (rarely: as a player dodging)
    if (s_footwork == FW_JUKE && ap_randInt(5) == 0) s_ctl.jump = 1;
    if (s_footwork == FW_PAUSE && ap_randInt(10) == 0) s_tCrouch = s_tFootwork;
}

static void ap_ease(float* axis, float wanted, float dt)
{
    // (picking up at about 4 a second, dying down a little quicker)
    const float rate = (fabsf(wanted) > fabsf(*axis) && wanted * *axis >= 0.0f) ? 4.0f : 6.0f;
    *axis += ap_clampf(wanted - *axis, -rate * dt, rate * dt);
}

// The Force, now and then, as fits the moment: a power picked (as on the
// Force wheel), a moment later used with FORCE (a tap, or held for those
// that go on: grip, lightning, a charged jump). Saber Throw is the saber's
// own (below).
static int ap_powerFits(int bin, float d, int bSeen, int bMelee)
{
    switch (bin) {
        case 21: return !bSeen || ap_randInt(3) == 0;                 // jump: on the move, or a leap in a fight
        case 22: return !bSeen || d > FT(30.0f);                      // speed: to close the distance
        case 24: return bSeen && d > FT(8.0f) && d < FT(45.0f) && !bMelee; // pull: a gun out of a hand
        case 27: return bSeen && d < FT(40.0f) && s_numSeen >= 2;     // blinding: several of them
        case 30: return bSeen && d < FT(35.0f);                       // throw
        case 31: return bSeen && d < FT(30.0f);                       // grip
        case 32: case 39: return bSeen && d < FT(22.0f);              // lightning, chain lightning
        case 33: return bSeen && d > FT(10.0f) && d < FT(55.0f);      // destruction
        case 38: return bSeen && d < FT(14.0f);                       // push
    }
    return 0;
}
static float ap_powerHold(int bin)
{
    switch (bin) {
        case 21: return ap_randRange(0.35f, 0.75f);
        case 31: return ap_randRange(1.4f, 2.6f);
        case 32: return ap_randRange(0.8f, 1.8f);
        case 39: return ap_randRange(0.5f, 1.0f);
    }
    return ap_randRange(0.08f, 0.16f); // (a tap)
}

static int ap_saberThrowBin(void) { return ap_mots() ? 37 : IOSSABERTHROWJK_BIN; }

static void ap_force(double now, SithThing* pl, float targetD, int bFighting, int bHolding, int bSettled, int bReachOut)
{
    switch (s_forceStage) {
    case FP_NONE: {
        const int bMelee = bHolding && ap_isMelee(s_weapon);
        // Saber Throw: the saber in hand, someone in sight at a fair distance
        const int bOutOfReach = bReachOut;
        if (bHolding && s_weapon == AP_W_SABER && bFighting && bSettled && (now >= s_tSaberThrowNext || (bOutOfReach && now >= s_tSaberThrowNext - 6.0))
            && targetD > FT(8.0f) && targetD < FT(40.0f) && iosGame_IsPowerAvailable(ap_saberThrowBin())) {
            s_power = ap_saberThrowBin();
            s_forceStage = FP_PICKING;
            s_tPowerPicked = now + ap_randRange(0.15f, 0.35f);
            s_tSaberThrowNext = now + ap_randRange(10.0f, 18.0f);
            iosGame_SelectPower(s_power);
            s_tShowWheel = now + 0.3;
            APLOG("saber throw at %.1f ft\n", targetD / s_ft);
            break;
        }
        if (now < s_tForceNext || s_numPowers <= 0) break;
        int aFit[20], n = 0;
        for (int i = 0; i < s_numPowers; i++)
            if (s_aPowers[i] != s_lastPower && ap_powerFits(s_aPowers[i], targetD, bFighting, bMelee)) aFit[n++] = s_aPowers[i];
        if (n == 0) { s_tForceNext = now + ap_randRange(1.0f, 2.5f); break; }
        s_power = aFit[ap_randInt(n)];
        s_forceStage = FP_PICKING;
        s_tPowerPicked = now + ap_randRange(0.2f, 0.45f);
        iosGame_SelectPower(s_power);
        s_tShowWheel = now + 0.3;
        APLOG("force %d at %.1f ft\n", s_power, targetD / s_ft);
        break;
    }
    case FP_PICKING:
        if (now < s_tPowerPicked) break;
        if (iosGame_GetCurPower() != s_power) { // (it didn't take: given up)
            s_forceStage = FP_NONE;
            s_tForceNext = now + ap_randRange(2.0f, 4.0f);
            break;
        }
        s_forceStage = FP_USING;
        s_tForceUntil = now + ap_powerHold(s_power);
        break;
    case FP_USING:
        s_ctl.force = 1;
        if (now >= s_tForceUntil) {
            s_ctl.force = 0;
            s_forceStage = FP_DONE;
            s_tForceUntil = now + 0.2;
        }
        break;
    case FP_DONE:
        if (now < s_tForceUntil) break;
        s_lastPower = s_power == ap_saberThrowBin() ? s_lastPower : s_power;
        s_power = -1;
        s_forceStage = FP_NONE;
        s_tForceNext = now + ap_randRange(5.0f, 12.0f);
        break;
    }
}

static void ap_autopilot(double now, float dt, SithThing* pl)
{
    const rdVector3 pos = pl->position;
    SithSector* sec = pl->sector;
    apFloor here;
    const float feetZ = ap_floorBelow(sec, pos, FT(12.0f), &here) ? here.z : pos.z - FT(3.0f);
    const rdVector3 feet = ap_v3(pos.x, pos.y, feetZ);
    const rdVector3 eye = ap_v3(pos.x, pos.y, pos.z + pl->actorParams.eyeOffset.z);
    SithSector* eyeSec = ap_sectorAt(sec, pos, eye);
    if (!eyeSec) eyeSec = sec;
    const float yaw = ap_yawOf(pl);
    const int curWeapon = iosGame_GetCurWeapon();

    if (now >= s_tScan) {
        s_tScan = now + 0.12;
        ap_scan(eye, eyeSec, now);
    }
    ap_wobble(dt);
    SithThing* tgt = ap_targetThing();

    // a new face: a moment to react (the view stays where it was going)
    if (tgt != s_noticed) {
        const int bFresh = s_noticed == NULL || tgt != NULL;
        s_noticed = tgt;
        if (tgt && bFresh) {
            s_tReact = now + ap_randRange(0.15f, 0.4f);
            s_pitchBias = ap_randRange(-0.02f, 0.025f);
        }
        s_tSettled = 0.0;
    }
    const int bReacting = now < s_tReact;
    const int bFighting = tgt != NULL && !bReacting;
    if (!bReacting) s_aimFrom = s_aim;

    // the weapon: a detonator at a bunch, another weapon only in a lull (no
    // one in sight for a moment), never mid-fight; a weapon that didn't take
    // is asked for again, as a thumb would
    const float targetD = tgt ? ap_len(ap_sub(s_aim, eye)) : 1e9f;
    const int bLull = !tgt && now - s_tSeen > 0.8;
    if (s_throws == 0 && s_forceStage == FP_NONE) {
        if (s_bDets && s_bBunchSeen && now >= s_tDetsNext && !ap_isMelee(s_weapon)) {
            s_before = s_weapon;
            ap_takeWeapon(ap_mots() ? 124 : 4);
            s_throws = 1 + ap_randInt(2);
            s_tThrow = now + 0.6;
            s_tRelease = 0.0;
        }
        else if (bLull && now >= s_tSwitch) {
            ap_pickWeapon(s_weapon);
            s_tSwitch = now + ap_randRange(9.0f, 16.0f);
        }
    }
    if (curWeapon != s_weapon && now - s_tAsked > 0.9) {
        if (iosGame_SelectWeapon(s_weapon)) s_tShowSwitch = now + 0.3; // (as the weapon wheel picks it)
        s_tAsked = now;
    }
    const int bHolding = curWeapon == s_weapon;
    const int bMelee = bHolding && ap_isMelee(s_weapon);

    // where it looks
    const int bEvading = now < s_tEvade;
    const int bThrowing = bHolding && ap_isThrown(s_weapon);
    const rdVector3 aim = (bThrowing && s_bBunchSeen) ? s_bunch : s_aimFrom;
    float yawWanted = yaw, pitchWanted = -0.03f, quickness = 9.0f;
    float wantX = 0.0f, wantY = 0.0f;
    const float dxy = ap_maxf(ap_lenXY(ap_sub(aim, eye)), FT(0.1f));
    if (bEvading) {
        yawWanted = s_evadeYaw;
        quickness = 6.0f;
        wantY = 1.0f;
    }
    else if (tgt) {
        // on the target (its last place in sight, for the moment it's out of
        // it), with the hand's wander; a little up or down at those above and below
        yawWanted = ap_headingTo(eye, aim) + s_drift + s_tremor;
        pitchWanted = atan2f(aim.z - eye.z, dxy) * 0.9f + s_pitchBias + s_drift * 0.4f;
        quickness = bReacting ? 4.5f : (bMelee ? 9.5f : 8.0f);
        if (now >= s_tFootwork) ap_newFootwork(now, bFighting, bMelee);
        float range = FT(ap_fightRangeFt(bHolding ? s_weapon : curWeapon));
        if (bMelee && !ap_reachable(eye, aim)) range = FT(16.0f); // (out of reach: stood back, for the Force or a throw)
        const float keep = ap_clampf((dxy - range) / (bMelee ? FT(3.0f) : FT(8.0f)), -1.0f, 1.0f); // (to the fighting distance)
        switch (s_footwork) {
            case FW_STRAFE:   wantX = s_footDir * s_footStrength; wantY = keep * (bMelee ? 0.9f : 0.6f); break;
            case FW_CIRCLE:   wantX = s_footDir * s_footStrength; wantY = keep * 0.8f + (bMelee ? 0.0f : 0.12f); break;
            case FW_DIAGONAL: wantX = s_footDir * s_footStrength; wantY = (fabsf(keep) > 0.5f) ? keep : s_footIn; break;
            case FW_PAUSE:    wantX = 0.0f; wantY = (fabsf(keep) > 0.8f) ? keep * 0.5f : 0.0f; break;
            case FW_JUKE:     wantX = s_footDir; wantY = 0.0f; break;
        }
        // (closing in from afar with the saber: straight at them, running)
        if (bMelee && dxy > range + FT(10.0f) && ap_reachable(eye, aim)) { wantX *= 0.3f; wantY = 1.0f; }
        if (s_footwork == FW_CIRCLE) quickness = 9.5f;
        if (fabsf(ap_wrap(yawWanted - yaw)) > 1.0f) wantY *= 0.3f;
    }
    else {
        // no one in sight (the target is kept 1.5 s after it's gone): the
        // group's middle, those of it still about, gone to if it's far (or,
        // with none left there, the nearest anywhere); a glance to one side
        // now and then, as a player checking the corners
        rdVector3 goal = s_center, sum = ap_v3(0, 0, 0);
        int count = 0;
        for (int i = 0; i < s_numFoes; i++) {
            if (ap_lenXY(ap_sub(s_foes[i].pos, s_center)) < FT(AP_DOWN_FT) && fabsf(s_foes[i].pos.z - s_center.z) < FT(40.0f)) {
                sum = ap_add(sum, s_foes[i].pos);
                count++;
            }
        }
        if (count > 0) goal = ap_mul(sum, 1.0f / (float)count);
        else {
            float best = 1e9f;
            for (int i = 0; i < s_numFoes; i++) {
                const float dd = ap_lenXY(ap_sub(s_foes[i].pos, feet)) + fabsf(s_foes[i].pos.z - feet.z) * 2.0f;
                if (dd < best) { best = dd; goal = s_foes[i].pos; }
            }
        }
        const float d = ap_lenXY(ap_sub(goal, feet));
        float toGoal = (d > FT(1.0f)) ? ap_headingTo(feet, goal) : yaw;
        // (a wall that way: the way round it, the nearest open heading to it,
        // kept a moment so the view doesn't hunt about)
        if (now >= s_tSearch) {
            s_tSearch = now + ap_randRange(0.8f, 1.6f);
            s_searchOff = 0.0f;
            if (ap_openFt(eyeSec, eye, toGoal, 16.0f) < 8.0f) {
                const float side = ap_randInt(2) ? 1.0f : -1.0f;
                for (int k = 1; k <= 9; k++) {
                    const float off = side * (k % 2 ? 1.0f : -1.0f) * 0.35f * (float)((k + 1) / 2);
                    if (ap_openFt(eyeSec, eye, toGoal + off, 16.0f) >= 12.0f) { s_searchOff = off; break; }
                }
            }
        }
        toGoal += s_searchOff;
        if (now >= s_tGlance) {
            s_glance = ap_randInt(9) < 4 ? 0.0f : (ap_randInt(2) ? 1.0f : -1.0f) * ap_randRange(0.45f, 1.1f);
            // (never at a wall right in front of him: somewhere with room to look at)
            if (s_glance != 0.0f && ap_openFt(eyeSec, eye, toGoal + s_glance, 16.0f) < 12.0f) s_glance = 0.0f;
            s_tGlance = now + ap_randRange(1.0f, 2.8f);
        }
        yawWanted = toGoal + ((d > FT(10.0f)) ? s_glance * 0.3f : s_glance);
        pitchWanted = -0.04f + s_drift;
        quickness = 5.5f;
        if (d > FT(10.0f) || s_searchOff != 0.0f) wantY = fabsf(ap_wrap(toGoal - yaw)) < 0.6f ? 1.0f : 0.3f;
    }

    // keep off ledges, pits and water, and out of walls; a way that's shut
    // makes for other footwork (what it wanted counts for being stuck)
    const float wanted = fabsf(wantX) + fabsf(wantY);
    const float sy = sinf(yaw), cy = cosf(yaw);
    // (forward is (sin, cos) of the heading; right is (cos, -sin))
    if (wantY > 0.05f && !ap_canGo(feet, sec, sy, cy)) wantY = 0.0f;
    if (wantY < -0.05f && !ap_canGo(feet, sec, -sy, -cy)) wantY = 0.0f;
    if (fabsf(wantX) > 0.05f) {
        const float side = wantX > 0.0f ? 1.0f : -1.0f;
        if (!ap_canGo(feet, sec, cy * side, -sy * side)) {
            wantX = 0.0f;
            if (tgt && now + 0.15 < s_tFootwork) { s_tFootwork = now + 0.15; s_footDir = -side; }
        }
    }
    if (fabsf(wantX) > 0.05f && fabsf(wantY) > 0.05f) {
        float dx = sy * wantY + cy * wantX, dy = cy * wantY - sy * wantX;
        const float l = sqrtf(dx * dx + dy * dy);
        if (l > 1e-3f && !ap_canGo(feet, sec, dx / l, dy / l)) {
            if (fabsf(wantX) < fabsf(wantY)) wantX = 0.0f; else wantY = 0.0f;
        }
    }
    // (a run carrying on towards an edge anyway: the stick pushed back against it)
    {
        const float mx = feet.x - s_prevFeet.x, my = feet.y - s_prevFeet.y;
        const float ml = sqrtf(mx * mx + my * my);
        if (ml > FT(0.02f) && ml < FT(2.0f) && !ap_canGo(feet, sec, mx / ml, my / ml)) {
            wantY = -ap_clampf((mx * sy + my * cy) / ml, -1.0f, 1.0f) * 0.9f;
            wantX = -ap_clampf((mx * cy - my * sy) / ml, -1.0f, 1.0f) * 0.9f;
        }
        s_prevFeet = feet;
    }

    // not getting anywhere: other footwork; with no one in sight, turn away and hop
    if (now - s_tMoved >= 0.6) {
        const float moved = ap_lenXY(ap_sub(feet, s_lastPos));
        if (wanted > 0.6f && moved < FT(0.5f)) {
            s_stuck++;
            s_tFootwork = now;
            if (s_stuck >= 3 && !tgt) {
                s_stuck = 0;
                // (turned away to the most open way of a few)
                float bestOpen = -1.0f;
                for (int k = 0; k < 6; k++) {
                    const float h = yaw + (ap_randInt(2) ? 1.0f : -1.0f) * ap_randRange(1.2f, 3.0f);
                    const float open = ap_openFt(eyeSec, eye, h, 24.0f) + ap_rand01();
                    if (open > bestOpen) { bestOpen = open; s_evadeYaw = h; }
                }
                s_tEvade = now + ap_randRange(1.2f, 1.8f);
                if (ap_randInt(2)) s_ctl.jump = 1;
            }
            else if (s_stuck >= 4) s_stuck = 0;
        }
        else if (moved >= FT(0.5f)) s_stuck = 0;
        s_lastPos = feet;
        s_tMoved = now;
    }

    ap_steer(pl, yawWanted, pitchWanted, dt, quickness);
    ap_ease(&s_axisX, wantX, dt);
    ap_ease(&s_axisY, wantY, dt);
    s_ctl.strafe = s_axisX;
    s_ctl.forward = s_axisY;
    s_ctl.duck = now < s_tCrouch;

    // fire: once the aim has settled on the target, in bursts of varied
    // length; a short tail once it's lost; a detonator held to throw as far
    // as the bunch; with the saber, swings in reach
    const float err = fabsf(ap_wrap(ap_headingTo(eye, aim) - yaw));
    const float tol = ap_maxf(0.05f, atan2f(FT(1.6f), dxy));
    if (bFighting && err < tol && fabsf(s_yawVel) < 1.5f) { if (s_tSettled <= 0.0) s_tSettled = now; }
    else if (!bFighting || err > tol * 2.5f) s_tSettled = 0.0;
    const int bSettled = s_tSettled > 0.0 && now - s_tSettled >= 0.1;
    int fire = 0, alt = 0;
    if (tgt && bHolding && !bEvading && !bReacting && s_forceStage == FP_NONE) {
        if (ap_isThrown(s_weapon)) {
            if (s_tRelease > 0.0) {
                fire = now < s_tRelease;
                if (!fire) {
                    s_tRelease = 0.0;
                    s_tThrow = now + ap_randRange(0.9f, 1.5f);
                    if (s_throws > 0 && --s_throws == 0) {
                        ap_takeWeapon(s_before);
                        s_tDetsNext = now + ap_randRange(6.0f, 10.0f);
                    }
                }
            }
            else if (bSettled && now >= s_tThrow && dxy < FT(60.0f)) {
                // (held longer for further: the throw from short to far)
                s_tRelease = now + ap_clampf(0.25f + (dxy / s_ft - 12.0f) / 40.0f * 0.5f, 0.25f, 0.8f);
                fire = 1;
            }
        }
        else if (ap_isMine(s_weapon)) {
            fire = bSettled && fmod(now, 2.5) < 0.3;
        }
        else if (bMelee) {
            // swings in reach: a flurry, a pause, now and then the other swing
            if (dxy < FT(ap_fightRangeFt(s_weapon) + 2.5f) && err < 0.5f) {
                if (now >= s_tPause) {
                    s_tBurst = now + ap_randRange(0.25f, 1.1f);
                    s_tPause = s_tBurst + ap_randRange(0.15f, 0.7f);
                    s_bBurstAlt = s_weapon == AP_W_SABER && ap_randInt(10) < 4;
                }
                fire = now < s_tBurst;
                if (fire && s_bBurstAlt) { fire = 0; alt = 1; }
            }
        }
        else if (dxy < FT(90.0f) && (bSettled || (now < s_tBurst && err < tol * 2.5f))) {
            if (now >= s_tPause) {
                s_tBurst = now + ap_randRange(0.35f, 2.0f);
                s_tPause = s_tBurst + ap_randRange(0.15f, 0.6f);
                s_bBurstAlt = ap_hasAlt(s_weapon) && ap_randInt(10) < 3;
            }
            fire = now < s_tBurst;
            if (fire && s_bBurstAlt) { fire = 0; alt = 1; }
        }
    }
    // (a short tail as the target goes out of sight)
    if ((fire || alt) && s_bTargetInSight) { s_tFireTail = now + ap_randRange(0.12f, 0.35f); s_bTailAlt = alt; }
    else if (!fire && !alt && now < s_tFireTail && bHolding && !ap_isThrown(s_weapon) && !ap_isMine(s_weapon) && !bMelee) {
        if (s_bTailAlt) alt = 1; else fire = 1;
    }
    s_ctl.fire = fire;
    s_ctl.alt = alt;

    ap_force(now, pl, targetD, bFighting, bHolding, bSettled, tgt != NULL && !ap_reachable(eye, s_aim));
}

// The controls into the game: the stick as its keys (pulsed, as a thumb part
// way over: the walk picks up and dies down as the stick does), running when
// it's pushed far; the buttons; the turning as mouse movement
static void ap_input(double now, float dt)
{
    memset(s_aKeys, 0, sizeof(s_aKeys));
    const float fx = s_ctl.strafe, fy = s_ctl.forward;
    s_pwmX += fabsf(fx);
    s_pwmY += fabsf(fy);
    if (s_pwmX >= 1.0f) { s_pwmX -= 1.0f; s_aKeys[fx > 0.0f ? AP_KEY_D : AP_KEY_A] = 1; }
    if (s_pwmY >= 1.0f) { s_pwmY -= 1.0f; s_aKeys[fy > 0.0f ? AP_KEY_W : AP_KEY_S] = 1; }
    if (fabsf(fx) < 0.05f) s_pwmX = 0.0f;
    if (fabsf(fy) < 0.05f) s_pwmY = 0.0f;
    if (fabsf(fx) > 0.75f || fabsf(fy) > 0.75f) s_aKeys[AP_KEY_RUN] = 1;
    s_aKeys[AP_KEY_FIRE] = s_ctl.fire != 0;
    s_aKeys[AP_KEY_ALT] = s_ctl.alt != 0;
    s_aKeys[AP_KEY_DUCK] = s_ctl.duck != 0;
    s_aKeys[AP_KEY_FORCE] = s_ctl.force != 0;
    if (s_ctl.jump) {
        s_aKeys[AP_KEY_JUMP] = 1;
        s_tShowJump = now + 0.25;
        s_ctl.jump = 0;
    }
    (void)dt;
    const int dx = (int)s_ctl.lookX, dy = (int)s_ctl.lookY;
    s_ctl.lookX -= (float)dx;
    s_ctl.lookY -= (float)dy;
    Window_lastXRel += dx;
    Window_lastYRel += dy;
}

void iosAutoplay_GetDisplay(iosAutoplayDisplay* pOut)
{
    memset(pOut, 0, sizeof(*pOut));
    if (s_stage != ST_SCENE || !s_opt.bControls) return;
    const double now = s_tPrev;
    pOut->bOn = 1;
    pOut->moveX = s_axisX;
    pOut->moveY = s_axisY;
    pOut->bRun = s_axisY > 0.75f && fabsf(s_axisX) < 0.3f;
    unsigned int b = 0;
    if (s_ctl.fire) b |= IOSAUTOPLAY_BTN_FIRE;
    if (s_ctl.alt) b |= IOSAUTOPLAY_BTN_ALT;
    if (s_ctl.duck) b |= IOSAUTOPLAY_BTN_DUCK;
    if (s_ctl.force) b |= IOSAUTOPLAY_BTN_FORCE;
    if (now < s_tShowJump) b |= IOSAUTOPLAY_BTN_JUMP;
    if (now < s_tShowSwitch) b |= IOSAUTOPLAY_BTN_NEXTWPN;
    if (now < s_tShowWheel) b |= IOSAUTOPLAY_BTN_WHEEL;
    pOut->buttons = b;
}

// ------------------------------------------------------------ once a frame

#ifdef IOSAUTOPLAY_RIG
// The Linux test rig: START from the environment (APTEST_START=seconds,
// APTEST_ROW, APTEST_WEAPONS hex, APTEST_CROWD), double taps at
// APTEST_NEXT=seconds,... and the long press at APTEST_STOP=seconds
static void ap_rigTick(double now)
{
    static double tFirst = 0.0;
    static int bStarted = 0, nextIdx = 0, bStopped = 0;
    if (!ap_inPlay()) return;
    if (tFirst <= 0.0) tFirst = now;
    const char* pStart = getenv("APTEST_START");
    if (pStart && !bStarted && now - tFirst >= atof(pStart)) {
        iosAutoplayOptions o;
        o.levelRow = getenv("APTEST_ROW") ? atoi(getenv("APTEST_ROW")) : -1;
        o.difficulty = 2;
        o.crowd = getenv("APTEST_CROWD") ? atoi(getenv("APTEST_CROWD")) : 2;
        o.bAlarm = 1;
        o.weapons = getenv("APTEST_WEAPONS") ? (unsigned int)strtoul(getenv("APTEST_WEAPONS"), NULL, 16) : IOSAUTOPLAY_WEAPONS_ALL;
        o.bHud = getenv("APTEST_HUD") != NULL;
        o.bControls = 0;
        bStarted = 1;
        APLOG("before: sithewok %d, difficulty %d, saber style %d hilts %d %d\n", jkDev_Custom_SithEwokIsOn(), jkPlayer_setDiff,
              iosSaber_GetStyle(), iosSaber_GetHandHilt(0), iosSaber_GetHandHilt(1));
        APLOG("rig START: %d\n", iosAutoplay_Start(&o));
        tFirst = now;
    }
    if (!bStarted) return;
    const char* pNext = getenv("APTEST_NEXT");
    if (pNext) {
        const char* p = pNext;
        for (int i = 0; i < nextIdx && p; i++) { p = strchr(p, ','); if (p) p++; }
        if (p && *p && now - tFirst >= atof(p)) {
            APLOG("rig DOUBLE TAP\n");
            iosAutoplay_Next();
            nextIdx++;
        }
    }
    const char* pStop = getenv("APTEST_STOP");
    if (pStop && !bStopped && now - tFirst >= atof(pStop)) {
        bStopped = 1;
        APLOG("rig LONG PRESS\n");
        iosAutoplay_Stop();
    }
    static int bReported = 0;
    if (bStopped && !bReported && now - tFirst >= atof(pStop) + 8.0) {
        bReported = 1;
        APLOG("after: level %s, sithewok %d, difficulty %d, saber style %d hilts %d %d, in play %d\n",
              sithWorld_g_pCurrentWorld ? sithWorld_g_pCurrentWorld->map_jkl_fname : "-", jkDev_Custom_SithEwokIsOn(), jkPlayer_setDiff,
              iosSaber_GetStyle(), iosSaber_GetHandHilt(0), iosSaber_GetHandHilt(1), ap_inPlay());
    }
}
#endif

void iosAutoplay_Tick(double now)
{
    const float dt = (float)ap_clampf((float)(now - s_tPrev), 0.0f, 0.1f);
    s_tPrev = now;
#ifdef IOSAUTOPLAY_RIG
    ap_rigTick(now);
#endif
    if (s_tRestoring > 0.0 && now > s_tRestoring) s_tRestoring = 0.0;
    if (s_stage == ST_OFF) return;
    if (s_stage != ST_SCENE) memset(s_aKeys, 0, sizeof(s_aKeys));

    if (s_stage == ST_SAVING) {
        // (the save is written as the game next runs: then on to the level)
        if (sithGamesave_state == SITH_GS_NONE || now - s_tStage > 3.0) {
            if (!jkDev_Custom_SithEwokIsOn()) jkDev_Custom_CmdSithEwok(NULL, "");
            ap_travel(now);
        }
        return;
    }
    // a level opened (a new level's player): its fight in a moment, once the
    // cheat has given everything again
    SithThing* pl = ap_player();
    if (s_stage == ST_TRAVEL) {
        s_bLoaded = 0;
        if (s_bOpened && pl && ap_inPlay()) {
            s_bOpened = 0;
            s_stage = ST_ARRIVE;
            s_tStage = now;
            s_copies = 0;
            s_bWokeDisabled = 0;
            ap_collectFoes();
            ap_countKinds();
        }
        else if (now - s_tStage > 120.0) {
            APLOG("the level never came\n");
            ap_end(0);
        }
        return;
    }
    // (a game loaded or a level opened some other way -- the menu's Load --
    // or back to the main menu: the show is over, the player's game theirs)
    if (s_bOpened || s_bLoaded || jkSmack_currentGuiState == JK_GAMEMODE_MAIN || jkSmack_currentGuiState == JK_GAMEMODE_TITLE) {
        APLOG("the player went elsewhere\n");
        ap_end(0);
        return;
    }
    if (!pl || !ap_inPlay()) {
        ap_clearControls();
        return;
    }
    if (iosGame_IsHolding()) return;
    if (!jkDev_Custom_SithEwokIsOn()) jkDev_Custom_CmdSithEwok(NULL, ""); // (a load or a level script took it)
    if (s_stage == ST_ARRIVE) {
        if (now - s_tStage >= 1.0) ap_startScene(now, 0);
        return;
    }
    if (pl->flags & SITH_TF_DEAD) { ap_clearControls(); return; }

    if (s_bWantNext) {
        s_bWantNext = 0;
        APLOG("NEXT at %.1f s\n", now - s_tStart);
        if (s_opt.levelRow >= 0) ap_startScene(now, 1);
        else ap_travel(now);
        return;
    }

    ap_autopilot(now, dt, pl);
    ap_input(now, dt);

#ifdef IOSAUTOPLAY_RIG
    static double tDbg = 0.0;
    if (now >= tDbg) {
        tDbg = now + 0.5;
        SithThing* t = ap_targetThing();
        const rdVector3 eye = ap_v3(pl->position.x, pl->position.y, pl->position.z + pl->actorParams.eyeOffset.z);
        APLOG("t %.1f target %d seen %d wpn %d/%d power %d fw %d x %.2f y %.2f yaw %.2f pitch %.2f dist %.1f ft open %.0f evade %d\n", now - s_tScene, t ? 1 : 0,
              s_numSeen, iosGame_GetCurWeapon(), s_weapon, s_power, s_footwork, s_axisX, s_axisY, ap_yawOf(pl), ap_pitchOf(pl),
              t ? ap_len(ap_sub(s_aim, pl->position)) / s_ft : -1.0f, ap_openFt(pl->sector, eye, ap_yawOf(pl), 32.0f), now < s_tEvade);
    }
#endif

    // no one left where the group was (for a moment), or no one in sight for
    // a while: more of their kinds come in, from out of sight; lost (fallen
    // somewhere, wandered far, stuck): put back near the group, quietly
    if (now >= s_tCheck) {
        s_tCheck = now + 0.25;
        const rdVector3 feet = pl->position;
        if (ap_lenXY(ap_sub(feet, s_farPos)) > FT(6.0f)) { s_farPos = feet; s_tMovedFar = now; }
        const int left = ap_foesAround(s_center, FT(AP_DOWN_FT)) + s_numSeen;
        if (left == 0) { if (s_tDown <= 0.0) s_tDown = now; }
        else s_tDown = 0.0;
        const int bLull = !ap_targetThing() && now - s_tSeen > 4.0;
        if (((s_tDown > 0.0 && now - s_tDown >= 1.5) || bLull) && now >= s_tWave) {
            const int n = ap_reinforce(pl);
            s_tWave = now + (n > 0 ? 8.0 : 2.0);
            s_tRally = now + 1.0;
            APLOG("wave at %.1f s: %d come in\n", now - s_tStart, n);
        }
        // (no one in sight for a moment: those about are called to him, as
        // the noise of a fight would -- straight to him where they can walk
        // it, else to where the group was; a level's enemies don't find a way
        // round on their own)
        if (!ap_targetThing() && now - s_tSeen > 2.5 && now >= s_tRally) {
            s_tRally = now + 2.0;
            ap_rally(pl);
        }
        const int bLost = (!ap_targetThing() && now - s_tSeen > 6.0 && ap_lenXY(ap_sub(feet, s_center)) > FT(100.0f))
                          || s_center.z - feet.z > FT(24.0f) || (s_center.z - feet.z > FT(8.0f) && !ap_targetThing() && now - s_tSeen > 3.0)
                          || (!ap_targetThing() && now - s_tMovedFar > 12.0);
        if (bLost && now >= s_tReplace) {
            s_tReplace = now + 8.0;
            apSpot spot;
            if (ap_findSpot(&s_group, &spot)) {
                APLOG("put back at %.1f s\n", now - s_tStart);
                ap_place(pl, &spot);
                ap_resetPilot(now, spot.feet);
            }
        }
    }
}

#endif // JK_ENHANCED
