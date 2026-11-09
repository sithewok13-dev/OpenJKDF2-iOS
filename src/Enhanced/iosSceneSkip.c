// Added: entire file. See iosSceneSkip.h.

#include "Enhanced/iosSceneSkip.h"

#ifdef JK_ENHANCED

#include "Enhanced/enhTick.h"
#include "Enhanced/iosGame.h"
#include "Enhanced/iosPhotoCam.h"
#include "Devices/sithSoundMixer.h"
#include "Dss/sithGamesave.h"
#include "Engine/sithCamera.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithTime.h"
#include "Main/jkMain.h"
#include "Main/sithMain.h"
#include "World/sithWorld.h"
#include "jk.h"

// Each update while skipping: this much game time, as a frame at 30 fps
// does (the world's physics and scripts step on in their own small steps
// within it, as always)
#define IOSSCENESKIP_STEP_MS 33
// ...as many updates as fit in this much of each frame (s), up to this many
// (a scene of half a minute takes a second or so on a phone)
#define IOSSCENESKIP_FRAME_SECS 0.012
#define IOSSCENESKIP_MAX_STEPS 120
// A scene still going after this much game time skipped (ms) is left to play
// as it is (a level that keeps the controls away for good)
#define IOSSCENESKIP_MAX_MS (5 * 60 * 1000)

static int iosSceneSkip_bSkipping = 0;
static unsigned int iosSceneSkip_serial = 0;
static int iosSceneSkip_bWasIn = 0;
static uint32_t iosSceneSkip_lastGameMs = 0;
static uint32_t iosSceneSkip_startGameMs = 0;
static SithWorld* iosSceneSkip_pWorld = NULL;
static SithThing* iosSceneSkip_pPlayer = NULL;

int iosSceneSkip_InScene(void)
{
    SithThing* p = sithPlayer_g_pLocalPlayerThing;
    SithCamera* c = sithCamera_g_pCurCamera;
    if (sithNet_isMulti || !sithWorld_g_pCurrentWorld || !p || !c) return 0;
    if (p->type != SITH_THING_PLAYER || (p->flags & (SITH_TF_DEAD | SITH_TF_DESTROYED))) return 0;
    // (the script's: the controls away and the camera on something else, its ghost)
    if (!(p->actorParams.flags & SITH_AF_CONTROLSDISABLED)) return 0;
    if (iosPhotoCam_IsActive()) return 0;
    return c->pPrimaryFocusThing && c->pPrimaryFocusThing != p;
}

int iosSceneSkip_IsSkipping(void)
{
    return iosSceneSkip_bSkipping;
}

int iosSceneSkip_CanSkip(void)
{
    return !iosSceneSkip_bSkipping && jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY && !iosGame_HoldGameplay()
           && sithGamesave_state == SITH_GS_NONE && iosSceneSkip_InScene();
}

unsigned int iosSceneSkip_Serial(void)
{
    return iosSceneSkip_serial;
}

// The game's sounds that would only be heard sped up: every sound playing
// once (a line of the scene's talk, a door, a sabre lit) stopped; one that
// loops (a hum, the room's own) goes on, as it would after the scene. The
// music (the CD's, apart) goes on too.
static void iosSceneSkip_Silence(void)
{
    for (int i = 0; i < sithSoundMixer_numSoundsAvailable; i++) {
        sithPlayingSound* s = &sithSoundMixer_aPlayingSounds[i];
        if (s->sound && (s->flags & SITHSOUNDFLAG_PLAYING) && !(s->flags & SITHSOUNDFLAG_LOOP))
            sithSoundMixer_StopSound(s);
    }
}

static void iosSceneSkip_End(void)
{
    iosSceneSkip_bSkipping = 0;
    iosSceneSkip_pWorld = NULL;
    iosSceneSkip_pPlayer = NULL;
}

// (still the same scene, in the same game, playing on)
static int iosSceneSkip_GoesOn(void)
{
    return sithWorld_g_pCurrentWorld == iosSceneSkip_pWorld && sithPlayer_g_pLocalPlayerThing == iosSceneSkip_pPlayer
           && jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY && !jkSmack_stopTick && g_sithMode != 5 && !sithMain_bEndLevel
           && sithGamesave_state == SITH_GS_NONE && !iosGame_HoldGameplay()
           && sithTime_g_msecGameTime - iosSceneSkip_startGameMs < IOSSCENESKIP_MAX_MS && iosSceneSkip_InScene();
}

int iosSceneSkip_Skip(void)
{
    if (!iosSceneSkip_CanSkip()) return 0;
    iosSceneSkip_bSkipping = 1;
    iosSceneSkip_startGameMs = sithTime_g_msecGameTime;
    iosSceneSkip_pWorld = sithWorld_g_pCurrentWorld;
    iosSceneSkip_pPlayer = sithPlayer_g_pLocalPlayerThing;
    iosSceneSkip_Silence();
    return 1;
}

void iosSceneSkip_Run(void)
{
    if (!iosSceneSkip_bSkipping) return;
    if (!iosSceneSkip_GoesOn()) {
        // (over: the scene ended in the game's own update, or the game went elsewhere)
        iosSceneSkip_End();
        return;
    }
    iosSceneSkip_Silence();
    const double until = enh_Now() + IOSSCENESKIP_FRAME_SECS;
    for (int n = 0; n < IOSSCENESKIP_MAX_STEPS && enh_Now() < until; n++) {
        // (the game's clock told one step has passed: the update moves the world on by that)
        sithTime_SetNextStep(IOSSCENESKIP_STEP_MS);
        if (sithUpdate()) break;
        iosSceneSkip_Silence();
        if (!iosSceneSkip_GoesOn()) {
            iosSceneSkip_End();
            return;
        }
    }
}

int iosSceneSkip_HidesWorld(void)
{
    return iosSceneSkip_bSkipping;
}

void iosSceneSkip_Frame(void)
{
    // a scene begun (or the game loaded in another one, its clock gone back):
    // one more for the overlay's hint
    const int in = iosSceneSkip_InScene();
    const uint32_t now = sithTime_g_msecGameTime;
    if (in && (!iosSceneSkip_bWasIn || now < iosSceneSkip_lastGameMs)) iosSceneSkip_serial++;
    iosSceneSkip_bWasIn = in;
    iosSceneSkip_lastGameMs = now;
    if (iosSceneSkip_bSkipping && !iosSceneSkip_GoesOn()) iosSceneSkip_End();
}

#endif // JK_ENHANCED
