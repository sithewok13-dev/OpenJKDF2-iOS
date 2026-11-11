#ifndef _ENHANCED_IOSLASTWEAPON_H
#define _ENHANCED_IOSLASTWEAPON_H

// Added: entire file. LAST WPN's memory: the weapon held before the one in
// hand now, however the switch came about (the weapon wheel, NEXT WPN, the
// keys, a game controller, the game itself on a pickup or with the ammo
// gone): noted once a frame from the weapon in hand (iosGame_WeaponTick).
// Plain C, no game state of its own, so the touch overlay's rig can run it.

#ifdef JK_ENHANCED

#ifdef __cplusplus
extern "C" {
#endif

// A new level or player: nothing held before
void iosLastWeapon_Reset(void);
// The weapon in hand now (its bin; below 0: none, e.g. mid-switch: ignored)
void iosLastWeapon_Note(int held);
// The one held before it (a bin), or -1 (nothing different yet)
int iosLastWeapon_Get(void);

#ifdef __cplusplus
}
#endif

#endif // JK_ENHANCED

#endif // _ENHANCED_IOSLASTWEAPON_H
