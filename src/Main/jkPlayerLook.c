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
#include "Enhanced/iosSaberFold.h"
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

// ([version][style][key 0][key 1], every build's; then [flags], builds with the hilts'; then each
// hand's second blade's key, builds with each hand's own: jkPlayerLook.h)
#define JKPLAYERLOOK_MSG_LEN (2 + JKPLAYERLOOK_MAX_BLADES * JKPLAYERLOOK_KEY_LEN)
#define JKPLAYERLOOK_FLAGS_AT JKPLAYERLOOK_MSG_LEN
#define JKPLAYERLOOK_SECOND_AT (JKPLAYERLOOK_FLAGS_AT + 1)
#define JKPLAYERLOOK_FULL_LEN (JKPLAYERLOOK_SECOND_AT + JKPLAYERLOOK_MAX_BLADES * JKPLAYERLOOK_KEY_LEN)
#define JKPLAYERLOOK_OLD_STYLE_CROSS 2 // (builds before the hilts: one saber with a crossguard)
#define JKPLAYERLOOK_HILT_CROSS 1
#define JKPLAYERLOOK_HILT_BITS 2

static void jkPlayerLook_Key(char* pOut, const uint8_t* pIn)
{
    memcpy(pOut, pIn, JKPLAYERLOOK_KEY_LEN);
    pOut[JKPLAYERLOOK_KEY_LEN - 1] = 0;
}

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
    jkPlayerLook* l = &pSlot->look;
    memset(l, 0, sizeof(*l));
    // (CROSS from a build before the hilts: one saber, with that hilt; the flags, where there are
    // any, say which hilts)
    l->style = pData[1] == JKPLAYERLOOK_OLD_STYLE_CROSS ? 0 : pData[1];
    l->aHilt[0] = pData[1] == JKPLAYERLOOK_OLD_STYLE_CROSS ? JKPLAYERLOOK_HILT_CROSS : 0;
    for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES; h++) jkPlayerLook_Key(l->aColourKey[h][0], pData + 2 + h * JKPLAYERLOOK_KEY_LEN);
    if (len >= JKPLAYERLOOK_FULL_LEN) {
        const int flags = pData[JKPLAYERLOOK_FLAGS_AT];
        for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES; h++) {
            l->aHilt[h] = (flags >> (h * JKPLAYERLOOK_HILT_BITS)) & 3;
            jkPlayerLook_Key(l->aColourKey[h][1], pData + JKPLAYERLOOK_SECOND_AT + h * JKPLAYERLOOK_KEY_LEN);
        }
        l->flags = flags & (JKPLAYERLOOK_FLAG_FOLD_OPEN(0) | JKPLAYERLOOK_FLAG_FOLD_OPEN(1) | JKPLAYERLOOK_FLAG_SPLIT(0) | JKPLAYERLOOK_FLAG_SPLIT(1));
    }
    else if (len > JKPLAYERLOOK_FLAGS_AT) {
        // (a build of the hilts before each hand had its own: one hilt, both hands')
        l->aHilt[0] = l->aHilt[1] = pData[JKPLAYERLOOK_FLAGS_AT] & 3;
    }
    for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES; h++)
        if (!(l->flags & JKPLAYERLOOK_FLAG_SPLIT(h))) memcpy(l->aColourKey[h][1], l->aColourKey[h][0], JKPLAYERLOOK_KEY_LEN);
}

// The style a build before the hilts is sent ([1]) for a style and the right
// hand's hilt, so it draws the nearest it has: one saber with a crossguard
// its CROSS; anything else the style as it is (one plain saber, or two)
static int jkPlayerLook_OldStyle(int style, int rightHilt)
{
    if (style != 0 && style != 1) return 0;
    return (style == 0 && rightHilt == JKPLAYERLOOK_HILT_CROSS) ? JKPLAYERLOOK_OLD_STYLE_CROSS : style;
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
    for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES && h < iosSaber_StyleNumBlades(pOut->style); h++) {
        pOut->aHilt[h] = iosSaber_GetHandHilt(h) & 3;
        for (int b = 0; b < JKPLAYERLOOK_HAND_BLADES; b++) {
            // (as it is shown here: the saber wheel's look can make a kept colour the game's own)
            const int id = iosSaber_GetShownBladeColour(h, b);
            const char* key = iosSaber_ColourKey(id);
            if (key && id != IOSSABER_COLOUR_GAME)
                strncpy(pOut->aColourKey[h][b], key, JKPLAYERLOOK_KEY_LEN - 1);
        }
        if (!iosSaber_GetBladeLinked(h)) pOut->flags |= JKPLAYERLOOK_FLAG_SPLIT(h);
    }
    // (FOLD open, each hand's saber: iosSaberFold's bit 0 the right, bit 1 the left)
    const int open = sithPlayer_g_pLocalPlayerThing ? iosSaberFold_OpenBits(sithPlayer_g_pLocalPlayerThing) : 0;
    for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES; h++)
        if (open & (1 << h)) pOut->flags |= JKPLAYERLOOK_FLAG_FOLD_OPEN(h);
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
    if (jkPlayerLook_bSent && now - jkPlayerLook_sentMs < JKPLAYERLOOK_MIN_MS)
        return;
    uint8_t aMsg[JKPLAYERLOOK_FULL_LEN];
    memset(aMsg, 0, sizeof(aMsg));
    aMsg[0] = JKPLAYERLOOK_VERSION;
    aMsg[1] = (uint8_t)jkPlayerLook_OldStyle(look.style, look.aHilt[0]);
    int flags = look.flags;
    for (int h = 0; h < JKPLAYERLOOK_MAX_BLADES; h++) {
        memcpy(aMsg + 2 + h * JKPLAYERLOOK_KEY_LEN, look.aColourKey[h][0], JKPLAYERLOOK_KEY_LEN);
        memcpy(aMsg + JKPLAYERLOOK_SECOND_AT + h * JKPLAYERLOOK_KEY_LEN, look.aColourKey[h][1], JKPLAYERLOOK_KEY_LEN);
        flags |= (look.aHilt[h] & 3) << (h * JKPLAYERLOOK_HILT_BITS);
    }
    aMsg[JKPLAYERLOOK_FLAGS_AT] = (uint8_t)flags;
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
