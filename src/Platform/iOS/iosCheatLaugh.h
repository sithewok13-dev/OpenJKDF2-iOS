// Added: entire file. The touch overlay's cheat mode easter egg: the Emperor,
// once each time cheat mode is switched on (iosTouch: the tray's CHEAT switch
// off to on, or "sithewok" typed with CHEAT hidden) -- nowhere else. One of
// the recordings, picked at random, never the one played last time.
//
// None come with the app: they're the player's own, any audio files (mp3, m4a,
// aac, wav, caf, aif) in an "Emperor" folder in the app's own folder in the
// Files app (On My iPhone > OpenJKDF2 > Emperor, or OpenMoTS > Emperor; the
// folder's name in any case). With none there, in an Emperor folder in the
// game's folder (next to Episode and Resource, where copying a set of game
// files that carries one puts it), else in jk1/Emperor or mots/Emperor. With
// none in any, cheat mode is silent. A build of your own can still carry
// them: configured with -DOPENJKDF2_IOS_BUNDLE_EMPEROR=ON,
// cmake_modules/target_ios_all.cmake copies IOSCHEATLAUGH_NAMES (.mp3, from
// packaging/ios) into the app, used when no folder has any.
//
// Played whole and as it is, once, at the game's sound effects volume (none
// at 0), alongside the game's own sounds; while one is still playing, another
// switch-on doesn't start a second.

#ifndef _PLATFORM_IOS_IOSCHEATLAUGH_H
#define _PLATFORM_IOS_IOSCHEATLAUGH_H

#ifdef TARGET_IOS

#ifdef __cplusplus
extern "C" {
#endif

#define IOSCHEATLAUGH_FOLDER "Emperor"
#define IOSCHEATLAUGH_GAME_FOLDERS { "jk1", "mots" }   // (the game folders an Emperor folder can be in too)
#define IOSCHEATLAUGH_EXTS { "mp3", "m4a", "aac", "wav", "caf", "aif", "aiff" }
#define IOSCHEATLAUGH_MAX 16                          // (at most this many of the folder's files are picked from)
#define IOSCHEATLAUGH_MAX_BYTES (16 * 1024 * 1024)    // (a bigger file isn't read)
#define IOSCHEATLAUGH_NAMES { "laughing", "palpatine_dark_side", "palpatine_unlimited_power" } // (a build's own)

// One of them, if one can be read and none is already playing. Main thread.
// 1: playing one now; 0: not (none can be read, sound effects are off, or one is already playing).
int iosCheatLaugh_Play(float volume);

#ifdef __cplusplus
}
#endif

#endif // TARGET_IOS

#endif // _PLATFORM_IOS_IOSCHEATLAUGH_H
