#ifndef FONT_H
#define FONT_H

#include <SDL/SDL_ttf.h>
#include <stdbool.h>

#include "data.h"
#include "error.h"

typedef enum font {
    FONT_MENU = 0,
    FONT_MENU_BOLD,
    FONT_BROADCAST,
    FONT_BROADCAST_BOLD,
    FONT_SCORE,
    N_FONTS
} Font;

typedef enum font_size {
#ifdef __3DS__
    /* Press Start 2P is a pixel font, it is sharp at multiples of 8. */
    FONT_SIZE_BROADCAST = 8,
    FONT_SIZE_MENU      = 16,
    FONT_SIZE_SCORE     = 16
#else
    FONT_SIZE_BROADCAST = 13,
    FONT_SIZE_MENU      = 22,
    FONT_SIZE_SCORE     = 22
#endif
} FontSize;

extern TTF_Font *fonts[N_FONTS];

bool loadFont(Font f, const char *filename, FontSize size);
bool loadFonts(void);
void closeFonts(void);

#endif
