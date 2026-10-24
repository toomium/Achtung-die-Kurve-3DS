#ifdef __3DS__

#include "ui3ds.h"

#include <string.h>

#include <SDL/SDL_ttf.h>

#include "common.h"
#include "touch3ds.h"

const SDL_Color uiColGreen   = {0x5F, 0xD7, 0x78, 0};
const SDL_Color uiColBlue    = {0x5F, 0xAE, 0xFF, 0};
const SDL_Color uiColRed     = {0xFF, 0x6B, 0x6B, 0};
const SDL_Color uiColText    = {0xE8, 0xEC, 0xFF, 0};
const SDL_Color uiColDim = {0x8F, 0x9B, 0xD6, 0};
const SDL_Color uiColAccent  = {0xFF, 0xC8, 0x57, 0};
const SDL_Color uiColOff = {0x5C, 0x66, 0x96, 0};

static const SDL_Color tileFill     = {0x38, 0x3A, 0x48, 0};
const SDL_Color uiColPanel = {0x2B, 0x37, 0x6E, 0};
static const SDL_Color tileFillOff  = {0x24, 0x25, 0x2C, 0};
static const SDL_Color tileBorder   = {0x4E, 0x62, 0xB8, 0};
static const SDL_Color tileBorderOff= {0x33, 0x3D, 0x72, 0};
static const SDL_Color shadow       = {0x12, 0x16, 0x2E, 0};
static const SDL_Color barFill      = {0x12, 0x16, 0x2E, 0};
static const SDL_Color darkText     = {0x12, 0x16, 0x2E, 0};

const char *const uiIconPlay[8] = {
    "..#.....",
    "..###...",
    "..#####.",
    "..######",
    "..#####.",
    "..###...",
    "..#.....",
    "........"
};

const char *const uiIconOptions[8] = {
    "...##...",
    ".######.",
    ".##..##.",
    "##....##",
    "##....##",
    ".##..##.",
    ".######.",
    "...##..."
};

const char *const uiIconExit[8] = {
    "##....##",
    "###..###",
    ".######.",
    "..####..",
    "..####..",
    ".######.",
    "###..###",
    "##....##"
};

const char *const uiIconOneConsole[8] = {
    ".######.",
    "#......#",
    "#.####.#",
    "#.#..#.#",
    "#.####.#",
    "#......#",
    ".######.",
    "........"
};

const char *const uiIconManyConsoles[8] = {
    "#####...",
    "#...#...",
    "#.#.####",
    "#...#..#",
    "#####..#",
    "...#...#",
    "...#####",
    "........"
};

SDL_Color uiMix(SDL_Color a, SDL_Color b, int t)
{
    SDL_Color c = {
        (a.r * (100 - t) + b.r * t) / 100,
        (a.g * (100 - t) + b.g * t) / 100,
        (a.b * (100 - t) + b.b * t) / 100,
        0
    };
    return c;
}

const char *const uiIconRestart[8] = {
    "..####..",
    ".#....#.",
    "#....###",
    "#.....#.",
    "#.......",
    ".#.....#",
    "..#####.",
    "........"
};

const char *const uiIconSound[8] = {
    "...#....",
    "..##..#.",
    ".###...#",
    "####.#.#",
    "####.#.#",
    ".###...#",
    "..##..#.",
    "...#...."
};

const char *const uiIconHome[8] = {
    "...##...",
    "..####..",
    ".######.",
    "########",
    ".#....#.",
    ".#.##.#.",
    ".#.##.#.",
    ".######."
};

static const char *const digitRows[10][7] = {
    {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."},
    {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
    {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"},
    {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."},
    {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."},
    {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."},
    {".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."},
    {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."},
    {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."},
    {".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."}
};

void uiDigit(SDL_Surface *s, int digit, int cx, int cy, int scale,
             SDL_Color color)
{
    if (digit < 0 || digit > 9)
        return;

    Uint32 c = SDL_MapRGB(s->format, color.r, color.g, color.b);
    int x0 = cx - 5 * scale / 2;
    int y0 = cy - 7 * scale / 2;

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (digitRows[digit][row][col] == '#') {
                SDL_Rect px = {x0 + col * scale, y0 + row * scale, scale, scale};
                SDL_FillRect(s, &px, c);
            }
        }
    }
}

Uint32 uiMap(SDL_Surface *s, SDL_Color c)
{
    return SDL_MapRGB(s->format, c.r, c.g, c.b);
}

void uiPanel(SDL_Surface *s, SDL_Rect r, SDL_Color fill, SDL_Color border)
{
    SDL_Rect inner = {r.x + 2, r.y + 2, r.w - 4, r.h - 4};
    SDL_FillRect(s, &r, uiMap(s, border));
    SDL_FillRect(s, &inner, uiMap(s, fill));

    /* Stepped corners: take the corner pixels back to the background. */
    Uint32 bg = SDL_MapRGB(s->format, BG_R, BG_G, BG_B);
    SDL_Rect corners[4] = {
        {r.x, r.y, 2, 2},
        {r.x + r.w - 2, r.y, 2, 2},
        {r.x, r.y + r.h - 2, 2, 2},
        {r.x + r.w - 2, r.y + r.h - 2, 2, 2}
    };
    for (int i = 0; i < 4; ++i)
        SDL_FillRect(s, &corners[i], bg);
}

SDL_Rect uiCard(SDL_Surface *s, SDL_Rect r, SDL_Color fill, SDL_Color border,
                bool lifted)
{
    SDL_Rect shade = {r.x + 4, r.y + 4, r.w, r.h};
    uiPanel(s, shade, shadow, shadow);

    if (lifted) {
        r.x -= 2;
        r.y -= 2;
    }
    uiPanel(s, r, fill, border);
    return r;
}

void uiBall(SDL_Surface *s, int cx, int cy, int radius, int scale,
            SDL_Color color)
{
    Uint32 c = uiMap(s, color);

    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= radius * radius + radius)
            ++dx;
        SDL_Rect row = {cx - dx * scale, cy + dy * scale, (2 * dx + 1) * scale,
                        scale};
        SDL_FillRect(s, &row, c);
    }
}

void uiText(SDL_Surface *s, Font font, const char *text, SDL_Color color,
            int x, int y, UiAlign align)
{
    if (text == NULL || text[0] == '\0')
        return;

    SDL_Surface *rendered = TTF_RenderUTF8_Blended(fonts[font], text, color);
    if (rendered == NULL)
        return;

    SDL_Rect offset = {x, y, 0, 0};
    if (align == UI_CENTER)
        offset.x -= rendered->w / 2;
    else if (align == UI_RIGHT)
        offset.x -= rendered->w;

    SDL_BlitSurface(rendered, NULL, s, &offset);
    SDL_FreeSurface(rendered);
}

void uiIcon(SDL_Surface *s, UiIcon icon, int x, int y, int scale,
            SDL_Color color)
{
    Uint32 c = uiMap(s, color);

    for (int row = 0; row < 8; ++row) {
        const char *line = icon[row];
        for (int col = 0; col < 8 && line[col] != '\0'; ++col) {
            if (line[col] == '#') {
                SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                SDL_FillRect(s, &px, c);
            }
        }
    }
}

void uiTile(SDL_Surface *s, SDL_Rect r, const char *label, const char *sub,
            UiIcon icon, SDL_Color theme, bool selected, bool enabled)
{
    /* The shadow stays, a selected tile moves up and left. */
    SDL_Rect shade = {r.x + 4, r.y + 4, r.w, r.h};
    uiPanel(s, shade, shadow, shadow);

    if (selected) {
        r.x -= 2;
        r.y -= 2;
    }

    SDL_Color fill, border, text, iconColor;
    if (!enabled) {
        fill = tileFillOff;
        border = tileBorderOff;
        text = iconColor = uiColOff;
    } else if (selected) {
        fill = uiMix(tileFill, theme, 22);
        border = theme;
        text = iconColor = theme;
    } else {
        fill = tileFill;
        border = uiMix(tileFill, theme, 55);
        text = iconColor = uiMix(tileFill, theme, 75);
    }

    uiPanel(s, r, fill, border);

    int labelH = 16;
    int subH = sub != NULL ? 8 + 6 : 0;

    if (r.w > 200) {
        /* Wide: icon left, label (and the line below it) right of it. */
        int textBlock = labelH + subH;
        int cx = r.x + r.w / 2 + (icon != NULL ? 24 : 0);
        int ty = r.y + (r.h - textBlock) / 2;
        if (icon != NULL) {
            /* A low tile gets a smaller icon. */
            int scale = r.h >= 56 ? 5 : 3;
            uiIcon(s, icon, r.x + 24, r.y + (r.h - 8 * scale) / 2, scale,
                   iconColor);
        }
        uiText(s, FONT_MENU, label, text, cx, ty, UI_CENTER);
        if (sub != NULL)
            uiText(s, FONT_BROADCAST, sub, text, cx, ty + labelH + 6,
                   UI_CENTER);
    } else {
        /* Narrow: icon above, label below. */
        int top = r.y + 8;
        if (icon != NULL) {
            uiIcon(s, icon, r.x + (r.w - 24) / 2, top, 3, iconColor);
            top += 24 + 6;
        }
        uiText(s, FONT_MENU, label, text, r.x + r.w / 2, top, UI_CENTER);
        if (sub != NULL)
            uiText(s, FONT_BROADCAST, sub, text, r.x + r.w / 2,
                   top + labelH + 4, UI_CENTER);
    }
}

void uiHintBar(SDL_Surface *s, const struct uiHint *hints, int n)
{
    const int h = 20;
    SDL_Rect bar = {0, s->h - h, s->w, h};
    SDL_FillRect(s, &bar, uiMap(s, barFill));

    int x = 10;
    int y = bar.y + 4;
    for (int i = 0; i < n; ++i) {
        /* The key as a small cap, then what it does. */
        SDL_Rect cap = {x, y - 1, 8 * (int)strlen(hints[i].key) + 6, 14};
        uiPanel(s, cap, uiColAccent, uiColAccent);
        uiText(s, FONT_BROADCAST, hints[i].key, darkText, cap.x + 3, y + 2,
               UI_LEFT);
        int start = cap.x;
        x = cap.x + cap.w + 6;
        uiText(s, FONT_BROADCAST, hints[i].label, uiColDim, x, y + 2, UI_LEFT);
        int chars = (int)strlen(hints[i].label);
        if (chars < hints[i].reserve)
            chars = hints[i].reserve;
        x += 8 * chars + 16;

        /* The keys can be touched: START 0, A 1, B 2. */
        int button = strcmp(hints[i].key, "START") == 0 ? 0
            : strcmp(hints[i].key, "A") == 0 ? 1
            : strcmp(hints[i].key, "B") == 0 ? 2 : -1;
        if (button >= 0) {
            SDL_Rect area = {start - 4, bar.y, x - start - 8, h};
            touchAdd(area, TOUCH_HINT, button);
        }
    }
}

#endif /* __3DS__ */
