/* items3ds -- the item mode: weapons lie on the field and are picked up by
 * driving over them, instead of being chosen before the game.
 */
#ifndef ITEMS3DS_H
#define ITEMS3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <SDL/SDL.h>

#include "weapon.h"

#define MAX_ITEMS       3
#define ITEM_RADIUS     9       /* How close a head must come, in pixels */

/* Whether the weapons of the game are items on the field (a rule). */
extern bool itemMode;

/* A new round: no items, the first one comes soon. */
void itemsReset(void);

/*
 * Lets the items age and appear, and gives them to the players that drive over
 * them. A player only takes one if he has no weapon left. Returns true if
 * somebody took one (the score page has to be drawn again). (mx, my) is how far
 * the walls have closed in: nothing appears there, and what is there goes.
 */
bool itemsUpdate(Uint32 delta, const unsigned char *hitmap, unsigned w,
                 unsigned h, int mx, int my);

/* Draws the items over the playfield; `icons` are the small weapon icons and
 * `at` is the shake of the field. */
void itemsDraw(SDL_Surface *s, SDL_Rect at, SDL_Surface *const icons[N_WEAPONS]);

/* The nearest item within `range` pixels of (x, y). */
bool itemsNearest(double x, double y, double range, int *tx, int *ty);

/* For the test: how many items there are and where the first one is. */
int itemsCount(void);
bool itemsGet(int i, int *x, int *y, int *type);

#endif

#endif
