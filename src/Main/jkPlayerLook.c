// Added: entire file. See jkPlayerLook.h.

#include "Main/jkPlayerLook.h"

#ifdef QOL_IMPROVEMENTS

#include <string.h>
#include "jk.h"
#include "stdPlatform.h"
#include "Dss/jkNetExt.h"
#include "Dss/sithMulti.h"
#include "Gameplay/sithPlayer.h"
#include "Win95/stdComm.h"
#ifdef JK_ENHANCED
#include "Enhanced/iosSaber.h"
#endif

#define JKPLAYERLOOK_VERSION 1
#define JKPLAYERLOOK_NUM 32

typedef struct jkPlayerLookSlot
{
    int netId;          // (0: free)
    uint32_t heardMs;
    jkPlayerLook look;
} jkPlayerLookSlot;

static jkPlayerLookSlot jkPlayerLook_aSlot[JKPLAYERLOOK_NUM];
static uint32_t jkPlayerLook_sentMs = 0;
static jkPlayerLook jkPlayerLook_sent;
static int jkPlayerLook_bSent = 0;

// ([version][style][key 0][key 1], every build's; then [flags], builds with the hilts': jkPlayerLook.h)
#define JKPLAYERLOOK_MSG_LEN (2 + JKPLAYERLOOK_MAX_BLADES * JKPLAYERLOOK_KEY_LEN)
#define JKPLAYERLOOK_FLAGS_AT JKPLAYERLOOK_MSG_LEN
#define JKPLAYERLOOK_OLD_STYLE_CROSS 2 // (builds before the hilts: one saber with a crossguard)

static void jkPlayerLook_Arrived(int idFrom, const uint8_t* pData, int len)
{
    if (len < JKPLAYERLOOK_MSG_LEN || pData[0] != JKPLAYERLOOK_VERSION || !idFrom || idFrom == (int)stdComm_dplayIdSelf)
        return;
    jkPlayerLookSlot* pSlot = NULL;
    for (int i = 0; i < JKPLAYERLOOK_NUM && !pSlot; i++)
        if (jkPlayerLook_aSlot[i].netId == idFrom) pSlot = &jkPlayerLook_aSlot[i];
    const uint32_t now = stdPlatform_GetTimeMsec();
    for (int i = 0; i < JKPLAYERLOOK_NUM && !pSlot; i++)
        if (!jkPlayerLook_aSlot[i].netId || now - jkPlayerLook_aSlot[i].heardMs > JKPLAYERLOOK_KEEP_MS) pSlot = &jkPlayerLook_aSlot[i];
    if (!pSlot)
        return;
    pSlot->netId = idFrom;
    pSlot->heardMs = now;
    // (CROSS from a build before the hilts, or for them: one saber, with that hilt; the flags, where
    // there are any, say which hilt)
    pSlot->look.style = pData[1] == JKPLAYERLOOK_OLD_STYLE_CROSS ? 0 : pData[1];
    pSlot->look.hilt = pData[1] == JKPLAYERLOOK_OLD_STYLE_CROSS ? 1 : 0;
    pSlot->look.flags = 0;
    if (len > JKPLAYERLOOK_FLAGS_AT) {
        const int flags = pData[JKPLAYERLOOK_FLAGS_AT] & JKPLAYERLOOK_FLAGS_KNOWN;
        pSlot->look.hilt = flags & JKPLAYERLOOK_FLAGS_HILT_MASK;
        pSlot->look.flags = flags & ~JKPLAYERLOOK_FLAGS_HILT_MASK;
    }
    for (int b = 0; b < JKPLAYERLOOK_MAX_BLADES; b++) {
        memcpy(pSlot->look.aColourKey[b], pData + 2 + b * JKPLAYERLOOK_KEY_LEN, JKPLAYERLOOK_KEY_LEN);
        pSlot->look.aColourKey[b][JKPLAYERLOOK_KEY_LEN - 1] = 0;
    }
}

// The style a build before the hilts is sent ([1]) for a style and hilt, so it
// draws the nearest it has: one saber with a crossguard its CROSS; two
// crossguards two plain sabers; STAFF and FOLD, for now, the style as it is
// (one saber, or two)
static int jkPlayerLook_OldStyle(int style, int hilt)
{
    if (style != 0 && style != 1) return 0;
    return (style == 0 && hilt == 1) ? JKPLAYERLOOK_OLD_STYLE_CROSS : style;
}

void jkPlayerLook_Startup(void)
{
    memset(jkPlayerLook_aSlot, 0, sizeof(jkPlayerLook_aSlot));
    jkPlayerLook_bSent = 0;
    jkNetExt_Register(JKNETEXT_LOOK, jkPlayerLook_Arrived);
}

int jkPlayerLook_GetLocal(jkPlayerLook* pOut)
{
    memset(pOut, 0, sizeof(*pOut));
#ifdef JK_ENHANCED
    pOut->style = iosSaber_GetStyle();
    pOut->hilt = iosSaber_GetHilt();
    for (int b = 0; b < JKPLAYERLOOK_MAX_BLADES && b < iosSaber_StyleNumBlades(pOut->style); b++) {
        // (as it is shown here: the saber wheel's look can make a kept colour the game's own)
        const int id = iosSaber_GetShownColour(pOut->style, b);
        const char* key = iosSaber_ColourKey(id);
        if (key && id != IOSSABER_COLOUR_GAME)
            strncpy(pOut->aColourKey[b], key, JKPLAYERLOOK_KEY_LEN - 1);
    }
    return 1;
#else
    return 0;
#endif
}

void jkPlayerLook_Tick(void)
{
    if (!sithNet_isMulti || !sithPlayer_g_pLocalPlayerThing)
        return;
    jkPlayerLook look;
    if (!jkPlayerLook_GetLocal(&look))
        return;
    const uint32_t now = stdPlatform_GetTimeMsec();
    if (jkPlayerLook_bSent && now - jkPlayerLook_sentMs < JKPLAYERLOOK_SEND_MS && !memcmp(&look, &jkPlayerLook_sent, sizeof(look)))
        return;
    uint8_t aMsg[JKPLAYERLOOK_MSG_LEN + 1];
    memset(aMsg, 0, sizeof(aMsg));
    aMsg[0] = JKPLAYERLOOK_VERSION;
    aMsg[1] = (uint8_t)jkPlayerLook_OldStyle(look.style, look.hilt);
    for (int b = 0; b < JKPLAYERLOOK_MAX_BLADES; b++)
        memcpy(aMsg + 2 + b * JKPLAYERLOOK_KEY_LEN, look.aColourKey[b], JKPLAYERLOOK_KEY_LEN);
    aMsg[JKPLAYERLOOK_FLAGS_AT] = (uint8_t)(((look.hilt & JKPLAYERLOOK_FLAGS_HILT_MASK) | (look.flags & ~JKPLAYERLOOK_FLAGS_HILT_MASK))
                                            & JKPLAYERLOOK_FLAGS_KNOWN);
    jkNetExt_Send(JKNETEXT_LOOK, aMsg, sizeof(aMsg), -1);
    jkPlayerLook_sent = look;
    jkPlayerLook_sentMs = now;
    jkPlayerLook_bSent = 1;
}

const jkPlayerLook* jkPlayerLook_ForThing(SithThing* pThing)
{
    if (!sithNet_isMulti || !pThing || pThing == sithPlayer_g_pLocalPlayerThing || pThing->type != SITH_THING_PLAYER
        || !pThing->actorParams.pPlayer)
        return NULL;
    const int netId = (int)pThing->actorParams.pPlayer->playerNetId;
    if (!netId)
        return NULL;
    const uint32_t now = stdPlatform_GetTimeMsec();
    for (int i = 0; i < JKPLAYERLOOK_NUM; i++) {
        const jkPlayerLookSlot* pSlot = &jkPlayerLook_aSlot[i];
        if (pSlot->netId == netId && now - pSlot->heardMs <= JKPLAYERLOOK_KEEP_MS)
            return &pSlot->look;
    }
    return NULL;
}

#endif // QOL_IMPROVEMENTS
