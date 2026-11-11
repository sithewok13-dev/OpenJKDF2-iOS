#ifndef _JKGUIESC_H
#define _JKGUIESC_H

#include "types.h"

#define jkGuiEsc_Startup_ADDR (0x00411C00)
#define jkGuiEsc_Shutdown_ADDR (0x00411C20)
#define jkGuiEsc_Show_ADDR (0x00411C30)

//#define jkGuiEsc_menu (*(jkGuiMenu*)0x0529948)
//#define jkGuiEsc_aElements ((jkGuiElement*)0x0529560)

void jkGuiEsc_Startup();
void jkGuiEsc_Shutdown();
void jkGuiEsc_Show();
#ifdef JK_ENHANCED
// Added: the next jkGuiEsc_Show (the game menu Escape opens, within a moment)
// goes straight to the Save (bSave) or Load screen, and back to the game
// after it, as if Return to Game were picked (iosGame_OpenSaveLoad); -1:
// none (jkMain_GuiAdvance, the game going anywhere but the game menu)
void jkGuiEsc_ShowDirect(int bSave);
#endif

#endif // _JKGUIESC_H
