// Added: entire file. See iosAutoplayPanel.h.
// Written to work with or without ARC (as iosTouch.m: what it makes lives
// for the whole process).

#ifdef TARGET_IOS

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "Platform/iOS/iosAutoplayPanel.h"
#include "Platform/iOS/iosGame.h"
#include "Enhanced/iosAutoplay.h"

// The card: this wide at most, its rows this tall at most (less on a short
// screen); the picker's tiles this tall
#define IOSAP_W 560.0
#define IOSAP_PAD 12.0
#define IOSAP_TITLE_H 34.0
#define IOSAP_ROW_H 40.0
#define IOSAP_START_H 40.0
#define IOSAP_HINT_H 22.0
#define IOSAP_TILE_H 44.0
#define IOSAP_LIST_ROW 44.0
#define IOSAP_CLOSE 30.0

// The settings (the app's own; each app, Jedi Knight's and Mysteries of the Sith's, has its own)
#define IOSAP_KEY_ROW @"iosAutoplayLevelRow"
#define IOSAP_KEY_DIFF @"iosAutoplayDifficulty"
#define IOSAP_KEY_CROWD @"iosAutoplayCrowd"
#define IOSAP_KEY_ALARM @"iosAutoplayAlarm"
#define IOSAP_KEY_WEAPONS @"iosAutoplayWeapons"
#define IOSAP_KEY_HUD @"iosAutoplayHud"
#define IOSAP_KEY_CONTROLS @"iosAutoplayControls"

enum { APROW_LEVELS = 0, APROW_DIFFICULTY, APROW_CROWD, APROW_ALARM, // (the left column)
       APROW_WEAPONS, APROW_HUD, APROW_CONTROLS,                       // (the right one)
       APROW_COUNT };
enum { APPAGE_MAIN = 0, APPAGE_WEAPONS, APPAGE_LEVELS };

// The weapons, as the weapon wheel has them: its order, its colours (by ammo)
typedef struct { int bin; int group; } iosApWeapon;
static const iosApWeapon iosAp_aJkWeapons[] = {
    { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 3 }, { 5, 2 }, { 6, 2 }, { 7, 3 }, { 8, 3 }, { 9, 2 }, { 10, 0 },
};
static const iosApWeapon iosAp_aMotsWeapons[] = {
    { 121, 0 }, { 131, 0 }, { 122, 1 }, { 132, 1 }, { 123, 1 }, { 133, 1 }, { 124, 3 }, { 134, 3 }, { 135, 2 },
    { 126, 2 }, { 127, 3 }, { 137, 3 }, { 128, 3 }, { 138, 3 }, { 129, 2 }, { 130, 4 }, { 140, 4 },
};
static const unsigned char iosAp_aGroupRGB[5][3] = {
    { 255, 214, 120 }, { 120, 200, 255 }, { 110, 220, 170 }, { 255, 105, 90 }, { 190, 150, 255 },
};
#define IOSAP_MAX_WEAPONS 17
#define IOSAP_MAX_LEVELS 64

static const iosApWeapon* iosAp_Weapons(int* pCount)
{
    if (iosGame_IsMots()) {
        *pCount = (int)(sizeof(iosAp_aMotsWeapons) / sizeof(iosAp_aMotsWeapons[0]));
        return iosAp_aMotsWeapons;
    }
    *pCount = (int)(sizeof(iosAp_aJkWeapons) / sizeof(iosAp_aJkWeapons[0]));
    return iosAp_aJkWeapons;
}

// Every weapon bit of this game (what ALL is made of, for counting)
static unsigned int iosAp_AllBits(void)
{
    int n = 0;
    const iosApWeapon* a = iosAp_Weapons(&n);
    unsigned int bits = 0;
    for (int i = 0; i < n; i++) {
        const int bit = iosAutoplay_WeaponBit(a[i].bin);
        if (bit >= 0) bits |= 1u << bit;
    }
    return bits;
}

// The port's gold (as the level list's)
static UIColor* iosAp_Gold(CGFloat a)
{
    return [UIColor colorWithRed:1.0 green:0.85 blue:0.3 alpha:a];
}

static UIColor* iosAp_GroupColour(int group, CGFloat a)
{
    if (group < 0 || group > 4) group = 0;
    return [UIColor colorWithRed:iosAp_aGroupRGB[group][0] / 255.0 green:iosAp_aGroupRGB[group][1] / 255.0
                            blue:iosAp_aGroupRGB[group][2] / 255.0 alpha:a];
}

// The wheels' lettering
static UIFont* iosAp_Font(CGFloat size)
{
    if (@available(iOS 16.0, *)) {
        return [UIFont systemFontOfSize:size weight:UIFontWeightBold width:UIFontWidthCondensed];
    }
    return [UIFont boldSystemFontOfSize:size];
}

static UILabel* iosAp_Label(UIView* parent, CGFloat size, UIColor* colour, NSTextAlignment align)
{
    UILabel* l = [[UILabel alloc] initWithFrame:CGRectZero];
    l.font = iosAp_Font(size);
    l.textColor = colour;
    l.textAlignment = align;
    l.adjustsFontSizeToFitWidth = YES;
    l.minimumScaleFactor = 0.55;
    l.userInteractionEnabled = NO;
    [parent addSubview:l];
    return l;
}

// What's set (loaded the first time the panel opens)
static iosAutoplayOptions iosAp_opt;
static int iosAp_bLoaded = 0;

static int iosAp_Clamp(NSInteger v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : (int)v);
}

static void iosAp_Load(void)
{
    if (iosAp_bLoaded) return;
    iosAp_bLoaded = 1;
    NSUserDefaults* d = [NSUserDefaults standardUserDefaults];
    // (the defaults: every level shuffled, the hardest, twice the crowd, the
    // alarm on, every weapon, no HUD, no controls)
    iosAp_opt.levelRow = [d objectForKey:IOSAP_KEY_ROW] ? iosAp_Clamp([d integerForKey:IOSAP_KEY_ROW], -1, IOSAP_MAX_LEVELS - 1) : -1;
    iosAp_opt.difficulty = [d objectForKey:IOSAP_KEY_DIFF] ? iosAp_Clamp([d integerForKey:IOSAP_KEY_DIFF], 0, 2) : 2;
    iosAp_opt.crowd = [d objectForKey:IOSAP_KEY_CROWD] ? iosAp_Clamp([d integerForKey:IOSAP_KEY_CROWD], 1, 3) : 2;
    iosAp_opt.bAlarm = [d objectForKey:IOSAP_KEY_ALARM] ? ([d boolForKey:IOSAP_KEY_ALARM] ? 1 : 0) : 1;
    iosAp_opt.weapons = IOSAUTOPLAY_WEAPONS_ALL;
    if ([d objectForKey:IOSAP_KEY_WEAPONS]) {
        const long long w = [[d objectForKey:IOSAP_KEY_WEAPONS] longLongValue];
        iosAp_opt.weapons = (w < 0 || (unsigned int)w == IOSAUTOPLAY_WEAPONS_ALL) ? IOSAUTOPLAY_WEAPONS_ALL : ((unsigned int)w & iosAp_AllBits());
        if (!iosAp_opt.weapons) iosAp_opt.weapons = IOSAUTOPLAY_WEAPONS_ALL;
    }
    iosAp_opt.bHud = [d boolForKey:IOSAP_KEY_HUD] ? 1 : 0;
    iosAp_opt.bControls = [d boolForKey:IOSAP_KEY_CONTROLS] ? 1 : 0;
}

static void iosAp_Save(void)
{
    NSUserDefaults* d = [NSUserDefaults standardUserDefaults];
    [d setInteger:iosAp_opt.levelRow forKey:IOSAP_KEY_ROW];
    [d setInteger:iosAp_opt.difficulty forKey:IOSAP_KEY_DIFF];
    [d setInteger:iosAp_opt.crowd forKey:IOSAP_KEY_CROWD];
    [d setBool:(iosAp_opt.bAlarm ? YES : NO) forKey:IOSAP_KEY_ALARM];
    [d setObject:[NSNumber numberWithLongLong:(iosAp_opt.weapons == IOSAUTOPLAY_WEAPONS_ALL ? -1LL : (long long)iosAp_opt.weapons)]
          forKey:IOSAP_KEY_WEAPONS];
    [d setBool:(iosAp_opt.bHud ? YES : NO) forKey:IOSAP_KEY_HUD];
    [d setBool:(iosAp_opt.bControls ? YES : NO) forKey:IOSAP_KEY_CONTROLS];
}

// A level row's name, as the cheat's list shows it ("3  Fuel Station")
static NSString* iosAp_LevelName(int row)
{
    int level = 0;
    char title[128], file[64];
    if (!iosGame_LevelRowInfo(row, &level, title, sizeof(title), file, sizeof(file))) return @"";
    NSString* pTitle = title[0] ? [NSString stringWithUTF8String:title] : nil;
    if (!pTitle) pTitle = [NSString stringWithUTF8String:file].stringByDeletingPathExtension;
    return [NSString stringWithFormat:@"%d  %@", level, pTitle ? pTitle : @""];
}

// ------------------------------------------------------------ the view

@interface IOSAutoplayPanelView : UIView {
@public
    int page;
    UIView* card;
    UILabel* title;
    UILabel* closeX;
    // the main page
    UIView* aRowBack[APROW_COUNT];
    UILabel* aRowName[APROW_COUNT];
    UILabel* aRowValue[APROW_COUNT];
    UILabel* aRowLess[APROW_COUNT];
    UILabel* aRowMore[APROW_COUNT];
    UILabel* start;
    UILabel* hint;
    // the weapon picker
    UILabel* aTile[IOSAP_MAX_WEAPONS];
    UILabel* allTile;
    UILabel* doneTile;
    unsigned int picking;
    int numWeapons;
    // the list of levels
    UIScrollView* list;
    UILabel* aListRow[IOSAP_MAX_LEVELS];
    UIView* aListLine[IOSAP_MAX_LEVELS];
    int numList;
}
- (void)refresh;
@end

@implementation IOSAutoplayPanelView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (!self) return self;
    self.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.45];
    self.userInteractionEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    card = [[UIView alloc] initWithFrame:CGRectZero];
    card.backgroundColor = [UIColor colorWithWhite:0.06 alpha:0.92];
    card.layer.cornerRadius = 16.0;
    card.layer.borderWidth = 1.5;
    card.layer.borderColor = iosAp_Gold(0.7).CGColor;
    card.clipsToBounds = YES;
    card.userInteractionEnabled = YES;
    [self addSubview:card];
    title = iosAp_Label(card, 20.0, iosAp_Gold(1.0), NSTextAlignmentLeft);
    // (the X: as the level list's)
    closeX = iosAp_Label(card, 15.0, [UIColor whiteColor], NSTextAlignmentCenter);
    closeX.font = [UIFont systemFontOfSize:15 weight:UIFontWeightBold];
    closeX.text = @"✕";
    closeX.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.45];
    closeX.layer.cornerRadius = IOSAP_CLOSE * 0.5;
    closeX.layer.borderWidth = 1.5;
    closeX.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.6].CGColor;
    closeX.clipsToBounds = YES;
    for (int i = 0; i < APROW_COUNT; i++) {
        aRowBack[i] = [[UIView alloc] initWithFrame:CGRectZero];
        aRowBack[i].backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
        aRowBack[i].layer.cornerRadius = 10.0;
        aRowBack[i].userInteractionEnabled = NO;
        [card addSubview:aRowBack[i]];
        aRowName[i] = iosAp_Label(card, 14.0, [UIColor colorWithWhite:0.85 alpha:1.0], NSTextAlignmentLeft);
        aRowValue[i] = iosAp_Label(card, 15.0, iosAp_Gold(1.0), NSTextAlignmentCenter);
        aRowLess[i] = iosAp_Label(card, 16.0, iosAp_Gold(0.7), NSTextAlignmentCenter);
        aRowMore[i] = iosAp_Label(card, 16.0, iosAp_Gold(0.7), NSTextAlignmentCenter);
        aRowLess[i].text = @"‹";
        aRowMore[i].text = @"›";
    }
    start = iosAp_Label(card, 18.0, [UIColor colorWithWhite:0.05 alpha:1.0], NSTextAlignmentCenter);
    start.text = @"START";
    start.backgroundColor = iosAp_Gold(1.0);
    start.layer.cornerRadius = IOSAP_START_H * 0.5;
    start.clipsToBounds = YES;
    hint = iosAp_Label(card, 12.0, [UIColor colorWithWhite:0.75 alpha:1.0], NSTextAlignmentCenter);
    hint.text = @"Double tap: new fight · Hold: exit";
    for (int i = 0; i < IOSAP_MAX_WEAPONS; i++) {
        aTile[i] = iosAp_Label(card, 13.0, [UIColor whiteColor], NSTextAlignmentCenter);
        aTile[i].numberOfLines = 2;
        aTile[i].layer.cornerRadius = 10.0;
        aTile[i].layer.borderWidth = 1.5;
        aTile[i].clipsToBounds = YES;
    }
    allTile = iosAp_Label(card, 15.0, iosAp_Gold(1.0), NSTextAlignmentCenter);
    allTile.text = @"ALL";
    allTile.layer.cornerRadius = IOSAP_START_H * 0.5;
    allTile.layer.borderWidth = 1.5;
    allTile.layer.borderColor = iosAp_Gold(0.8).CGColor;
    allTile.clipsToBounds = YES;
    doneTile = iosAp_Label(card, 16.0, [UIColor colorWithWhite:0.05 alpha:1.0], NSTextAlignmentCenter);
    doneTile.text = @"DONE";
    doneTile.backgroundColor = iosAp_Gold(1.0);
    doneTile.layer.cornerRadius = IOSAP_START_H * 0.5;
    doneTile.clipsToBounds = YES;
    list = [[UIScrollView alloc] initWithFrame:CGRectZero];
    list.backgroundColor = [UIColor clearColor];
    list.indicatorStyle = UIScrollViewIndicatorStyleWhite;
    list.alwaysBounceVertical = YES;
    [card addSubview:list];
    UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(listTapped:)];
    [list addGestureRecognizer:tap];
#if !__has_feature(objc_arc)
    [tap release];
#endif
    for (int i = 0; i < IOSAP_MAX_LEVELS; i++) {
        aListRow[i] = iosAp_Label(list, 16.0, [UIColor whiteColor], NSTextAlignmentLeft);
        aListRow[i].hidden = YES;
        aListLine[i] = [[UIView alloc] initWithFrame:CGRectZero];
        aListLine[i].backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.12];
        aListLine[i].hidden = YES;
        [list addSubview:aListLine[i]];
    }
    page = APPAGE_MAIN;
    return self;
}

// How the card is cut up on the main page: two columns of rows, START and
// the hint under them
- (CGFloat)rowH
{
    const CGFloat h = card.bounds.size.height - IOSAP_PAD * 3.0 - IOSAP_TITLE_H - IOSAP_START_H - IOSAP_HINT_H;
    return MIN((CGFloat)IOSAP_ROW_H, floor(h / 4.0));
}

- (CGRect)rowFrame:(int)i
{
    const CGFloat w = card.bounds.size.width, colW = floor((w - IOSAP_PAD * 3.0) * 0.5), rowH = [self rowH];
    const int col = i < 4 ? 0 : 1, k = i < 4 ? i : i - 4;
    return CGRectMake(IOSAP_PAD + col * (colW + IOSAP_PAD), IOSAP_PAD + IOSAP_TITLE_H + k * rowH + 2.0, colW, rowH - 4.0);
}

- (CGRect)startFrame
{
    const CGFloat w = card.bounds.size.width;
    const CGFloat y = IOSAP_PAD + IOSAP_TITLE_H + 4.0 * [self rowH] + IOSAP_PAD * 0.5;
    return CGRectMake(round(w * 0.5 - 90.0), y, 180.0, IOSAP_START_H);
}

// The picker's tiles: as many across as fit, in the wheel's order
- (CGRect)tileFrame:(int)i
{
    const CGFloat w = card.bounds.size.width;
    const int across = numWeapons > 10 ? 6 : 5;
    const CGFloat tw = floor((w - IOSAP_PAD * (across + 1)) / across);
    const int rows = (numWeapons + across - 1) / across;
    const CGFloat room = card.bounds.size.height - IOSAP_PAD * 3.0 - IOSAP_TITLE_H - IOSAP_START_H;
    const CGFloat th = MIN((CGFloat)IOSAP_TILE_H, floor(room / MAX(rows, 1)) - 6.0);
    const int r = i / across, c = i % across;
    return CGRectMake(IOSAP_PAD + c * (tw + IOSAP_PAD), IOSAP_PAD + IOSAP_TITLE_H + r * (th + 6.0), tw, th);
}

- (CGRect)allFrame
{
    const CGFloat w = card.bounds.size.width, h = card.bounds.size.height;
    return CGRectMake(round(w * 0.5) - 150.0, h - IOSAP_PAD - IOSAP_START_H, 140.0, IOSAP_START_H);
}

- (CGRect)doneFrame
{
    const CGFloat w = card.bounds.size.width, h = card.bounds.size.height;
    return CGRectMake(round(w * 0.5) + 10.0, h - IOSAP_PAD - IOSAP_START_H, 140.0, IOSAP_START_H);
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    const CGRect b = self.bounds;
    const UIEdgeInsets in = self.superview ? self.superview.safeAreaInsets : UIEdgeInsetsZero;
    const CGFloat left = MAX(in.left, 8.0), right = b.size.width - MAX(in.right, 8.0);
    const CGFloat top = MAX(in.top, 8.0), bottom = b.size.height - MAX(in.bottom, 8.0);
    const CGFloat w = MIN((CGFloat)IOSAP_W, right - left);
    CGFloat h = IOSAP_PAD * 3.0 + IOSAP_TITLE_H + 4.0 * IOSAP_ROW_H + IOSAP_START_H + IOSAP_HINT_H;
    if (page == APPAGE_LEVELS) h = IOSAP_PAD + IOSAP_TITLE_H + IOSAP_LIST_ROW * MAX(numList, 1) + 8.0;
    h = MIN(h, bottom - top);
    card.frame = CGRectMake(round((left + right - w) * 0.5), round((top + bottom - h) * 0.5), w, h);
    title.frame = CGRectMake(IOSAP_PAD + 4.0, IOSAP_PAD - 4.0, w - 2.0 * IOSAP_PAD - IOSAP_CLOSE - 12.0, IOSAP_TITLE_H - 4.0);
    closeX.frame = CGRectMake(w - IOSAP_PAD - IOSAP_CLOSE + 4.0, IOSAP_PAD - 4.0, IOSAP_CLOSE, IOSAP_CLOSE);
    for (int i = 0; i < APROW_COUNT; i++) {
        const CGRect r = [self rowFrame:i];
        aRowBack[i].frame = r;
        const CGFloat nameW = floor(r.size.width * 0.36);
        aRowName[i].frame = CGRectMake(r.origin.x + 10.0, r.origin.y, nameW - 10.0, r.size.height);
        const CGFloat vx = r.origin.x + nameW, vw = r.size.width - nameW;
        aRowLess[i].frame = CGRectMake(vx, r.origin.y, 18.0, r.size.height);
        aRowMore[i].frame = CGRectMake(vx + vw - 22.0, r.origin.y, 18.0, r.size.height);
        aRowValue[i].frame = CGRectMake(vx + 18.0, r.origin.y, vw - 40.0, r.size.height);
    }
    start.frame = [self startFrame];
    hint.frame = CGRectMake(IOSAP_PAD, CGRectGetMaxY(start.frame) + 4.0, w - 2.0 * IOSAP_PAD, IOSAP_HINT_H);
    for (int i = 0; i < IOSAP_MAX_WEAPONS; i++) aTile[i].frame = [self tileFrame:i];
    allTile.frame = [self allFrame];
    doneTile.frame = [self doneFrame];
    list.frame = CGRectMake(0.0, IOSAP_PAD + IOSAP_TITLE_H, w, h - IOSAP_PAD - IOSAP_TITLE_H);
    list.contentSize = CGSizeMake(w, IOSAP_LIST_ROW * numList);
    for (int i = 0; i < IOSAP_MAX_LEVELS; i++) {
        aListRow[i].frame = CGRectMake(IOSAP_PAD + 4.0, IOSAP_LIST_ROW * i, w - 2.0 * IOSAP_PAD - 8.0, IOSAP_LIST_ROW);
        aListLine[i].frame = CGRectMake(IOSAP_PAD, IOSAP_LIST_ROW * (i + 1) - 0.5, w - 2.0 * IOSAP_PAD, 0.5);
    }
}

// A row's name and value as shown ("LEVELS", "SHUFFLE ALL")
static void iosAp_RowText(int row, NSString** pName, NSString** pValue)
{
    static NSString* aNames[APROW_COUNT] = { @"LEVELS", @"DIFFICULTY", @"CROWD", @"ALARM", @"WEAPONS", @"HUD", @"CONTROLS" };
    static NSString* aDiff[3] = { @"EASY", @"MEDIUM", @"HARD" };
    *pName = aNames[row];
    switch (row) {
    case APROW_LEVELS:
        *pValue = iosAp_opt.levelRow >= 0 ? iosAp_LevelName(iosAp_opt.levelRow) : @"SHUFFLE ALL";
        break;
    case APROW_DIFFICULTY: *pValue = aDiff[iosAp_Clamp(iosAp_opt.difficulty, 0, 2)]; break;
    case APROW_CROWD: *pValue = [NSString stringWithFormat:@"%d×", iosAp_opt.crowd]; break;
    case APROW_ALARM: *pValue = iosAp_opt.bAlarm ? @"ON" : @"OFF"; break;
    case APROW_WEAPONS: {
        if (iosAp_opt.weapons == IOSAUTOPLAY_WEAPONS_ALL) {
            *pValue = @"ALL";
            break;
        }
        int n = 0;
        for (int b = 0; b < 32; b++) n += (iosAp_opt.weapons >> b) & 1;
        *pValue = [NSString stringWithFormat:@"%d PICKED", n];
        break;
    }
    case APROW_HUD: *pValue = iosAp_opt.bHud ? @"ON" : @"OFF"; break;
    default: *pValue = iosAp_opt.bControls ? @"ON" : @"OFF"; break;
    }
}

- (void)refresh
{
    const int bMain = page == APPAGE_MAIN, bWeapons = page == APPAGE_WEAPONS, bLevels = page == APPAGE_LEVELS;
    title.text = bMain ? @"AUTOPLAY" : bWeapons ? @"AUTOPLAY WEAPONS" : [NSString stringWithFormat:@"AUTOPLAY LEVEL  (%d)", numList];
    for (int i = 0; i < APROW_COUNT; i++) {
        NSString* name = nil;
        NSString* value = nil;
        iosAp_RowText(i, &name, &value);
        aRowName[i].text = name;
        aRowValue[i].text = value;
        aRowBack[i].hidden = aRowName[i].hidden = aRowValue[i].hidden = aRowLess[i].hidden = aRowMore[i].hidden = !bMain;
    }
    start.hidden = hint.hidden = !bMain;
    int n = 0;
    const iosApWeapon* a = iosAp_Weapons(&n);
    numWeapons = n;
    for (int i = 0; i < IOSAP_MAX_WEAPONS; i++) {
        aTile[i].hidden = !bWeapons || i >= n;
        if (i >= n) continue;
        const int bit = iosAutoplay_WeaponBit(a[i].bin);
        const int bOn = bit >= 0 && (picking & (1u << bit));
        const char* pName = iosGame_GetWeaponName(a[i].bin);
        aTile[i].text = pName ? [NSString stringWithUTF8String:pName] : @"?";
        aTile[i].textColor = bOn ? [UIColor colorWithWhite:0.05 alpha:1.0] : iosAp_GroupColour(a[i].group, 0.55);
        aTile[i].backgroundColor = bOn ? iosAp_GroupColour(a[i].group, 0.95) : [UIColor colorWithWhite:1.0 alpha:0.04];
        aTile[i].layer.borderColor = iosAp_GroupColour(a[i].group, bOn ? 1.0 : 0.35).CGColor;
    }
    allTile.hidden = doneTile.hidden = !bWeapons;
    list.hidden = !bLevels;
    for (int i = 0; i < IOSAP_MAX_LEVELS; i++) {
        aListRow[i].hidden = aListLine[i].hidden = !bLevels || i >= numList;
        if (!bLevels || i >= numList) continue;
        // (the number in gold, then the name; the one picked before ticked)
        int level = 0;
        char t[128], f[64];
        if (!iosGame_LevelRowInfo(i, &level, t, sizeof(t), f, sizeof(f))) t[0] = f[0] = 0;
        NSString* pTitle = t[0] ? [NSString stringWithUTF8String:t] : [NSString stringWithUTF8String:f].stringByDeletingPathExtension;
        NSMutableAttributedString* s = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%d", level]
                                                                               attributes:@{ NSForegroundColorAttributeName: iosAp_Gold(1.0) }];
        NSString* rest = [NSString stringWithFormat:@"   %@%@", pTitle ? pTitle : @"", i == iosAp_opt.levelRow ? @"  ✓" : @""];
        NSAttributedString* r = [[NSAttributedString alloc] initWithString:rest attributes:@{ NSForegroundColorAttributeName: [UIColor whiteColor] }];
        [s appendAttributedString:r];
        aListRow[i].attributedText = s;
#if !__has_feature(objc_arc)
        [r release];
        [s release];
#endif
    }
}

- (void)setPage:(int)p
{
    page = p;
    if (p == APPAGE_LEVELS) {
        numList = MIN(iosAutoplay_LevelRows(), IOSAP_MAX_LEVELS);
        [list setContentOffset:CGPointZero animated:NO];
    }
    [self setNeedsLayout];
    [self layoutIfNeeded];
    [self refresh];
}

// A row tapped: on its right half (dir 1) or its left (-1)
- (void)stepRow:(int)row dir:(int)dir
{
    switch (row) {
    case APROW_LEVELS:
        // (two values: either way goes to the other one; PICK ONE picks)
        if (iosAp_opt.levelRow >= 0) iosAp_opt.levelRow = -1;
        else {
            [self setPage:APPAGE_LEVELS];
            return;
        }
        break;
    case APROW_DIFFICULTY: iosAp_opt.difficulty = (iosAp_opt.difficulty + dir + 3) % 3; break;
    case APROW_CROWD: iosAp_opt.crowd = (iosAp_opt.crowd - 1 + dir + 3) % 3 + 1; break;
    case APROW_ALARM: iosAp_opt.bAlarm = !iosAp_opt.bAlarm; break;
    case APROW_WEAPONS:
        picking = iosAp_opt.weapons == IOSAUTOPLAY_WEAPONS_ALL ? iosAp_AllBits() : iosAp_opt.weapons;
        [self setPage:APPAGE_WEAPONS];
        return;
    case APROW_HUD: iosAp_opt.bHud = !iosAp_opt.bHud; break;
    case APROW_CONTROLS: iosAp_opt.bControls = !iosAp_opt.bControls; break;
    default: return;
    }
    iosAp_Save();
    [self refresh];
}

- (void)listTapped:(UITapGestureRecognizer*)g
{
    if (page != APPAGE_LEVELS) return;
    const CGPoint p = [g locationInView:list];
    const int row = (int)floor(p.y / IOSAP_LIST_ROW);
    if (row < 0 || row >= numList) return;
    iosAp_opt.levelRow = row;
    iosAp_Save();
    [self setPage:APPAGE_MAIN];
}

static int iosAp_Hit(UIView* v, CGPoint p)
{
    return !v.hidden && CGRectContainsPoint(CGRectInset(v.frame, -4.0, -4.0), p);
}

// A finger lifted: on a row, START, a tile, the X; off the card, the panel closes
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    UITouch* t = [touches anyObject];
    if (!t) return;
    const CGPoint p = [t locationInView:card];
    if (!CGRectContainsPoint(card.bounds, p)) {
        iosAutoplayPanel_Close();
        return;
    }
    if (iosAp_Hit(closeX, p)) {
        // (the picker or the list: back to the main page, as it was)
        if (page != APPAGE_MAIN) [self setPage:APPAGE_MAIN];
        else iosAutoplayPanel_Close();
        return;
    }
    if (page == APPAGE_MAIN) {
        for (int i = 0; i < APROW_COUNT; i++) {
            const CGRect r = [self rowFrame:i];
            if (!CGRectContainsPoint(r, p)) continue;
            const CGFloat nameW = floor(r.size.width * 0.36);
            const CGFloat mid = r.origin.x + nameW + (r.size.width - nameW) * 0.5;
            [self stepRow:i dir:(p.x < mid && p.x > r.origin.x + nameW ? -1 : 1)];
            return;
        }
        if (iosAp_Hit(start, p)) {
            iosAutoplayOptions o = iosAp_opt;
            if (o.levelRow >= iosAutoplay_LevelRows()) o.levelRow = -1;
            iosAutoplayPanel_Close();
            if (!iosAutoplay_Start(&o)) iosGame_ShowMessage("AUTOPLAY can't start here");
        }
        return;
    }
    if (page == APPAGE_WEAPONS) {
        int n = 0;
        const iosApWeapon* a = iosAp_Weapons(&n);
        for (int i = 0; i < n && i < IOSAP_MAX_WEAPONS; i++) {
            if (!iosAp_Hit(aTile[i], p)) continue;
            const int bit = iosAutoplay_WeaponBit(a[i].bin);
            if (bit < 0) return;
            const unsigned int next = picking ^ (1u << bit);
            if (next & iosAp_AllBits()) picking = next; // (one at least stays ticked)
            [self refresh];
            return;
        }
        if (iosAp_Hit(allTile, p)) {
            picking = iosAp_AllBits();
            [self refresh];
        }
        else if (iosAp_Hit(doneTile, p)) {
            iosAp_opt.weapons = (picking & iosAp_AllBits()) == iosAp_AllBits() ? IOSAUTOPLAY_WEAPONS_ALL : (picking & iosAp_AllBits());
            iosAp_Save();
            [self setPage:APPAGE_MAIN];
        }
    }
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {}

@end

// ------------------------------------------------------------ C API

static IOSAutoplayPanelView* iosAp_pPanel = nil;

void iosAutoplayPanel_Open(void* pOverlay)
{
    @autoreleasepool {
        UIView* host = (__bridge UIView*)pOverlay;
        if (!host) return;
        if (iosGame_IsMultiplayer()) {
            iosGame_ShowMessage("AUTOPLAY: single player only");
            return;
        }
        if (!iosAutoplay_CanStart()) {
            iosGame_ShowMessage("AUTOPLAY can't start here");
            return;
        }
        iosAp_Load();
        if (iosAp_opt.levelRow >= iosAutoplay_LevelRows()) iosAp_opt.levelRow = -1;
        if (!iosAp_pPanel) iosAp_pPanel = [[IOSAutoplayPanelView alloc] initWithFrame:host.bounds];
        if (iosAp_pPanel.superview != host) {
            [iosAp_pPanel removeFromSuperview];
            [host addSubview:iosAp_pPanel];
        }
        iosAp_pPanel.frame = host.bounds;
        [host bringSubviewToFront:iosAp_pPanel];
        iosAp_pPanel.hidden = NO;
        [iosAp_pPanel setPage:APPAGE_MAIN];
        iosGame_SetHoldReason(IOSGAME_HOLD_AUTOPLAY, 1); // (the game holds still while it is open)
    }
}

void iosAutoplayPanel_Close(void)
{
    if (!iosAp_pPanel || iosAp_pPanel.hidden) return;
    iosAp_pPanel.hidden = YES;
    iosGame_SetHoldReason(IOSGAME_HOLD_AUTOPLAY, 0);
}

int iosAutoplayPanel_IsOpen(void)
{
    return iosAp_pPanel && !iosAp_pPanel.hidden;
}

void iosAutoplayPanel_Tick(void)
{
    if (!iosAutoplayPanel_IsOpen()) return;
    @autoreleasepool {
        if (iosGame_IsMultiplayer()) {
            iosAutoplayPanel_Close();
            return;
        }
        UIView* host = iosAp_pPanel.superview;
        if (host && !CGRectEqualToRect(iosAp_pPanel.frame, host.bounds)) {
            iosAp_pPanel.frame = host.bounds;
            [iosAp_pPanel setNeedsLayout];
        }
        if (host && host.subviews.lastObject != iosAp_pPanel) [host bringSubviewToFront:iosAp_pPanel];
    }
}

#endif // TARGET_IOS
