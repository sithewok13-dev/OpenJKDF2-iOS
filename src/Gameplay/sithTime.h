#ifndef _SITHTIME_H
#define _SITHTIME_H

#include <stdint.h>
#include "types.h"

#define sithTime_Advance_ADDR (0x004DD640)
#define sithTime_Pause_ADDR (0x004DD710)
#define sithTime_Resume_ADDR (0x004DD730)
#define sithTime_SetFrameTime_ADDR (0x004DD760)
#define sithTime_Startup_ADDR (0x004DD800)
#define sithTime_SetGameTime_ADDR (0x004DD830)

MATH_FUNC void sithTime_Advance();
void sithTime_Pause();
void sithTime_Resume();
MATH_FUNC void sithTime_SetFrameTime(int frameTime);
void sithTime_Startup();
MATH_FUNC void sithTime_SetGameTime(uint32_t msecTime);
#ifdef JK_ENHANCED
// Added: the next sithTime_Advance finds this much time passed since the last
// one, whatever really has (a level's scene skipped, iosSceneSkip: update
// after update run within one frame, each a step of game time)
void sithTime_SetNextStep(int msec);
#endif

extern flex_d_t sithTime_physicsRolloverFrames;

#endif // _SITHTIME_H
