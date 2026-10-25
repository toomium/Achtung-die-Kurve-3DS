#ifdef __3DS__

#include "shop3ds.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "config.h"
#include "gfx.h"
#include "input.h"
#include "particle.h"
#include "platform.h"
#include "player.h"
#include "slotmenu3ds.h"
#include "sound.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

enum itemKind {
    KIND_THEME,         /* The color of the playfield */
    KIND_PATTERN,       /* A pattern over it */
    KIND_FRAME,         /* The color of the frame around it */
    KIND_BLAST,         /* The particles of a crash */
    KIND_COLOR,         /* A color for the trail, and the player */
    KIND_HEAD,          /* A marker on the head */
    N_KINDS
};

struct item {
    enum itemKind kind;
    const char *name;
    SDL_Color color;    /* The field or frame color, or the main color of the effect;
                           a color of the palette has its own (see itemColor) */
    int price;          /* In Play Coins */
};

/* Every item has a name here, so the table cannot drift away from the
 * numbers: the bit of an item in shopOwned is its number. New items go at the end. */
enum itemId {
    ITEM_THEME_NIGHT,
    ITEM_THEME_FOREST,
    ITEM_THEME_WINE,
    ITEM_THEME_RETRO,
    ITEM_THEME_SUNSET,
    ITEM_BLAST_CONFETTI,
    ITEM_BLAST_SPARKS,
    ITEM_BLAST_EMBERS,
    ITEM_BLAST_RING,
    ITEM_THEME_OCEAN,
    ITEM_THEME_GRAPE,
    ITEM_THEME_GOLD,
    ITEM_BLAST_NOVA,
    ITEM_BLAST_BLOOM,
    ITEM_THEME_TEAL,
    ITEM_THEME_ROSE,
    ITEM_THEME_INDIGO,
    ITEM_THEME_LIME,
    ITEM_THEME_PUMPKIN,
    ITEM_THEME_SLATE,
    ITEM_FRAME_ICE,
    ITEM_FRAME_LIME,
    ITEM_FRAME_GOLD,
    ITEM_FRAME_ROSE,
    ITEM_FRAME_RED,
    ITEM_FRAME_VIOLET,
    ITEM_FRAME_MINT,
    ITEM_FRAME_ORANGE,
    ITEM_BLAST_BURST,
    ITEM_BLAST_SMOKE,
    ITEM_BLAST_STARS,
    ITEM_HEAD_DOT,
    ITEM_HEAD_DIAMOND,
    ITEM_HEAD_PLUS,
    ITEM_HEAD_RING,
    ITEM_HEAD_STAR,
    ITEM_HEAD_SQUARE,
    ITEM_COLOR_8,
    ITEM_COLOR_9,
    ITEM_COLOR_10,
    ITEM_COLOR_11,
    ITEM_COLOR_12,
    ITEM_COLOR_13,
    ITEM_COLOR_14,
    ITEM_COLOR_15,
    ITEM_COLOR_16,
    ITEM_COLOR_17,
    ITEM_COLOR_18,
    ITEM_COLOR_19,
    ITEM_COLOR_20,
    ITEM_COLOR_21,
    ITEM_COLOR_22,
    ITEM_COLOR_23,
    ITEM_PATTERN_GRID,
    ITEM_PATTERN_DOTS,
    ITEM_PATTERN_STARS,
    ITEM_PATTERN_STRIPES,
    ITEM_PATTERN_CHECKER,
    ITEM_PATTERN_LINES,
    N_ITEM_IDS
};

static const struct item items[N_ITEM_IDS] = {
    [ITEM_THEME_NIGHT] = {KIND_THEME, "NIGHT", {0x0A, 0x12, 0x48, 0}, 3},
    [ITEM_THEME_FOREST] = {KIND_THEME, "FOREST", {0x06, 0x42, 0x20, 0}, 3},
    [ITEM_THEME_WINE] = {KIND_THEME, "WINE", {0x52, 0x0D, 0x24, 0}, 3},
    [ITEM_THEME_RETRO] = {KIND_THEME, "RETRO", {0x00, 0x46, 0x14, 0}, 3},
    [ITEM_THEME_SUNSET] = {KIND_THEME, "SUNSET", {0x60, 0x24, 0x20, 0}, 3},
    [ITEM_BLAST_CONFETTI] = {KIND_BLAST, "CONFETTI", {0xFF, 0x6B, 0xC8, 0}, 5},
    [ITEM_BLAST_SPARKS] = {KIND_BLAST, "SPARKS", {0xFF, 0xE0, 0x90, 0}, 5},
    [ITEM_BLAST_EMBERS] = {KIND_BLAST, "EMBERS", {0xFF, 0x7A, 0x1A, 0}, 5},
    [ITEM_BLAST_RING] = {KIND_BLAST, "RING", {0x5F, 0xD7, 0x78, 0}, 5},
    [ITEM_THEME_OCEAN] = {KIND_THEME, "OCEAN", {0x00, 0x40, 0x58, 0}, 3},
    [ITEM_THEME_GRAPE] = {KIND_THEME, "GRAPE", {0x36, 0x16, 0x62, 0}, 3},
    [ITEM_THEME_GOLD] = {KIND_THEME, "GOLD", {0x50, 0x3C, 0x00, 0}, 3},
    [ITEM_BLAST_NOVA] = {KIND_BLAST, "NOVA", {0xFF, 0xFF, 0xFF, 0}, 6},
    [ITEM_BLAST_BLOOM] = {KIND_BLAST, "BLOOM", {0xFF, 0x8A, 0xE0, 0}, 6},
    [ITEM_THEME_TEAL] = {KIND_THEME, "TEAL", {0x00, 0x4E, 0x48, 0}, 3},
    [ITEM_THEME_ROSE] = {KIND_THEME, "ROSE", {0x64, 0x1C, 0x44, 0}, 3},
    [ITEM_THEME_INDIGO] = {KIND_THEME, "INDIGO", {0x24, 0x22, 0x6C, 0}, 3},
    [ITEM_THEME_LIME] = {KIND_THEME, "LIME", {0x2C, 0x48, 0x00, 0}, 3},
    [ITEM_THEME_PUMPKIN] = {KIND_THEME, "PUMPKIN", {0x6C, 0x34, 0x00, 0}, 3},
    [ITEM_THEME_SLATE] = {KIND_THEME, "SLATE", {0x24, 0x38, 0x50, 0}, 3},
    [ITEM_FRAME_ICE] = {KIND_FRAME, "ICE", {0x7F, 0xE0, 0xFF, 0}, 2},
    [ITEM_FRAME_LIME] = {KIND_FRAME, "LIME", {0xA0, 0xFF, 0x40, 0}, 2},
    [ITEM_FRAME_GOLD] = {KIND_FRAME, "GOLD", {0xFF, 0xC8, 0x57, 0}, 2},
    [ITEM_FRAME_ROSE] = {KIND_FRAME, "ROSE", {0xFF, 0x6B, 0xB5, 0}, 2},
    [ITEM_FRAME_RED] = {KIND_FRAME, "RED", {0xFF, 0x40, 0x40, 0}, 2},
    [ITEM_FRAME_VIOLET] = {KIND_FRAME, "VIOLET", {0xB0, 0x7B, 0xFF, 0}, 2},
    [ITEM_FRAME_MINT] = {KIND_FRAME, "MINT", {0x5F, 0xFF, 0xB0, 0}, 2},
    [ITEM_FRAME_ORANGE] = {KIND_FRAME, "ORANGE", {0xFF, 0x8A, 0x1A, 0}, 2},
    [ITEM_BLAST_BURST] = {KIND_BLAST, "BURST", {0x5F, 0xAE, 0xFF, 0}, 5},
    [ITEM_BLAST_SMOKE] = {KIND_BLAST, "SMOKE", {0xB4, 0xB4, 0xB4, 0}, 4},
    [ITEM_BLAST_STARS] = {KIND_BLAST, "STARS", {0xFF, 0xE8, 0x90, 0}, 6},
    [ITEM_HEAD_DOT] = {KIND_HEAD, "DOT", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_HEAD_DIAMOND] = {KIND_HEAD, "DIAMOND", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_HEAD_PLUS] = {KIND_HEAD, "PLUS", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_HEAD_RING] = {KIND_HEAD, "RING", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_HEAD_STAR] = {KIND_HEAD, "STAR", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_HEAD_SQUARE] = {KIND_HEAD, "SQUARE", {0xFF, 0xFF, 0xFF, 0}, 4},
    [ITEM_COLOR_8] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_9] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_10] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_11] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_12] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_13] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_14] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_15] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_16] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_17] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_18] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_19] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_20] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_21] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_22] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_COLOR_23] = {KIND_COLOR, NULL, {0, 0, 0, 0}, 2},
    [ITEM_PATTERN_GRID] = {KIND_PATTERN, "GRID", {0, 0, 0, 0}, 3},
    [ITEM_PATTERN_DOTS] = {KIND_PATTERN, "DOTS", {0, 0, 0, 0}, 3},
    [ITEM_PATTERN_STARS] = {KIND_PATTERN, "STARS", {0, 0, 0, 0}, 3},
    [ITEM_PATTERN_STRIPES] = {KIND_PATTERN, "STRIPES", {0, 0, 0, 0}, 3},
    [ITEM_PATTERN_CHECKER] = {KIND_PATTERN, "CHECKER", {0, 0, 0, 0}, 3},
    [ITEM_PATTERN_LINES] = {KIND_PATTERN, "LINES", {0, 0, 0, 0}, 3}
};
#define N_ITEMS ((int)(sizeof(items) / sizeof(items[0])))

/* shopOwned has 64 bits. */
_Static_assert(sizeof(items) / sizeof(items[0]) <= 64, "too many shop items");

/* Where the color items begin: the palette entry of the first one. */
static int colorIndexOf(int item)
{
    int n = N_BASE_COLORS;
    for (int i = 0; i < item; ++i)
        n += items[i].kind == KIND_COLOR;
    return n;
}

static const char *itemName(int item)
{
    return items[item].kind == KIND_COLOR ? colorName(colorIndexOf(item))
                                          : items[item].name;
}

static SDL_Color itemColor(int item)
{
    return items[item].kind == KIND_COLOR ? colors[colorIndexOf(item)]
                                          : items[item].color;
}

/* ---- The effects in the game ---- */

/* ---- The playfield ---- */

static void fill(SDL_Surface *s, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect r = {x, y, w, h};
    SDL_FillRect(s, &r, uiMap(s, c));
}

/* A pattern over `area` of `s`, in a shade of `base` (lighter, or darker for
 * the lines). Small rectangles only, so it is quick to draw once. */
static void drawPattern(SDL_Surface *s, SDL_Rect area, int pattern,
                        SDL_Color base)
{
    SDL_Color light = uiMix(base, (SDL_Color){255, 255, 255, 0}, 14);
    SDL_Color faint = uiMix(base, (SDL_Color){255, 255, 255, 0}, 7);
    SDL_Color dark = uiMix(base, (SDL_Color){0, 0, 0, 0}, 30);
    int x1 = area.x + area.w, y1 = area.y + area.h;

    switch (pattern) {
    case ITEM_PATTERN_GRID:
        for (int x = area.x; x < x1; x += 20)
            fill(s, x, area.y, 1, area.h, light);
        for (int y = area.y; y < y1; y += 20)
            fill(s, area.x, y, area.w, 1, light);
        break;
    case ITEM_PATTERN_DOTS:
        for (int y = area.y + 8; y < y1; y += 16) {
            for (int x = area.x + 8; x < x1; x += 16)
                fill(s, x, y, 2, 2, light);
        }
        break;
    case ITEM_PATTERN_STARS: {
        /* The same stars every time: a small generator with a fixed start. */
        unsigned r = 12345;
        for (int i = 0; i < area.w * area.h / 900 + 8; ++i) {
            r = r * 1103515245u + 12345u;
            int x = area.x + (r >> 8) % area.w;
            r = r * 1103515245u + 12345u;
            int y = area.y + (r >> 8) % area.h;
            fill(s, x, y, 1 + (r >> 4) % 2, 1 + (r >> 4) % 2, light);
        }
        break;
    }
    case ITEM_PATTERN_STRIPES:
        for (int y = area.y; y < y1; y += 16)
            fill(s, area.x, y, area.w, 8, faint);
        break;
    case ITEM_PATTERN_CHECKER:
        for (int y = area.y; y < y1; y += 24) {
            for (int x = area.x + ((y - area.y) / 24 % 2) * 24; x < x1; x += 48)
                fill(s, x, y, 24, 24, faint);
        }
        break;
    case ITEM_PATTERN_LINES:
        for (int y = area.y + 1; y < y1; y += 2)
            fill(s, area.x, y, area.w, 1, dark);
        break;
    default:
        break;
    }
}

static bool patternInUse(void)
{
    return shopPattern >= 0 && shopPattern < N_ITEMS
        && items[shopPattern].kind == KIND_PATTERN;
}

static bool themeInUse(void)
{
    return shopTheme >= 0 && shopTheme < N_ITEMS
        && items[shopTheme].kind == KIND_THEME;
}

void shopClearPlayfield(SDL_Surface *s)
{
    /* The ground with its pattern is drawn once and copied on every frame. */
    static SDL_Surface *ground = NULL;
    static int groundFor = -1;

    SDL_Color base = themeInUse() ? itemColor(shopTheme)
                                  : (SDL_Color){BG_R, BG_G, BG_B, 0};

    if (!patternInUse()) {
        if (ground != NULL) {
            SDL_FreeSurface(ground);
            ground = NULL;
        }
        if (themeInUse())
            SDL_FillRect(s, &s->clip_rect, uiMap(s, base));
        else
            clearBackground(s);
        return;
    }

    int key = (shopTheme + 1) * 100 + shopPattern;
    if (ground == NULL || groundFor != key || ground->w != s->w
        || ground->h != s->h) {
        SDL_FreeSurface(ground);
        ground = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h,
                                      s->format->BitsPerPixel, s->format->Rmask,
                                      s->format->Gmask, s->format->Bmask, 0);
        if (ground == NULL) {
            clearBackground(s);
            return;
        }
        SDL_FillRect(ground, NULL, uiMap(ground, base));
        drawPattern(ground, (SDL_Rect){0, 0, s->w, s->h}, shopPattern, base);
        groundFor = key;
    }
    SDL_BlitSurface(ground, NULL, s, NULL);
}

bool shopFrameColor(SDL_Color *color)
{
    if (shopFrame < 0 || shopFrame >= N_ITEMS
        || items[shopFrame].kind != KIND_FRAME)
        return false;
    *color = itemColor(shopFrame);
    return true;
}

void shopCrashBurst(double x, double y, double dir, SDL_Color c)
{
    const double all = 2 * M_PI;

    switch (shopBlast) {
    case ITEM_BLAST_CONFETTI: {
        static const SDL_Color palette[] = {
            {0xFF, 0x6B, 0x6B, 0}, {0xFF, 0xC8, 0x57, 0}, {0x5F, 0xD7, 0x78, 0},
            {0x5F, 0xAE, 0xFF, 0}, {0xC8, 0x7B, 0xFF, 0}, {0xFF, 0xFF, 0xFF, 0}
        };
        for (unsigned int i = 0; i < sizeof(palette) / sizeof(palette[0]); ++i)
            addParticles(12, x, y, 0.02, 0.35, 0, all, 1, 1, 0.0005,
                         palette[i].r, palette[i].r, palette[i].g, palette[i].g,
                         palette[i].b, palette[i].b);
        break;
    }
    case ITEM_BLAST_SPARKS:
        addParticles(70, x, y, 0.1, 0.7, 0, all, 0.6, 1, 0.0009,
                     255, 255, 200, 255, 80, 200);
        break;
    case ITEM_BLAST_EMBERS:
        addParticles(60, x, y, 0.03, 0.25, -M_PI_2 - 0.9, -M_PI_2 + 0.9, 1, 1,
                     0.0004, 255, 255, 80, 170, 0, 30);
        break;
    case ITEM_BLAST_RING:
        addParticles(60, x, y, 0.28, 0.30, 0, all, 1, 1, 0.0006,
                     c.r, c.r, c.g, c.g, c.b, c.b);
        break;
    case ITEM_BLAST_NOVA:
        /* A white ring runs ahead of one in the player's color. */
        addParticles(70, x, y, 0.45, 0.47, 0, all, 1, 1, 0.0008,
                     255, 255, 255, 255, 255, 255);
        addParticles(70, x, y, 0.25, 0.27, 0, all, 1, 1, 0.0006,
                     c.r, c.r, c.g, c.g, c.b, c.b);
        break;
    case ITEM_BLAST_BLOOM:
        addParticles(120, x, y, 0.01, 0.16, 0, all, 1, 1, 0.0003,
                     c.r, 255, c.g, 255, c.b, 255);
        break;
    case ITEM_BLAST_BURST:
        /* Eight thin rays. */
        for (int k = 0; k < 8; ++k)
            addParticles(10, x, y, 0.1, 0.5, k * M_PI_4 - 0.04,
                         k * M_PI_4 + 0.04, 1, 1, 0.0006,
                         c.r, c.r, c.g, c.g, c.b, c.b);
        break;
    case ITEM_BLAST_SMOKE:
        addParticles(90, x, y, 0.01, 0.12, 0, all, 1, 1, 0.00025,
                     150, 200, 150, 200, 150, 200);
        break;
    case ITEM_BLAST_STARS:
        addParticles(40, x, y, 0.02, 0.2, 0, all, 1, 1, 0.0002,
                     255, 255, 220, 255, 120, 220);
        break;
    default:
        /* The normal burst, a cone behind the player. */
        addParticles(50, x, y, 0, 0.5, dir + M_PI - M_PI_4, dir + M_PI + M_PI_4,
                     1, 1, 0.0005, c.r, c.r, c.b, c.b, c.g, c.g);
        break;
    }
}

/* ---- Colors and heads ---- */

static bool isOwned(int item);

bool shopColorOwned(int color)
{
    if (color < N_BASE_COLORS)
        return true;
    for (int i = 0; i < N_ITEMS; ++i) {
        if (items[i].kind == KIND_COLOR && colorIndexOf(i) == color)
            return isOwned(i);
    }
    return false;
}

/* A head style is the item's index plus one; 0 is no marker. The consoles send
 * it to each other, so anything unknown has to look like no marker. */
unsigned char shopOwnedHead(unsigned char style)
{
    int item = style - 1;
    if (item < 0 || item >= N_ITEMS || items[item].kind != KIND_HEAD
        || !isOwned(item))
        return 0;
    return style;
}

int shopOwnedHeads(unsigned char *list, int max)
{
    int n = 0;

    list[n++] = 0;
    for (int i = 0; i < N_ITEMS && n < max; ++i) {
        if (items[i].kind == KIND_HEAD && isOwned(i))
            list[n++] = i + 1;
    }
    return n;
}

unsigned char shopCycleHead(unsigned char style, int dir)
{
    unsigned char list[N_ITEMS + 1];
    int n = shopOwnedHeads(list, N_ITEMS + 1), at = 0;

    for (int i = 0; i < n; ++i) {
        if (list[i] == style)
            at = i;
    }
    return list[(at + dir + n) % n];
}

const char *shopHeadName(unsigned char style)
{
    return style == 0 || style > N_ITEMS ? "NONE" : items[style - 1].name;
}

/* A head marker of the given size around (cx, cy): `n` is one step. */
static void drawHead(SDL_Surface *s, int head, int cx, int cy, int n,
                     SDL_Color color)
{
    Uint32 c = uiMap(s, color);
#define BOX(dx, dy, w, h) do { \
        SDL_Rect r_ = {cx + (dx) * n, cy + (dy) * n, (w) * n, (h) * n}; \
        SDL_FillRect(s, &r_, c); } while (0)

    switch (head) {
    case ITEM_HEAD_DOT:
        BOX(-1, -1, 3, 3);
        break;
    case ITEM_HEAD_DIAMOND:
        BOX(0, -2, 1, 5);
        BOX(-1, -1, 3, 3);
        BOX(-2, 0, 5, 1);
        break;
    case ITEM_HEAD_PLUS:
        BOX(-3, 0, 7, 1);
        BOX(0, -3, 1, 7);
        break;
    case ITEM_HEAD_RING:
        BOX(-3, -3, 7, 1);
        BOX(-3, 3, 7, 1);
        BOX(-3, -2, 1, 5);
        BOX(3, -2, 1, 5);
        break;
    case ITEM_HEAD_STAR:
        BOX(-2, 0, 5, 1);
        BOX(0, -2, 1, 5);
        BOX(-2, -2, 1, 1);
        BOX(2, -2, 1, 1);
        BOX(-2, 2, 1, 1);
        BOX(2, 2, 1, 1);
        break;
    case ITEM_HEAD_SQUARE:
        BOX(-2, -2, 5, 5);
        break;
    default:
        break;
    }
#undef BOX
}

void shopDrawHeadIcon(SDL_Surface *s, unsigned char style, int cx, int cy,
                      int scale, SDL_Color color)
{
    if (style > 0)
        drawHead(s, style - 1, cx, cy, scale, color);
}

void shopDrawHeads(SDL_Surface *s, SDL_Rect at)
{
    for (int k = 0; k < nPlayers; ++k) {
        const struct player *p = &players[k];
        if (p->headStyle == 0 || !p->alive)
            continue;
        drawHead(s, p->headStyle - 1, (int)p->posx + at.x, (int)p->posy + at.y,
                 1, uiMix(colors[p->color], (SDL_Color){255, 255, 255, 0}, 60));
    }
}

/* ---- The coins ---- */

static unsigned int coins = 0;
static bool coinsKnown = false;     /* False: no coin data on this system */
static int testBalance = -1;        /* The coins of a console without any */

static void refreshCoins(void)
{
    if (testCoins >= 0) {
        if (testBalance < 0)
            testBalance = testCoins;
        coins = testBalance;
        coinsKnown = true;
        return;
    }
    coins = 0;
    coinsKnown = platformCoinsRead(&coins);
}

static bool spendCoins(unsigned int amount)
{
    if (testCoins >= 0) {
        testBalance -= amount;
        return true;
    }
    return platformCoinsSpend(amount);
}

/* ---- What is owned and in use ---- */

static bool isOwned(int item)
{
    return (shopOwned >> item) & 1;
}

/* The setting that holds what is in use for a kind of item. */
static int *inUse(enum itemKind kind)
{
    return kind == KIND_THEME ? &shopTheme
         : kind == KIND_PATTERN ? &shopPattern
         : kind == KIND_FRAME ? &shopFrame : &shopBlast;
}

/* Colors and heads are chosen per player, in the lineup, not here. */
static bool isGlobal(enum itemKind kind)
{
    return kind != KIND_COLOR && kind != KIND_HEAD;
}

/* The cards of a kind: for what is global, "normal" first, then the items in
 * the order above. */
static int cardCount(enum itemKind kind)
{
    int n = isGlobal(kind);
    for (int i = 0; i < N_ITEMS; ++i)
        n += items[i].kind == kind;
    return n;
}

/* The item on a card, -1 for the "normal" card. */
static int cardItem(enum itemKind kind, int card)
{
    if (isGlobal(kind) && card-- == 0)
        return -1;
    for (int i = 0; i < N_ITEMS; ++i) {
        if (items[i].kind == kind && card-- == 0)
            return i;
    }
    return -1;
}

static bool cardEquipped(enum itemKind kind, int card)
{
    return isGlobal(kind) && *inUse(kind) == cardItem(kind, card);
}

/* ---- The shop ---- */

#define CARD_COLS   4
#define TOUCH_TAB   1000        /* Touch ids from here on are the tabs */

static const char *const tabName[N_KINDS] = {"FIELD", "BG", "FRAME", "CRASH",
                                             "TRAIL", "HEAD"};

static enum itemKind tab = KIND_THEME;
static int sel = 0;                     /* The card in the tab */
static bool onTabs = false;             /* The cursor is on the tabs */
static bool confirming = false;         /* Press A once more to buy */
static const char *message = NULL;
static bool messageGood = false;

static void showMessage(const char *text, bool good)
{
    message = text;
    messageGood = good;
    playSound(good ? SOUND_BEEP : SOUND_BEP, sound);
}

static void useCard(enum itemKind kind, int card)
{
    *inUse(kind) = cardItem(kind, card);
    saveSettings();
    playSound(SOUND_BEEP, sound);
}

/* Buys an item and puts it to use. The item is saved before the coins go: the
 * coins are a system file and cannot be given back. */
static void buyItem(int item)
{
    shopOwned |= 1ull << item;
    if (!saveSettings()) {
        shopOwned &= ~(1ull << item);
        showMessage("COULD NOT SAVE ON THE SD CARD", false);
        return;
    }
    if (!spendCoins(items[item].price)) {
        shopOwned &= ~(1ull << item);
        saveSettings();
        refreshCoins();
        showMessage("COULD NOT TAKE THE COINS", false);
        return;
    }
    refreshCoins();
    if (isGlobal(items[item].kind)) {
        *inUse(items[item].kind) = item;
        saveSettings();
        showMessage("BOUGHT! IT IS IN USE", true);
    } else {
        showMessage("BOUGHT! PICK IT WHEN YOU SET UP A PLAYER", true);
    }
}

static int logicShop(void)
{
    int count = cardCount(tab);

    if (enterButtonDown()) {
        clearEnterButtons();
        message = NULL;
        if (onTabs) {
            onTabs = false;
            return 1;
        }
        int item = cardItem(tab, sel);
        if (item < 0 || isOwned(item)) {
            confirming = false;
            if (isGlobal(tab))
                useCard(tab, sel);
            else
                showMessage("PICK IT WHEN YOU SET UP A PLAYER", true);
        } else if (!coinsKnown) {
            showMessage("NO COIN DATA ON THIS SYSTEM", false);
        } else if (coins < (unsigned)items[item].price) {
            showMessage("NOT ENOUGH PLAY COINS", false);
        } else if (!confirming) {
            confirming = true;
            playSound(SOUND_BEEP, sound);
        } else {
            confirming = false;
            buyItem(item);
        }
        return 1;
    }

    enum itemKind newTab = tab;
    int newSel = sel;
    bool newOnTabs = onTabs;

    if (onTabs) {
        if (menuButtonQuery(KEY_LEFT) && tab > 0)
            newTab = tab - 1;
        else if (menuButtonQuery(KEY_RIGHT) && tab < N_KINDS - 1)
            newTab = tab + 1;
        else if (menuButtonQuery(KEY_DOWN))
            newOnTabs = false;
    } else if (menuButtonQuery(KEY_LEFT)) {
        if (sel > 0)
            newSel = sel - 1;
    } else if (menuButtonQuery(KEY_RIGHT)) {
        if (sel < count - 1)
            newSel = sel + 1;
    } else if (menuButtonQuery(KEY_UP)) {
        if (sel < CARD_COLS)
            newOnTabs = true;
        else
            newSel = sel - CARD_COLS;
    } else if (menuButtonQuery(KEY_DOWN)) {
        if (sel + CARD_COLS < count)
            newSel = sel + CARD_COLS;
        else if (sel / CARD_COLS < (count - 1) / CARD_COLS)
            newSel = count - 1;
    }

    if (newTab != tab || newSel != sel || newOnTabs != onTabs) {
        if (newTab != tab)
            newSel = 0;
        tab = newTab;
        sel = newSel;
        onTabs = newOnTabs;
        confirming = false;
        message = NULL;
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static SDL_Rect cardRect(int card)
{
    return (SDL_Rect){8 + (card % CARD_COLS) * 78, 42 + (card / CARD_COLS) * 36,
                      72, 32};
}

static SDL_Rect tabRect(int kind)
{
    return (SDL_Rect){4 + kind * 53, 8, 51, 24};
}

/* The top screen: the coins, and the selected card big. */
static void drawShopTop(void)
{
    useTopView();
    clearBackground(screen);

    int cx = screen->w / 2;
    SDL_Rect purse = uiCard(screen, (SDL_Rect){cx - 130, 10, 260, 40},
                            uiColPanel, uiColDim, false);
    uiText(screen, FONT_BROADCAST, "PLAY COINS", uiColDim, purse.x + 12,
           purse.y + 16, UI_LEFT);
    if (coinsKnown) {
        char number[16];
        snprintf(number, sizeof(number), "%u", coins);
        uiText(screen, FONT_MENU, number, uiColAccent, purse.x + purse.w - 12,
               purse.y + 8, UI_RIGHT);
    } else {
        uiText(screen, FONT_BROADCAST, "NO DATA", uiColRed,
               purse.x + purse.w - 12, purse.y + 16, UI_RIGHT);
    }

    int item = cardItem(tab, sel);
    SDL_Color bg = {BG_R, BG_G, BG_B, 0};
    SDL_Rect view = {cx - 56, 66, 112, 60};

    /* What it looks like. */
    if (tab == KIND_THEME) {
        uiCard(screen, view, item < 0 ? bg : itemColor(item), uiColDim, false);
    } else if (tab == KIND_PATTERN) {
        SDL_Color ground = themeInUse() ? itemColor(shopTheme) : bg;
        uiCard(screen, view, ground, uiColDim, false);
        if (item >= 0)
            drawPattern(screen, (SDL_Rect){view.x + 2, view.y + 2, view.w - 4,
                                           view.h - 4}, item, ground);
    } else if (tab == KIND_FRAME) {
        SDL_Color frame = item < 0 ? uiColText : itemColor(item);
        uiCard(screen, view, bg, frame, false);
    } else if (tab == KIND_COLOR) {
        /* A piece of trail, 3 pixels wide, in the color. */
        uiCard(screen, view, bg, uiColDim, false);
        SDL_Color c = itemColor(item);
        for (int k = 0; k < 100; ++k) {
            int x = view.x + 6 + k, y = view.y + 30 - (int)(sin(k / 14.0) * 14);
            SDL_Rect px = {x - 1, y - 1, 3, 3};
            SDL_FillRect(screen, &px, uiMap(screen, c));
        }
    } else if (tab == KIND_HEAD) {
        uiCard(screen, view, bg, uiColDim, false);
        if (item >= 0)
            drawHead(screen, item, cx, view.y + view.h / 2, 5, uiColText);
    } else {
        SDL_Color dot = item < 0 ? uiColDim : itemColor(item);
        for (int k = 0; k < 12; ++k) {
            double a = k * M_PI / 6;
            uiBall(screen, cx + (int)(cos(a) * 34), 96 + (int)(sin(a) * 26), 2,
                   2, dot);
        }
    }

    bool owned = item < 0 || isOwned(item);
    uiText(screen, FONT_MENU, item < 0 ? "NORMAL" : itemName(item),
           owned ? uiColText : uiColAccent, cx, 138, UI_CENTER);

    if (cardEquipped(tab, sel)) {
        uiText(screen, FONT_BROADCAST, "IN USE", uiColGreen, cx, 168, UI_CENTER);
    } else if (owned) {
        uiText(screen, FONT_BROADCAST, "OWNED", uiColDim, cx, 168, UI_CENTER);
    } else {
        char price[24];
        snprintf(price, sizeof(price), "%d COINS", items[item].price);
        uiText(screen, FONT_BROADCAST, price,
               coins >= (unsigned)items[item].price ? uiColAccent : uiColRed,
               cx, 168, UI_CENTER);
    }
    if (confirming)
        uiText(screen, FONT_BROADCAST, "PRESS A AGAIN TO BUY", uiColAccent, cx,
               190, UI_CENTER);
    if (message != NULL)
        uiText(screen, FONT_BROADCAST, message,
               messageGood ? uiColGreen : uiColRed, cx, 212, UI_CENTER);
}

static void displayShop(void)
{
    drawShopTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    for (int k = 0; k < N_KINDS; ++k) {
        SDL_Rect r = tabRect(k);
        touchAdd(r, TOUCH_TAB + k, TOUCH_ROW);
        bool current = k == (int)tab;
        SDL_Rect in = uiCard(screen, r, current ? uiColPanel : uiMix(uiColPanel,
                             (SDL_Color){0, 0, 0, 0}, 50),
                             current ? uiColAccent : uiColDim,
                             onTabs && current);
        uiText(screen, FONT_BROADCAST, tabName[k], current ? uiColText : uiColDim,
               in.x + in.w / 2, in.y + 8, UI_CENTER);
    }

    int count = cardCount(tab);
    for (int i = 0; i < count; ++i) {
        SDL_Rect r = cardRect(i);
        touchAdd(r, i, TOUCH_ROW);

        int item = cardItem(tab, i);
        bool owned = item < 0 || isOwned(item);
        bool equipped = cardEquipped(tab, i);
        bool selected = i == sel && !onTabs;
        SDL_Color fill = item >= 0 && tab == KIND_THEME ? itemColor(item)
                                                        : uiColPanel;
        SDL_Color border = equipped ? uiColGreen
                           : selected ? uiColAccent : uiColDim;
        SDL_Rect in = uiCard(screen, r, fill, border, selected);

        /* A strip in the color of the item. */
        if (item >= 0 && tab != KIND_THEME) {
            SDL_Rect strip = {in.x + 2, in.y + 2, in.w - 4, 4};
            SDL_FillRect(screen, &strip, uiMap(screen, itemColor(item)));
        }

        uiText(screen, FONT_BROADCAST, item < 0 ? "NORMAL" : itemName(item),
               owned ? uiColText : uiColDim, in.x + in.w / 2, in.y + 9,
               UI_CENTER);
        char sub[16];
        SDL_Color subColor = uiColDim;
        if (equipped) {
            snprintf(sub, sizeof(sub), "IN USE");
            subColor = uiColGreen;
        } else if (owned) {
            snprintf(sub, sizeof(sub), "OWNED");
        } else {
            snprintf(sub, sizeof(sub), "%d COINS", items[item].price);
            subColor = coins >= (unsigned)items[item].price ? uiColAccent
                                                            : uiColOff;
        }
        uiText(screen, FONT_BROADCAST, sub, subColor, in.x + in.w / 2,
               in.y + 20, UI_CENTER);
    }

    int item = cardItem(tab, sel);
    const char *action = onTabs ? "OK"
        : item < 0 || isOwned(item) ? (isGlobal(tab) ? "USE" : "OK")
        : confirming ? "BUY NOW" : "BUY";
    struct uiHint hints[] = {{"A", action, 7}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchShop(int id, int value, bool released)
{
    (void)value;
    if (id >= TOUCH_TAB) {
        if (!released && tab != (enum itemKind)(id - TOUCH_TAB)) {
            tab = id - TOUCH_TAB;
            sel = 0;
            onTabs = true;
            confirming = false;
            message = NULL;
        }
    } else if (!released) {
        if (sel != id)
            confirming = false;
        sel = id;
        onTabs = false;
    } else {
        touchPressA();
    }
}

/* ---- Setup ---- */

static struct scene shopScene;

void shopEnter(struct scene *parent)
{
    shopScene = (struct scene){logicShop, displayShop, parent, touchShop,
                               NULL, NULL};
    message = NULL;
    confirming = false;
    onTabs = false;
    sel = 0;
    refreshCoins();
    curScene = &shopScene;
}

#endif /* __3DS__ */
