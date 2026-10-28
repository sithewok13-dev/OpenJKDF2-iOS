// Added: entire file. See iosSecretLevel.h.
//
// Mysteries of the Sith has a secret level, Bespin (S6L1.jkl), that the game
// leaves out of its episode. On PC it is set up by hand: make the folder
// Episode/MotS Secret Level, copy the Cog and Jkl folders out of the game's
// Resource/Video/UNSUPPORTED.ZIP into it, and write an episode.jk there. The
// game then lists "MotS Secret Level" under Single Player > New Game. On a
// phone that is hard to do, so OpenMoTS does it by itself at launch, from the
// player's own files: nothing from the game is in the app.
//
// The zip is read here with a small reader of its own (stored and deflated
// entries, deflate through zlib, which the app links already): only the
// entries under a "cog" or "jkl" folder are taken out.

#ifdef TARGET_IOS

#include "Platform/iOS/iosSecretLevel.h"
#include "Platform/iOS/iosApp.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <zlib.h>

#include "Main/Main.h"
#include "stdPlatform.h"

#define IOSSECRET_FOLDER "MotS Secret Level"
#define IOSSECRET_LEVEL "S6L1.jkl"
#define IOSSECRET_ZIP "unsupported.zip"
#define IOSSECRET_MAX_ENTRIES 4096
#define IOSSECRET_MAX_FILE (32u * 1024u * 1024u) // (the level's files are far smaller)

// The episode.jk the PC game's instructions give
static const char iosSecretLevel_aEpisodeJk[] =
    "\"MotS Secret Level\"\n"
    "TYPE 1\n"
    "SEQ 1\n"
    "10:\t1\t1\tLEVEL\tS6L1.jkl\t0\t0\t-1\t-1\n"
    "end\n";

// pDir's entry named pName, any case, into pOut (pDir/<its real name>);
// bWantDir: only a folder, else only a file. 0: none.
static int iosSecretLevel_Find(const char* pDir, const char* pName, int bWantDir, char* pOut, size_t outLen)
{
    DIR* d = opendir(pDir);
    if (!d) return 0;
    int bFound = 0;
    struct dirent* e;
    while (!bFound && (e = readdir(d)) != NULL) {
        if (strcasecmp(e->d_name, pName)) continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", pDir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (bWantDir ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode)) continue;
        snprintf(pOut, outLen, "%s", path);
        bFound = 1;
    }
    closedir(d);
    return bFound;
}

// The game's UNSUPPORTED.ZIP: in Resource/Video, where the game has it (or,
// in case it was copied elsewhere, in Resource or next to it). Every folder
// named "resource" (any case) is looked in.
static int iosSecretLevel_FindZip(char* pOut, size_t outLen)
{
    DIR* d = opendir(".");
    if (!d) return 0;
    int bFound = 0;
    struct dirent* e;
    while (!bFound && (e = readdir(d)) != NULL) {
        if (strcasecmp(e->d_name, "resource")) continue;
        char res[1024], video[1024];
        snprintf(res, sizeof(res), "./%s", e->d_name);
        if (iosSecretLevel_Find(res, "video", 1, video, sizeof(video))
            && iosSecretLevel_Find(video, IOSSECRET_ZIP, 0, pOut, outLen)) bFound = 1;
        else if (iosSecretLevel_Find(res, IOSSECRET_ZIP, 0, pOut, outLen)) bFound = 1;
    }
    closedir(d);
    return bFound || iosSecretLevel_Find(".", IOSSECRET_ZIP, 0, pOut, outLen);
}

static uint32_t iosSecretLevel_U16(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t iosSecretLevel_U32(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// pPath's folders made, each in turn (the last part is the file's name)
static int iosSecretLevel_MakeDirsFor(const char* pPath)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", pPath);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return 1;
}

// The zip entry's name (zip folders split by '/' or '\') from its first
// "cog" or "jkl" folder on, as a path under the secret level's folder: 0 if
// it isn't under one, is a folder itself, or has a part that could leave
// the folder ("..")
static int iosSecretLevel_EntryPath(const char* pName, char* pOut, size_t outLen)
{
    char name[512];
    snprintf(name, sizeof(name), "%s", pName);
    for (char* p = name; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    const size_t len = strlen(name);
    if (len == 0 || name[len - 1] == '/') return 0;
    const char* pStart = NULL;
    const char* part = name;
    while (part && *part) {
        const char* slash = strchr(part, '/');
        const size_t partLen = slash ? (size_t)(slash - part) : strlen(part);
        if ((partLen == 1 && part[0] == '.') || (partLen == 2 && part[0] == '.' && part[1] == '.')) return 0;
        if (!pStart && slash && partLen == 3 && (!strncasecmp(part, "cog", 3) || !strncasecmp(part, "jkl", 3)))
            pStart = part;
        part = slash ? slash + 1 : NULL;
    }
    if (!pStart || strstr(pStart, "//")) return 0;
    snprintf(pOut, outLen, "%s", pStart);
    for (int i = 0; i < 3 && pOut[i]; i++) { // ("cog" and "jkl" in small letters: one folder each)
        if (pOut[i] >= 'A' && pOut[i] <= 'Z') pOut[i] = (char)(pOut[i] - 'A' + 'a');
    }
    return 1;
}

// One entry's data (at its local header, lhOffset) written to pDest; 1: done
static int iosSecretLevel_ExtractEntry(FILE* f, uint32_t lhOffset, uint32_t method, uint32_t compSize,
                                       uint32_t size, const char* pDest)
{
    unsigned char lh[30];
    if (fseek(f, (long)lhOffset, SEEK_SET) != 0 || fread(lh, 1, 30, f) != 30 || iosSecretLevel_U32(lh) != 0x04034b50)
        return 0;
    if (fseek(f, (long)(lhOffset + 30 + iosSecretLevel_U16(lh + 26) + iosSecretLevel_U16(lh + 28)), SEEK_SET) != 0)
        return 0;
    unsigned char* pComp = (unsigned char*)malloc(compSize ? compSize : 1);
    unsigned char* pData = (unsigned char*)malloc(size ? size : 1);
    int bOk = pComp && pData && fread(pComp, 1, compSize, f) == compSize;
    if (bOk && method == 0) {
        bOk = compSize == size;
        if (bOk) memcpy(pData, pComp, size);
    }
    else if (bOk) { // (8: deflated, raw)
        z_stream z;
        memset(&z, 0, sizeof(z));
        bOk = inflateInit2(&z, -MAX_WBITS) == Z_OK;
        if (bOk) {
            z.next_in = pComp;
            z.avail_in = compSize;
            z.next_out = pData;
            z.avail_out = size;
            const int ret = inflate(&z, Z_FINISH);
            bOk = (ret == Z_STREAM_END || (ret == Z_BUF_ERROR && z.avail_out == 0)) && z.total_out == size;
            inflateEnd(&z);
        }
    }
    if (bOk) {
        // (written next to it first, then put in place: never half a file)
        char tmp[1100];
        snprintf(tmp, sizeof(tmp), "%s.part", pDest);
        FILE* o = fopen(tmp, "wb");
        bOk = o && fwrite(pData, 1, size, o) == size;
        if (o && fclose(o) != 0) bOk = 0;
        if (bOk) bOk = rename(tmp, pDest) == 0;
        if (!bOk) remove(tmp);
    }
    free(pComp);
    free(pData);
    return bOk;
}

// The zip's cog and jkl folders taken out into pFolder; 1 if every one of
// their files was (and the level's own .jkl was among them)
static int iosSecretLevel_Extract(const char* pZip, const char* pFolder)
{
    FILE* f = fopen(pZip, "rb");
    if (!f) return 0;
    int bOk = 0, numFiles = 0, bLevel = 0;
    unsigned char* pCd = NULL;
    unsigned char* pTail = NULL;
    do {
        // The end of the central directory: in the last 22 bytes, or before a comment (64 KB at most)
        if (fseek(f, 0, SEEK_END) != 0) break;
        const long fileSize = ftell(f);
        if (fileSize < 22) break;
        const long tailLen = fileSize < 22 + 65535 ? fileSize : 22 + 65535;
        pTail = (unsigned char*)malloc((size_t)tailLen);
        if (!pTail || fseek(f, fileSize - tailLen, SEEK_SET) != 0 || fread(pTail, 1, (size_t)tailLen, f) != (size_t)tailLen) break;
        long eocd = -1;
        for (long i = tailLen - 22; i >= 0; i--) {
            if (iosSecretLevel_U32(pTail + i) == 0x06054b50) {
                eocd = i;
                break;
            }
        }
        if (eocd < 0) break;
        const uint32_t numEntries = iosSecretLevel_U16(pTail + eocd + 10);
        const uint32_t cdSize = iosSecretLevel_U32(pTail + eocd + 12);
        const uint32_t cdOffset = iosSecretLevel_U32(pTail + eocd + 16);
        if (numEntries == 0 || numEntries > IOSSECRET_MAX_ENTRIES || (long)cdOffset + (long)cdSize > fileSize) break;
        pCd = (unsigned char*)malloc(cdSize ? cdSize : 1);
        if (!pCd || fseek(f, (long)cdOffset, SEEK_SET) != 0 || fread(pCd, 1, cdSize, f) != cdSize) break;

        bOk = 1;
        uint32_t pos = 0;
        for (uint32_t n = 0; n < numEntries && bOk; n++) {
            if (pos + 46 > cdSize || iosSecretLevel_U32(pCd + pos) != 0x02014b50) {
                bOk = 0;
                break;
            }
            const unsigned char* h = pCd + pos;
            const uint32_t flags = iosSecretLevel_U16(h + 8);
            const uint32_t method = iosSecretLevel_U16(h + 10);
            const uint32_t compSize = iosSecretLevel_U32(h + 20);
            const uint32_t size = iosSecretLevel_U32(h + 24);
            const uint32_t nameLen = iosSecretLevel_U16(h + 28);
            const uint32_t extraLen = iosSecretLevel_U16(h + 30);
            const uint32_t commentLen = iosSecretLevel_U16(h + 32);
            const uint32_t lhOffset = iosSecretLevel_U32(h + 42);
            if (pos + 46 + nameLen > cdSize) {
                bOk = 0;
                break;
            }
            char name[512], rel[512], dest[1024];
            const uint32_t copyLen = nameLen < sizeof(name) - 1 ? nameLen : (uint32_t)sizeof(name) - 1;
            memcpy(name, h + 46, copyLen);
            name[copyLen] = 0;
            pos += 46 + nameLen + extraLen + commentLen;
            if (nameLen >= sizeof(name) || !iosSecretLevel_EntryPath(name, rel, sizeof(rel)))
                continue; // (not one of the level's files)
            if ((flags & 1) || (method != 0 && method != 8) || size > IOSSECRET_MAX_FILE || compSize > IOSSECRET_MAX_FILE) {
                stdPlatform_Printf("iosSecretLevel: can't take %s out of the zip\n", name);
                bOk = 0;
                break;
            }
            snprintf(dest, sizeof(dest), "%s/%s", pFolder, rel);
            bOk = iosSecretLevel_MakeDirsFor(dest) && iosSecretLevel_ExtractEntry(f, lhOffset, method, compSize, size, dest);
            if (!bOk) {
                stdPlatform_Printf("iosSecretLevel: failed on %s\n", name);
                break;
            }
            numFiles++;
            const char* base = strrchr(rel, '/');
            if (base && !strcasecmp(base + 1, IOSSECRET_LEVEL)) bLevel = 1;
        }
    } while (0);
    free(pTail);
    free(pCd);
    fclose(f);
    stdPlatform_Printf("iosSecretLevel: %d files from %s (%s)\n", numFiles, pZip, bOk && bLevel ? "ok" : "not usable");
    return bOk && bLevel;
}

// The secret level's folder has its level (jkl/S6L1.jkl, any case)
static int iosSecretLevel_HasLevel(const char* pFolder)
{
    char jkl[1024], level[1024];
    return iosSecretLevel_Find(pFolder, "jkl", 1, jkl, sizeof(jkl))
           && iosSecretLevel_Find(jkl, IOSSECRET_LEVEL, 0, level, sizeof(level));
}

void iosSecretLevel_Setup(void)
{
    if (!iosApp_IsMots() || !Main_bMotsCompat) return; // (never in the Jedi Knight app)

    char episode[1024], folder[1024], file[1024], zip[1024];
    if (!iosSecretLevel_Find(".", "episode", 1, episode, sizeof(episode))) return; // (no game files yet)

    // Set up already (by this, or by hand as on PC): nothing to do
    const int bFolder = iosSecretLevel_Find(episode, IOSSECRET_FOLDER, 1, folder, sizeof(folder));
    if (bFolder && iosSecretLevel_Find(folder, "episode.jk", 0, file, sizeof(file))) return;
    if (!bFolder) snprintf(folder, sizeof(folder), "%s/%s", episode, IOSSECRET_FOLDER);

    // The level's files from the zip (all of them again, if a first try stopped before its
    // episode.jk); without the zip, only if they were copied in by hand
    if (iosSecretLevel_FindZip(zip, sizeof(zip))) {
        if (mkdir(folder, 0755) != 0 && errno != EEXIST) return;
        if (!iosSecretLevel_Extract(zip, folder)) return;
    }
    if (!iosSecretLevel_HasLevel(folder)) return; // (not in the player's files: no secret level)

    // Its episode.jk last: once it is there, this is done
    char tmp[1100];
    snprintf(file, sizeof(file), "%s/episode.jk", folder);
    snprintf(tmp, sizeof(tmp), "%s.part", file);
    FILE* o = fopen(tmp, "wb");
    if (!o) return;
    const size_t len = sizeof(iosSecretLevel_aEpisodeJk) - 1;
    int bOk = fwrite(iosSecretLevel_aEpisodeJk, 1, len, o) == len;
    if (fclose(o) != 0) bOk = 0;
    if (bOk && rename(tmp, file) == 0) {
        stdPlatform_Printf("iosSecretLevel: %s set up\n", folder);
    }
    else {
        remove(tmp);
    }
}

#endif // TARGET_IOS
