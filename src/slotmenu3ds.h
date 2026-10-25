/* slotmenu3ds -- the pages under a slot of the lineup: trail, head and
 * controls. B leads back to the lineup, which shows the change at once.
 */
#ifndef SLOTMENU3DS_H
#define SLOTMENU3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <SDL/SDL.h>

#include "scene.h"

enum { SLOTMENU_TRAIL, SLOTMENU_HEAD, SLOTMENU_CONTROLS };

/* Sets up the pages; `lineup` is the page that B leads back to. */
void slotMenusInit(struct scene *lineup);

/* Opens a page for slot `slot` (an index into players[]). */
void slotMenuOpen(int menu, int slot);

/* ---- In zatackax.c ---- */

/* The eight slots on the top screen. */
void drawLobbyTop(void);

/* Whether another slot that is in the game has this color. */
bool slotColorTaken(int slot, int color);

/* Changes the color of a slot. A name that is still the color's name follows. */
void setSlotColor(int slot, int color);

const char *colorName(int color);
bool nameIsDefault(const char *name);

/* A row of the lineup and the buttons, see zatackax.c. */
SDL_Rect lobbyRowPanel(int y, bool focused, const char *label);
void setNextKey(unsigned char pedit, unsigned char key);

#endif

#endif
