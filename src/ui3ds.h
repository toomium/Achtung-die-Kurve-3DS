/* ui3ds -- drawing blocks for the 3DS menus: tiles, hint bar, text, icons.
 *
 * Everything is made of rectangles with stepped corners, to fit the pixel
 * font.
 */
#ifndef UI3DS_H
#define UI3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <SDL/SDL.h>

#include "font.h"

typedef enum {
    UI_LEFT,
    UI_CENTER,
    UI_RIGHT
} UiAlign;

/* A key and what it does, for the hint bar at the bottom of a menu. */
struct uiHint {
    const char *key;
    const char *label;
    int reserve;        /* Reserves room for this many characters of the label,
                           so the hints to the right of it stay in place. 0:
                           just as long as the label. */
};

/* An 8x8 pixel icon: 8 strings of 8 characters, one per row ('#' is set). */
typedef const char *const *UiIcon;

extern const char *const uiIconPlay[8];
extern const char *const uiIconOptions[8];
extern const char *const uiIconExit[8];
extern const char *const uiIconOneConsole[8];
extern const char *const uiIconManyConsoles[8];
extern const char *const uiIconRestart[8];
extern const char *const uiIconSound[8];
extern const char *const uiIconHome[8];
extern const char *const uiIconShop[8];
extern const char *const uiIconLock[8];
extern const char *const uiIconStar[8];
extern const char *const uiIconTrophy[8];

/* The palette. */
/* Meaning of the colors: green go/confirm, blue settings,
 * red leave, yellow highlight, gray unavailable. */
extern const SDL_Color uiColGreen;
extern const SDL_Color uiColBlue;
extern const SDL_Color uiColRed;
extern const SDL_Color uiColText;
extern const SDL_Color uiColDim;
extern const SDL_Color uiColAccent;
extern const SDL_Color uiColOff;

Uint32 uiMap(SDL_Surface *s, SDL_Color c);

/* Mixes two colors: t = 0 gives a, t = 100 gives b. */
SDL_Color uiMix(SDL_Color a, SDL_Color b, int t);

/* The fill color of tiles and cards. */
extern const SDL_Color uiColPanel;

/* A filled panel with a border and stepped corners. */
void uiPanel(SDL_Surface *s, SDL_Rect r, SDL_Color fill, SDL_Color border);

/*
 * A button-like tile in the color `theme`: border, icon and text use it, strongly
 * when the tile is selected (it is lifted and tinted), dimmed otherwise. A
 * disabled tile is gray. `icon` and `sub` may be NULL. A wide tile (w > 200) puts
 * the icon left of the label, a narrow one above it.
 */
void uiTile(SDL_Surface *s, SDL_Rect r, const char *label, const char *sub,
            UiIcon icon, SDL_Color theme, bool selected, bool enabled);

/*
 * A card: a panel with a shadow, lifted up and left when `lifted`. Returns the
 * rectangle that was drawn, to place the content in.
 */
SDL_Rect uiCard(SDL_Surface *s, SDL_Rect r, SDL_Color fill, SDL_Color border,
                bool lifted);

/* A filled circle of pixels: radius and scale give the size (2 * radius * scale). */
void uiBall(SDL_Surface *s, int cx, int cy, int radius, int scale,
            SDL_Color color);

void uiText(SDL_Surface *s, Font font, const char *text, SDL_Color color,
            int x, int y, UiAlign align);

void uiIcon(SDL_Surface *s, UiIcon icon, int x, int y, int scale,
            SDL_Color color);

/* A digit (0-9) in pixel font of 5x7 pixels, centered on (cx, cy). */
void uiDigit(SDL_Surface *s, int digit, int cx, int cy, int scale,
             SDL_Color color);

void uiHintBar(SDL_Surface *s, const struct uiHint *hints, int n);

#endif /* __3DS__ */

#endif
