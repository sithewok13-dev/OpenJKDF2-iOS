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
#include "Main/jkDev.h"
#include "Main/jkMain.h"
#include "Main/sithMain.h"
#include "World/sithWorld.h"
#include "stdPlatform.h"
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
// as it is, and isn't skipped again (a level that keeps the controls away for
// good; the longest of the games' own is some 30 s)
#define IOSSCENESKIP_MAX_MS (2 * 60 * 1000)

static int iosSceneSkip_bSkipping = 0;
static unsigned int iosSceneSkip_serial = 0;
static int iosSceneSkip_bWasIn = 0;
static uint32_t iosSceneSkip_lastGameMs = 0;
static uint32_t iosSceneSkip_startGameMs = 0;
static SithWorld* iosSceneSkip_pWorld = NULL;
static SithThing* iosSceneSkip_pPlayer = NULL;
static int iosSceneSkip_bAuto = 0;   // AUTOPLAY's show: each scene skipped by itself (iosSceneSkip_SetAuto)...
static int iosSceneSkip_bGaveUp = 0; // a scene given up on (still going after IOSSCENESKIP_MAX_MS): not skipped again
static uint32_t iosSceneSkip_startRealMs = 0; // (when the skip began, by the HUD's lines' clock)

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
    return !iosSceneSkip_bSkipping && !iosSceneSkip_bGaveUp && jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY && !iosGame_WantsHold()
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
    // (the lines the scene printed meanwhile -- its talk's subtitles -- gone with it)
    jkDev_Custom_DropLinesSince(iosSceneSkip_startRealMs);
    iosSceneSkip_bSkipping = 0;
    iosSceneSkip_pWorld = NULL;
    iosSceneSkip_pPlayer = NULL;
}

// (still the same scene, in the same game, playing on; given up on after
// IOSSCENESKIP_MAX_MS of it)
static int iosSceneSkip_GoesOn(void)
{
    if (sithTime_g_msecGameTime - iosSceneSkip_startGameMs >= IOSSCENESKIP_MAX_MS) {
        iosSceneSkip_bGaveUp = 1;
        return 0;
    }
    return sithWorld_g_pCurrentWorld == iosSceneSkip_pWorld && sithPlayer_g_pLocalPlayerThing == iosSceneSkip_pPlayer
           && jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY && !jkSmack_stopTick && g_sithMode != 5 && !sithMain_bEndLevel
           && sithGamesave_state == SITH_GS_NONE && !iosGame_WantsHold() && iosSceneSkip_InScene();
}

void iosSceneSkip_SetAuto(int bOn)
{
    iosSceneSkip_bAuto = bOn ? 1 : 0;
}

int iosSceneSkip_Skip(void)
{
    if (!iosSceneSkip_CanSkip()) return 0;
    iosSceneSkip_bSkipping = 1;
    iosSceneSkip_startGameMs = sithTime_g_msecGameTime;
    iosSceneSkip_startRealMs = stdPlatform_GetTimeMsec();
    iosSceneSkip_pWorld = sithWorld_g_pCurrentWorld;
    iosSceneSkip_pPlayer = sithPlayer_g_pLocalPlayerThing;
    iosSceneSkip_Silence();
    return 1;
}

void iosSceneSkip_Run(void)
{
    if (!iosSceneSkip_bSkipping) {
        // AUTOPLAY's show: a scene skipped from the update it begins in, before any of it is drawn
        if (!iosSceneSkip_bAuto || !iosSceneSkip_Skip()) return;
    }
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
        // (over: the sounds of the update it ended in -- play's first -- left to play)
        if (!iosSceneSkip_GoesOn()) {
            iosSceneSkip_End();
            return;
        }
        iosSceneSkip_Silence();
    }
}

int iosSceneSkip_HidesWorld(void)
{
    // (AUTOPLAY's show: a scene on and not skipped yet -- the level's first frame, drawn as it
    // opens, or one with a save still being written -- left black as well)
    return iosSceneSkip_bSkipping || (iosSceneSkip_bAuto && !iosSceneSkip_bGaveUp && iosSceneSkip_InScene());
}

void iosSceneSkip_Frame(void)
{
    // a scene begun (or the game loaded in another one, its clock gone back):
    // one more for the overlay's hint
    const int in = iosSceneSkip_InScene();
    const uint32_t now = sithTime_g_msecGameTime;
    if (in && (!iosSceneSkip_bWasIn || now < iosSceneSkip_lastGameMs)) iosSceneSkip_serial++;
    if (!in) iosSceneSkip_bGaveUp = 0; // (a scene given up on is over: the next can be skipped)
    iosSceneSkip_bWasIn = in;
    iosSceneSkip_lastGameMs = now;
    if (iosSceneSkip_bSkipping && !iosSceneSkip_GoesOn()) iosSceneSkip_End();
}

#endif // JK_ENHANCED
