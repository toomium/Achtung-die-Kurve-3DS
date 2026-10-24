/* touch3ds -- the touch targets of the page on the bottom screen.
 *
 * Every page lists its targets (rectangle, meaning) while it draws. The
 * coordinates are those of the bottom screen, 320x240.
 */
#ifndef TOUCH3DS_H
#define TOUCH3DS_H

#ifdef __3DS__

#include <SDL/SDL.h>

/* The id of a target in the hint bar: `value` is the button number it presses
 * (0 START, 1 A, 2 B). Pages use other ids for their own targets. */
#define TOUCH_HINT      -1

/* "Just select this row" (value of a target that has no button of its own). */
#define TOUCH_ROW       -9

/* Targets that several pages have: previous and next, and GO. */
#define TOUCH_PREV      100
#define TOUCH_NEXT      101
#define TOUCH_GO        200

struct touchTarget {
    SDL_Rect rect;
    int id;
    int value;
};

void touchReset(void);
void touchAdd(SDL_Rect rect, int id, int value);

/* The target at (x, y), or NULL. The first one added wins, so add the small
 * targets before the row they lie in. */
const struct touchTarget *touchFind(int x, int y);

#endif /* __3DS__ */

#endif
