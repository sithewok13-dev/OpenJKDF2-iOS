// Added: entire file. See iosCheatLaugh.h.

#ifdef TARGET_IOS

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <stdlib.h>

#include "Platform/iOS/iosCheatLaugh.h"

// (each made the first time it is picked and kept for the next, by its path; NSNull: can't be read).
// Not ARC (see config_platform_deps.cmake): these three are owned here, for good.
static NSMutableDictionary<NSString*, id>* iosCheatLaugh_players;
static AVAudioPlayer* iosCheatLaugh_playing;
static NSString* iosCheatLaugh_last;

// The ones to pick from, looked for again at each switch-on (so a file added
// meanwhile counts): the Files app's Emperor folder, else the app's own (a build's own)
static NSArray<NSString*>* iosCheatLaugh_Recordings(void)
{
    NSMutableArray<NSString*>* aPath = [NSMutableArray array];
    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    static const char* aExtC[] = IOSCHEATLAUGH_EXTS;
    NSMutableSet<NSString*>* aExt = [NSMutableSet set];
    for (size_t i = 0; i < sizeof(aExtC) / sizeof(aExtC[0]); i++) [aExt addObject:@(aExtC[i])];
    for (NSString* name in docs ? [fm contentsOfDirectoryAtPath:docs error:NULL] : nil) {
        if ([name caseInsensitiveCompare:@IOSCHEATLAUGH_FOLDER] != NSOrderedSame) continue;
        NSString* dir = [docs stringByAppendingPathComponent:name];
        // (by name, so which ones count past IOSCHEATLAUGH_MAX doesn't change from one look to the next)
        NSArray<NSString*>* aFile = [[fm contentsOfDirectoryAtPath:dir error:NULL] sortedArrayUsingSelector:@selector(compare:)];
        for (NSString* file in aFile) {
            if (aPath.count >= IOSCHEATLAUGH_MAX) break;
            if ([file hasPrefix:@"."] || ![aExt containsObject:file.pathExtension.lowercaseString]) continue;
            NSString* path = [dir stringByAppendingPathComponent:file];
            NSDictionary* attr = [fm attributesOfItemAtPath:path error:NULL];
            if (![[attr objectForKey:NSFileType] isEqual:NSFileTypeRegular] || [[attr objectForKey:NSFileSize] longLongValue] > IOSCHEATLAUGH_MAX_BYTES) continue;
            [aPath addObject:path];
        }
    }
    if (!aPath.count) {
        static const char* aName[] = IOSCHEATLAUGH_NAMES;
        for (size_t i = 0; i < sizeof(aName) / sizeof(aName[0]); i++) {
            NSString* path = [[NSBundle mainBundle] pathForResource:@(aName[i]) ofType:@"mp3"];
            if (path) [aPath addObject:path];
        }
    }
    return aPath;
}

static AVAudioPlayer* iosCheatLaugh_Player(NSString* path)
{
    if (!iosCheatLaugh_players) iosCheatLaugh_players = [[NSMutableDictionary alloc] init];
    id p = [iosCheatLaugh_players objectForKey:path];
    if (p) return p == [NSNull null] ? nil : p;
    AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:NULL];
    if (player) {
        player.numberOfLoops = 0;
        [player prepareToPlay];
    }
    [iosCheatLaugh_players setObject:(player ? (id)player : [NSNull null]) forKey:path];
    [player release]; // (the dictionary keeps it)
    return player;
}

int iosCheatLaugh_Play(float volume)
{
    if (iosCheatLaugh_playing && iosCheatLaugh_playing.playing)
        return 0; // (no second over the first)
    const float v = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    if (v <= 0.0f)
        return 0; // (sound effects off)
    // one at random, not the last one (unless it is the only one that can be read); only the one
    // picked is read, and one that can't be is passed over
    NSArray<NSString*>* aPath = iosCheatLaugh_Recordings();
    NSMutableArray<NSString*>* aPick = [NSMutableArray arrayWithArray:aPath];
    if (iosCheatLaugh_last) [aPick removeObject:iosCheatLaugh_last];
    NSString* path = nil;
    AVAudioPlayer* p = nil;
    while (!p && aPick.count) {
        const NSUInteger k = arc4random_uniform((uint32_t)aPick.count);
        path = [aPick objectAtIndex:k];
        p = iosCheatLaugh_Player(path);
        if (!p) [aPick removeObjectAtIndex:k];
    }
    if (!p && iosCheatLaugh_last && [aPath containsObject:iosCheatLaugh_last]) {
        path = iosCheatLaugh_last;
        p = iosCheatLaugh_Player(path);
    }
    if (!p)
        return 0;
    p.volume = v;
    p.currentTime = 0;
    if (![p play])
        return 0;
    iosCheatLaugh_playing = p; // (the dictionary keeps it)
    if (path != iosCheatLaugh_last) {
        [iosCheatLaugh_last release];
        iosCheatLaugh_last = [path copy];
    }
    return 1;
}

#endif // TARGET_IOS
