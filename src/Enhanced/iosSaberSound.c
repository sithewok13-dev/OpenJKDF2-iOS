// Added: entire file. See iosSaberSound.h.

#ifdef JK_ENHANCED

#include "Enhanced/iosSaberSound.h"

#include <string.h>

#include "Enhanced/iosSaber.h"
#include "Devices/sithSoundMixer.h"
#include "General/stdHashtbl.h"
#include "General/stdString.h"
#include "Gameplay/sithTime.h"
#include "stdPlatform.h"
#include "jk.h"

// ------------------------------------------------------------ the sound

// The hum played this much lower, its pitch wandering this much either way
// (a new aim every so often, glided to); the deeper copy under it this low
// and loud (of the hum's), wandering more, its loudness too
#define IOSSABERSOUND_HUM_PITCH 0.84f
#define IOSSABERSOUND_HUM_WANDER 0.035f
#define IOSSABERSOUND_GROWL_PITCH 0.56f
#define IOSSABERSOUND_GROWL_WANDER 0.06f
#define IOSSABERSOUND_GROWL_VOLUME 0.42f
#define IOSSABERSOUND_GROWL_SWELL 0.35f // (its loudness, either way, as a part of it)
#define IOSSABERSOUND_WANDER_MIN_MS 60
#define IOSSABERSOUND_WANDER_MAX_MS 150
// The crackle: one every so often (now and then two close together), each a
// snippet of a saber hit sound this high and loud, cut off this soon
#define IOSSABERSOUND_CRACKLE_MIN_MS 140
#define IOSSABERSOUND_CRACKLE_MAX_MS 520
#define IOSSABERSOUND_CRACKLE_DOUBLE 0.25f
#define IOSSABERSOUND_CRACKLE_PITCH_MIN 1.35f
#define IOSSABERSOUND_CRACKLE_PITCH_MAX 2.10f
#define IOSSABERSOUND_CRACKLE_VOL_MIN 0.10f
#define IOSSABERSOUND_CRACKLE_VOL_MAX 0.22f
#define IOSSABERSOUND_CRACKLE_LEN_MIN_MS 30
#define IOSSABERSOUND_CRACKLE_LEN_MAX_MS 75
#define IOSSABERSOUND_CRACKLE_CUT_SEC 0.02f
// A sputter now and then: the hum dips this far for a moment, the copy under
// it swells, a few crackles close together
#define IOSSABERSOUND_SPUTTER_MIN_MS 1800
#define IOSSABERSOUND_SPUTTER_MAX_MS 4500
#define IOSSABERSOUND_SPUTTER_DIP 0.86f
#define IOSSABERSOUND_SPUTTER_CRACKLES 3
// Igniting: the game's ignition a little lower, the same again much lower a
// moment later and cut short, and a burst of crackles; switching off and
// swinging, a little lower (a swing now and then with a crackle)
#define IOSSABERSOUND_ON_PITCH 0.88f
#define IOSSABERSOUND_ON_ECHO_PITCH 0.70f
#define IOSSABERSOUND_ON_ECHO_VOLUME 0.45f
#define IOSSABERSOUND_ON_ECHO_MS 50
#define IOSSABERSOUND_ON_ECHO_LEN_MS 320
#define IOSSABERSOUND_ON_CRACKLES 6
#define IOSSABERSOUND_OFF_PITCH 0.90f
#define IOSSABERSOUND_OFF_CRACKLES 2
#define IOSSABERSOUND_SWING_PITCH 0.93f
#define IOSSABERSOUND_SWING_CRACKLE 0.5f
// (none of its own started while the game's sounds have fewer than this many channels free)
#define IOSSABERSOUND_SPARE_CHANNELS 4
// (the game's clock seen up to this far back is no load: a sound started from the
// controls, as a saber throw is, has the frame's time, and the game's steps after
// it start from a frame earlier, sithMain_Tick: a frame of up to SITHTIME_MAXDELTA)
#define IOSSABERSOUND_CLOCK_SLACK_MS (SITHTIME_MAXDELTA + 50)

// The game's saber sounds, by name (both games' saber cogs': LtSaberLp01.WAV
// the hum, LtSaberOn01, LtSaberOff01, LtSaberSwing01-08), and the hit sounds
// the crackle is made of (the cogs load them)
enum { IOSSABERSOUND_NONE = 0, IOSSABERSOUND_HUM, IOSSABERSOUND_ON, IOSSABERSOUND_OFF, IOSSABERSOUND_SWING };
// (kept by the game by their names in small letters; the sparks' own two among them)
static const char* const iosSaberSound_aCrackle[] = { "ltsaberhit01.wav", "ltsaberhit02.wav", "ltsaberhit03.wav", "ltsaberhit06.wav",
                                                      "ltsaberhit07.wav", "ltsaberhit12.wav", "ltsaberhit14.wav" };
#define IOSSABERSOUND_NUM_CRACKLE_SOUNDS ((int)(sizeof(iosSaberSound_aCrackle) / sizeof(iosSaberSound_aCrackle[0])))

static int iosSaberSound_KindOf(const sithSound* s)
{
    const char* n = s->sound_fname;
    if (!__strnicmp(n, "LtSaberLp", 9)) return IOSSABERSOUND_HUM;
    if (!__strnicmp(n, "LtSaberOn", 9)) return IOSSABERSOUND_ON;
    if (!__strnicmp(n, "LtSaberOff", 10)) return IOSSABERSOUND_OFF;
    if (!__strnicmp(n, "LtSaberSwing", 12)) return IOSSABERSOUND_SWING;
    return IOSSABERSOUND_NONE;
}

// (its own random numbers: the game's are left as they would be)
static uint32_t iosSaberSound_seed = 0x2545F491u;
static float iosSaberSound_Rand(void)
{
    iosSaberSound_seed = iosSaberSound_seed * 1664525u + 1013904223u;
    return (float)(iosSaberSound_seed >> 8) * (1.0f / 16777216.0f);
}
static float iosSaberSound_Between(float a, float b)
{
    return a + (b - a) * iosSaberSound_Rand();
}
static uint32_t iosSaberSound_After(uint32_t now, int a, int b)
{
    return now + (uint32_t)(a + (int)((float)(b - a) * iosSaberSound_Rand()));
}

// ------------------------------------------------------------ its channels

// A channel still what it was started as: the same sound on the same thing,
// and its own id (the mixer's, unique to each sound it starts; 0: not known
// yet, for the game's hum: a sound sent over the network gets its sender's id
// just after it starts)
typedef struct {
    sithPlayingSound* p;
    sithSound* pSound;
    SithThing* pThing;
    int32_t refid;
} iosSaberSoundCh;

static int iosSaberSound_Alive(const iosSaberSoundCh* c)
{
    return c->p && c->pSound && c->p->sound == c->pSound && c->p->thing == c->pThing && (!c->refid || c->p->refid == c->refid);
}

static int iosSaberSound_bStarting = 0;

// One of its own on a thing (looping or not), as loud and as far heard as the
// game's sound it goes with; 0 if none (no channels to spare, or out of earshot)
static int iosSaberSound_Start(iosSaberSoundCh* out, sithSound* s, SithThing* t, float volume, float pitch, float minR, float maxR, int bLoop)
{
    memset(out, 0, sizeof(*out));
    if (!s || !t || sithSoundMixer_activeChannels + IOSSABERSOUND_SPARE_CHANNELS > (int)jkGuiSound_numChannels) return 0;
    iosSaberSound_bStarting = 1;
    sithPlayingSound* p = sithSoundMixer_PlaySoundThing(s, t, volume, minR, maxR, SITHSOUNDFLAG_FOLLOWSTHING | (bLoop ? SITHSOUNDFLAG_LOOP : 0));
    iosSaberSound_bStarting = 0;
    if (!p) return 0;
    sithSoundMixer_SetPitch(p, pitch, 0.0f);
    out->p = p;
    out->pSound = s;
    out->pThing = t;
    out->refid = p->refid;
    return 1;
}

// (faded out from now, and gone when silent; its own still while it fades)
#define IOSSABERSOUND_MAX_FADING 16
static iosSaberSoundCh iosSaberSound_aFading[IOSSABERSOUND_MAX_FADING];
static int iosSaberSound_nextFading = 0;

static void iosSaberSound_FadeOut(iosSaberSoundCh* c, float sec)
{
    if (iosSaberSound_Alive(c)) {
        if (c->p->vol_2 <= 0.001f || sec <= 0.0f) {
            sithSoundMixer_StopSound(c->p); // (already silent: a fade to where it is would do nothing)
        } else {
            sithSoundMixer_FadeVolume(c->p, 0.0f, sec);
            c->p->flags |= SITHSOUNDFLAG_FADING;
            iosSaberSound_aFading[iosSaberSound_nextFading] = *c;
            iosSaberSound_nextFading = (iosSaberSound_nextFading + 1) % IOSSABERSOUND_MAX_FADING;
        }
    }
    c->p = NULL;
}

// The crackles sounding now, each cut off at its time (by the real clock: the
// sounds play on while the game holds still, a wheel open)
#define IOSSABERSOUND_MAX_CRACKLES 16
typedef struct {
    iosSaberSoundCh ch;
    uint32_t cutMs;
} iosSaberSoundCrackle;
static iosSaberSoundCrackle iosSaberSound_aCrackles[IOSSABERSOUND_MAX_CRACKLES];

static void iosSaberSound_Crackle(SithThing* t, float minR, float maxR)
{
    iosSaberSoundCrackle* c = NULL;
    for (int i = 0; i < IOSSABERSOUND_MAX_CRACKLES && !c; i++)
        if (!iosSaberSound_Alive(&iosSaberSound_aCrackles[i].ch)) c = &iosSaberSound_aCrackles[i];
    if (!c || !sithSound_hashtable) return;
    // (one of those the level has, at random)
    sithSound* s = NULL;
    for (int k = 0, i = (int)(iosSaberSound_Rand() * IOSSABERSOUND_NUM_CRACKLE_SOUNDS); k < IOSSABERSOUND_NUM_CRACKLE_SOUNDS && !s; k++)
        s = (sithSound*)stdHashtbl_Find(sithSound_hashtable, iosSaberSound_aCrackle[(i + k) % IOSSABERSOUND_NUM_CRACKLE_SOUNDS]);
    if (!iosSaberSound_Start(&c->ch, s, t, iosSaberSound_Between(IOSSABERSOUND_CRACKLE_VOL_MIN, IOSSABERSOUND_CRACKLE_VOL_MAX),
                             iosSaberSound_Between(IOSSABERSOUND_CRACKLE_PITCH_MIN, IOSSABERSOUND_CRACKLE_PITCH_MAX), minR, maxR, 0))
        return;
    c->cutMs = iosSaberSound_After(stdPlatform_GetTimeMsec(), IOSSABERSOUND_CRACKLE_LEN_MIN_MS, IOSSABERSOUND_CRACKLE_LEN_MAX_MS);
}

// ------------------------------------------------------------ each saber's

// Each crossguard saber heard (the player's own, and in multiplayer others'):
// the game's hum it goes with, the deeper copy under it, what comes next
#define IOSSABERSOUND_SLOTS 8
typedef struct {
    SithThing* pThing;
    uint32_t signature;
    uint32_t usedMs;
    float minR, maxR;       // (as far as its sounds are heard: the hum's)
    iosSaberSoundCh hum;    // the game's (any player's: a style changed to CROSS while lit puts it on)
    int bCross;             // its sound made rougher now
    iosSaberSoundCh growl;  // the copy under it
    uint32_t nextWander, nextCrackle, nextSputter;
    int burst;              // crackles still to come close together (igniting, a sputter, switching off)
    uint32_t nextBurst;
    int bOffBurst;          // (switching off's: they outlast the hum)
    sithSound* pEchoSound;  // the ignition again, lower, from its time
    uint32_t echoMs, echoCutMs;
    iosSaberSoundCh echo;
    int bTimed;
    uint32_t lastMs;
} iosSaberSoundSaber;
static iosSaberSoundSaber iosSaberSound_aSaber[IOSSABERSOUND_SLOTS];

static iosSaberSoundSaber* iosSaberSound_SaberOf(SithThing* t, uint32_t now)
{
    iosSaberSoundSaber* s = &iosSaberSound_aSaber[0];
    for (int i = 0; i < IOSSABERSOUND_SLOTS; i++) {
        iosSaberSoundSaber* a = &iosSaberSound_aSaber[i];
        if (a->pThing == t && a->signature == t->signature) { a->usedMs = now; return a; }
        if (!a->pThing || (s->pThing && (int32_t)(a->usedMs - s->usedMs) < 0)) s = a;
    }
    iosSaberSound_FadeOut(&s->growl, 0.1f);
    iosSaberSound_FadeOut(&s->echo, 0.1f);
    memset(s, 0, sizeof(*s));
    s->pThing = t;
    s->signature = t->signature;
    s->usedMs = now;
    s->minR = 0.5f;
    s->maxR = 2.5f;
    return s;
}

static int iosSaberSound_IsCross(SithThing* t)
{
    return iosSaber_HandHilt(t, 0) == IOSSABER_HILT_CROSS || iosSaber_HandHilt(t, 1) == IOSSABER_HILT_CROSS;
}

// The hum made rougher: lower, and the deeper copy of it under it (one at a time)
// (glided there over sec: a hum already sounding)
static void iosSaberSound_Roughen(iosSaberSoundSaber* s, uint32_t now, float sec)
{
    iosSaberSound_FadeOut(&s->growl, 0.05f);
    sithSoundMixer_SetPitch(s->hum.p, IOSSABERSOUND_HUM_PITCH, sec);
    iosSaberSound_Start(&s->growl, s->hum.pSound, s->pThing, s->hum.p->vol_2 * IOSSABERSOUND_GROWL_VOLUME, IOSSABERSOUND_GROWL_PITCH, s->minR, s->maxR, 1);
    s->nextWander = now;
    s->nextCrackle = iosSaberSound_After(now, IOSSABERSOUND_CRACKLE_MIN_MS, IOSSABERSOUND_CRACKLE_MAX_MS);
    s->nextSputter = iosSaberSound_After(now, IOSSABERSOUND_SPUTTER_MIN_MS, IOSSABERSOUND_SPUTTER_MAX_MS);
    s->bCross = 1;
}

// ...and back to the game's (the style changed while lit)
static void iosSaberSound_Plain(iosSaberSoundSaber* s)
{
    if (iosSaberSound_Alive(&s->hum)) sithSoundMixer_SetPitch(s->hum.p, 1.0f, 0.1f);
    iosSaberSound_FadeOut(&s->growl, 0.1f);
    s->burst = 0;
    s->pEchoSound = NULL;
    s->bCross = 0;
}

void iosSaberSound_Played(sithPlayingSound* pPlaying, sithSound* pSound, SithThing* pThing, int flags)
{
    // (a player's saber: only players have a style of their own)
    if (iosSaberSound_bStarting || !pPlaying || !pSound || !pThing || pThing->type != SITH_THING_PLAYER) return;
    const int kind = iosSaberSound_KindOf(pSound);
    if (kind == IOSSABERSOUND_NONE) return;
    const int bCross = iosSaberSound_IsCross(pThing);
    // (any player's hum is kept track of, so a style changed to CROSS while lit puts the rough one on; the rest only for CROSS)
    if (!bCross && !(kind == IOSSABERSOUND_HUM && (flags & SITHSOUNDFLAG_LOOP))) return;
    const uint32_t now = sithTime_g_msecGameTime;
    iosSaberSoundSaber* s = iosSaberSound_SaberOf(pThing, now);
    switch (kind) {
    case IOSSABERSOUND_HUM:
        if (!(flags & SITHSOUNDFLAG_LOOP)) break;
        // the game's hum, from now (a new one: a new copy)
        iosSaberSound_FadeOut(&s->growl, 0.05f);
        s->hum.p = pPlaying;
        s->hum.pSound = pSound;
        s->hum.pThing = pThing;
        s->hum.refid = 0; // (taken once it's settled: see iosSaberSoundCh)
        s->minR = pPlaying->anonymous_5;
        s->maxR = pPlaying->maxPosition;
        s->bCross = 0;
        s->bOffBurst = 0;
        if (bCross) iosSaberSound_Roughen(s, now, 0.0f);
        break;
    case IOSSABERSOUND_ON:
        sithSoundMixer_SetPitch(pPlaying, IOSSABERSOUND_ON_PITCH, 0.0f);
        s->minR = pPlaying->anonymous_5;
        s->maxR = pPlaying->maxPosition;
        s->pEchoSound = pSound;
        s->echoMs = now + IOSSABERSOUND_ON_ECHO_MS;
        s->burst = IOSSABERSOUND_ON_CRACKLES;
        s->nextBurst = now;
        break;
    case IOSSABERSOUND_OFF:
        sithSoundMixer_SetPitch(pPlaying, IOSSABERSOUND_OFF_PITCH, 0.0f);
        s->burst = IOSSABERSOUND_OFF_CRACKLES;
        s->nextBurst = iosSaberSound_After(now, 20, 60);
        s->bOffBurst = 1;
        s->pEchoSound = NULL;
        break;
    case IOSSABERSOUND_SWING:
        sithSoundMixer_SetPitch(pPlaying, IOSSABERSOUND_SWING_PITCH, 0.0f);
        if (iosSaberSound_Rand() < IOSSABERSOUND_SWING_CRACKLE && !s->burst) {
            s->burst = 1;
            s->nextBurst = iosSaberSound_After(now, 20, 60);
        }
        break;
    }
    s->bTimed = 1; // (its clock from now: after a load, from the hum played again)
    s->lastMs = now;
}

// The game's hum gone, or fading out (switched off, thrown): the copy under it
// with it, and the crackles still to come and the ignition's echo (not those of
// switching off, which outlast it)
static void iosSaberSound_HumGone(iosSaberSoundSaber* s, float sec)
{
    iosSaberSound_FadeOut(&s->growl, sec);
    s->hum.p = NULL;
    s->bCross = 0;
    if (s->bOffBurst) return;
    s->burst = 0;
    s->pEchoSound = NULL;
    iosSaberSound_FadeOut(&s->echo, 0.1f);
}

static void iosSaberSound_TickSaber(iosSaberSoundSaber* s, uint32_t now, uint32_t real)
{
    SithThing* t = s->pThing;
    if (s->hum.p) {
        if (!iosSaberSound_Alive(&s->hum)) {
            iosSaberSound_HumGone(s, 0.05f);
        } else if (s->hum.p->flags & SITHSOUNDFLAG_FADING) {
            iosSaberSound_HumGone(s, 0.5f);
        } else if (!s->hum.refid) {
            s->hum.refid = s->hum.p->refid;
        }
    }
    // (its style changed while lit: CROSS's from now, or the game's)
    const int bCross = iosSaberSound_IsCross(t);
    if (s->hum.p && bCross && !s->bCross) iosSaberSound_Roughen(s, now, 0.1f);
    if (s->bCross && !bCross) iosSaberSound_Plain(s);
    if (!bCross) s->burst = 0;
    if (!iosSaberSound_Alive(&s->growl)) s->growl.p = NULL;

    // igniting: the ignition again, lower, a moment later and cut short
    if (s->pEchoSound && (int32_t)(now - s->echoMs) >= 0) {
        iosSaberSound_FadeOut(&s->echo, 0.05f); // (one still going: the last ignition's)
        iosSaberSound_Start(&s->echo, s->pEchoSound, t, IOSSABERSOUND_ON_ECHO_VOLUME, IOSSABERSOUND_ON_ECHO_PITCH, s->minR, s->maxR, 0);
        s->echoCutMs = real + IOSSABERSOUND_ON_ECHO_LEN_MS;
        s->pEchoSound = NULL;
    }
    if (s->echo.p && ((int32_t)(real - s->echoCutMs) >= 0 || (int32_t)(s->echoCutMs - real) > 1000)) iosSaberSound_FadeOut(&s->echo, 0.15f);

    // crackles close together
    if (s->burst > 0 && (int32_t)(now - s->nextBurst) >= 0) {
        iosSaberSound_Crackle(t, s->minR, s->maxR);
        s->burst--;
        s->nextBurst = iosSaberSound_After(now, 30, 110);
    }

    if (!s->hum.p || !s->bCross) return;
    // the hum's pitch, and the copy's pitch and loudness, never quite steady
    if ((int32_t)(now - s->nextWander) >= 0) {
        const uint32_t next = iosSaberSound_After(now, IOSSABERSOUND_WANDER_MIN_MS, IOSSABERSOUND_WANDER_MAX_MS);
        const float sec = (float)(next - now) * 0.001f;
        sithSoundMixer_SetPitch(s->hum.p, IOSSABERSOUND_HUM_PITCH * (1.0f + IOSSABERSOUND_HUM_WANDER * (2.0f * iosSaberSound_Rand() - 1.0f)), sec);
        if (s->growl.p) {
            sithSoundMixer_SetPitch(s->growl.p, IOSSABERSOUND_GROWL_PITCH * (1.0f + IOSSABERSOUND_GROWL_WANDER * (2.0f * iosSaberSound_Rand() - 1.0f)), sec);
            sithSoundMixer_FadeVolume(s->growl.p, s->hum.p->vol_2 * IOSSABERSOUND_GROWL_VOLUME * (1.0f + IOSSABERSOUND_GROWL_SWELL * (2.0f * iosSaberSound_Rand() - 1.0f)), sec);
        }
        s->nextWander = next;
    }
    // a crackle now and then (two close together now and then)
    if ((int32_t)(now - s->nextCrackle) >= 0) {
        iosSaberSound_Crackle(t, s->minR, s->maxR);
        if (iosSaberSound_Rand() < IOSSABERSOUND_CRACKLE_DOUBLE && !s->burst) {
            s->burst = 1;
            s->nextBurst = iosSaberSound_After(now, 30, 70);
        }
        s->nextCrackle = iosSaberSound_After(now, IOSSABERSOUND_CRACKLE_MIN_MS, IOSSABERSOUND_CRACKLE_MAX_MS);
    }
    // and a sputter: a quick dip, the copy swelling, crackles
    if ((int32_t)(now - s->nextSputter) >= 0) {
        sithSoundMixer_SetPitch(s->hum.p, IOSSABERSOUND_HUM_PITCH * IOSSABERSOUND_SPUTTER_DIP, 0.04f);
        if (s->growl.p) sithSoundMixer_FadeVolume(s->growl.p, s->hum.p->vol_2 * IOSSABERSOUND_GROWL_VOLUME * (1.0f + IOSSABERSOUND_GROWL_SWELL), 0.04f);
        s->nextWander = now + 90;
        s->burst = IOSSABERSOUND_SPUTTER_CRACKLES;
        s->nextBurst = now + 20;
        s->nextSputter = iosSaberSound_After(now, IOSSABERSOUND_SPUTTER_MIN_MS, IOSSABERSOUND_SPUTTER_MAX_MS);
    }
}

void iosSaberSound_Tick(void)
{
    const uint32_t now = sithTime_g_msecGameTime, real = stdPlatform_GetTimeMsec();
    // (the crackles cut off at their times)
    for (int i = 0; i < IOSSABERSOUND_MAX_CRACKLES; i++) {
        iosSaberSoundCrackle* c = &iosSaberSound_aCrackles[i];
        if (c->ch.p && ((int32_t)(real - c->cutMs) >= 0 || (int32_t)(c->cutMs - real) > 1000)) iosSaberSound_FadeOut(&c->ch, IOSSABERSOUND_CRACKLE_CUT_SEC);
    }
    for (int i = 0; i < IOSSABERSOUND_SLOTS; i++) {
        iosSaberSoundSaber* s = &iosSaberSound_aSaber[i];
        if (!s->pThing) continue;
        // (the clock gone back or far on, a load or a new level: what was going is forgotten;
        // a frame back is the game's own, IOSSABERSOUND_CLOCK_SLACK_MS)
        if (s->bTimed && (now < s->lastMs ? s->lastMs - now > IOSSABERSOUND_CLOCK_SLACK_MS : now - s->lastMs > 2000)) {
            iosSaberSound_FadeOut(&s->growl, 0.05f);
            iosSaberSound_FadeOut(&s->echo, 0.05f);
            memset(s, 0, sizeof(*s));
            continue;
        }
        s->bTimed = 1;
        s->lastMs = now;
        if (s->signature != s->pThing->signature) {
            iosSaberSound_FadeOut(&s->growl, 0.05f);
            iosSaberSound_FadeOut(&s->echo, 0.05f);
            memset(s, 0, sizeof(*s));
            continue;
        }
        iosSaberSound_TickSaber(s, now, real);
    }
}

// A level closed or opened, a game loaded (sithSoundMixer_Close, _Reset, _ClearAll):
// all it keeps of the sounds playing and their things forgotten, as the mixer
// stops or clears every channel (the clock's jumps, above, only a fallback)
void iosSaberSound_Reset(void)
{
    memset(iosSaberSound_aSaber, 0, sizeof(iosSaberSound_aSaber));
    memset(iosSaberSound_aCrackles, 0, sizeof(iosSaberSound_aCrackles));
    memset(iosSaberSound_aFading, 0, sizeof(iosSaberSound_aFading));
    iosSaberSound_nextFading = 0;
}

int iosSaberSound_IsStarting(void)
{
    return iosSaberSound_bStarting;
}

int iosSaberSound_IsOwn(const sithPlayingSound* pPlaying)
{
    if (!pPlaying) return 0;
    for (int i = 0; i < IOSSABERSOUND_SLOTS; i++) {
        const iosSaberSoundSaber* s = &iosSaberSound_aSaber[i];
        if ((s->growl.p == pPlaying && iosSaberSound_Alive(&s->growl)) || (s->echo.p == pPlaying && iosSaberSound_Alive(&s->echo))) return 1;
    }
    for (int i = 0; i < IOSSABERSOUND_MAX_CRACKLES; i++)
        if (iosSaberSound_aCrackles[i].ch.p == pPlaying && iosSaberSound_Alive(&iosSaberSound_aCrackles[i].ch)) return 1;
    for (int i = 0; i < IOSSABERSOUND_MAX_FADING; i++)
        if (iosSaberSound_aFading[i].p == pPlaying && iosSaberSound_Alive(&iosSaberSound_aFading[i]) && (pPlaying->flags & SITHSOUNDFLAG_FADING)) return 1;
    return 0;
}

#endif // JK_ENHANCED
