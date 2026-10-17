// Added: entire file. How a crossguard saber (iosSaber's CROSS hilt) sounds:
// a rougher, lower, unstable take on the game's own saber hum that crackles,
// matching its crackling blade, and a sputter as it ignites. Made at run time
// from the game's own saber sounds (no sound files of its own): the saber
// cog's hum played lower, its pitch never quite steady, with a second, much
// deeper copy of it under it (the two beat against each other: the rough
// edge), and now and then a short crackle (a snippet of one of the game's
// saber hit sounds, high and quiet, cut off after a few hundredths of a
// second), a burst of them as it ignites, and now and then a quick sputter;
// its ignition and switching off a little lower, and its swings too.
//
// For the local player, and in multiplayer for other players whose games
// sent the CROSS hilt (their hum comes as the game sends it; the crackle is
// added here). Other sabers sound exactly as the game has them. The extra
// sounds are this game's only: never sent to other players or kept in a
// save, and heard by no one in the game (the AI hears the saber as the game
// plays it). They follow the game's hum: it fades out, they fade out.

#ifndef _ENHANCED_IOSSABERSOUND_H
#define _ENHANCED_IOSSABERSOUND_H

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

struct sithPlayingSound;
struct sithSound;
struct SithThing;

// sithSoundMixer_PlaySoundThing: a sound just played following a thing
// (pPlaying its channel, NULL if none)
void iosSaberSound_Played(struct sithPlayingSound* pPlaying, struct sithSound* pSound, struct SithThing* pThing, int flags);

// sithSoundMixer_Update: once a frame
void iosSaberSound_Tick(void);

// 1 while it starts one of its own (sithSoundMixer_PlaySoundThing: no one in
// the game hears it)
int iosSaberSound_IsStarting(void);

// 1 for a channel of its own (sithSoundMixer_SyncSounds: not saved or sent)
int iosSaberSound_IsOwn(const struct sithPlayingSound* pPlaying);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSSABERSOUND_H
