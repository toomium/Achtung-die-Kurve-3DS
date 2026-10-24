/* font -- font file loading.
 * Copyright (C) 2010-2021 Simen Heggestøyl <simenheg@runbox.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "font.h"

TTF_Font *fonts[N_FONTS];

/**
 * Load a font file named `filename` and identify it as `f` with size
 * `size`. Return true on success, otherwise false.
 */
bool loadFont(Font f, const char *filename, FontSize size)
{
    char *file = dataFile("fonts", filename);
    TTF_Font *loadedFont = TTF_OpenFont(file, size);

    if (loadedFont == NULL) {
        fileNotFound(file);
        free(file);
        return false;
    }

    fonts[f] = loadedFont;
    free(file);
    return true;
}

/**
 * Close every font. Needed on the 3DS before the romfs is unmounted: an open
 * font keeps its file open.
 */
void closeFonts(void)
{
    for (int i = 0; i < N_FONTS; ++i) {
        if (fonts[i] != NULL) {
            TTF_CloseFont(fonts[i]);
            fonts[i] = NULL;
        }
    }
}

/**
 * Load every needed font file. Return true if every file loaded
 * successfully, otherwise false.
 */
bool loadFonts(void)
{
#ifdef __3DS__
    /* One pixel font for everything (3ds/assets/fonts). */
    const char *pixel = "PressStart2P-Regular.ttf";
    return loadFont(FONT_MENU, pixel, FONT_SIZE_MENU)
        && loadFont(FONT_MENU_BOLD, pixel, FONT_SIZE_MENU)
        && loadFont(FONT_BROADCAST, pixel, FONT_SIZE_BROADCAST)
        && loadFont(FONT_BROADCAST_BOLD, pixel, FONT_SIZE_BROADCAST)
        && loadFont(FONT_SCORE, pixel, FONT_SIZE_SCORE);
#else
    return loadFont(FONT_MENU, "jura/JuraLight.ttf", FONT_SIZE_MENU)
        && loadFont(FONT_MENU_BOLD, "jura/JuraMedium.ttf", FONT_SIZE_MENU)
        && loadFont(FONT_BROADCAST, "ankacoder/AnkaCoder-r.ttf", FONT_SIZE_BROADCAST)
        && loadFont(FONT_BROADCAST_BOLD, "ankacoder/AnkaCoder-b.ttf", FONT_SIZE_BROADCAST)
        && loadFont(FONT_SCORE, "jura/JuraMedium.ttf", FONT_SIZE_SCORE);
#endif
}
