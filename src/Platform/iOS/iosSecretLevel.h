// Added: entire file. Mysteries of the Sith's secret level (Bespin, S6L1.jkl)
// set up as on PC, from the player's own files. See iosSecretLevel.c.
#ifndef _IOS_SECRET_LEVEL_H
#define _IOS_SECRET_LEVEL_H

#ifdef __cplusplus
extern "C" {
#endif

// In the game's data folder (the current directory): if the player's
// Resource/Video has the game's UNSUPPORTED.ZIP and Episode has no
// "MotS Secret Level" yet, its Cog and Jkl folders are taken out of the zip
// into Episode/MotS Secret Level/, with the episode.jk the PC game's
// instructions give. Once: nothing is done when it is there already. Only in
// the OpenMoTS app (iosApp_IsMots) playing Mysteries of the Sith.
void iosSecretLevel_Setup(void);

#ifdef __cplusplus
}
#endif

#endif // _IOS_SECRET_LEVEL_H
