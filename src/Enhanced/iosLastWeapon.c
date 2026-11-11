// Added: entire file. See iosLastWeapon.h.

#include "Enhanced/iosLastWeapon.h"

#ifdef JK_ENHANCED

static int iosLastWeapon_held = -1, iosLastWeapon_last = -1;

void iosLastWeapon_Reset(void)
{
    iosLastWeapon_held = iosLastWeapon_last = -1;
}

void iosLastWeapon_Note(int held)
{
    if (held < 0) return;
    if (iosLastWeapon_held >= 0 && held != iosLastWeapon_held) iosLastWeapon_last = iosLastWeapon_held;
    iosLastWeapon_held = held;
}

int iosLastWeapon_Get(void)
{
    return (iosLastWeapon_last >= 0 && iosLastWeapon_last != iosLastWeapon_held) ? iosLastWeapon_last : -1;
}

#endif // JK_ENHANCED
