/* demo3ds -- the animation behind the logo: balls that drive around and leave
 * a trail, more and more of them.
 */
#ifndef DEMO3DS_H
#define DEMO3DS_H

#ifdef __3DS__

#include <SDL/SDL.h>

/* Starts over with one ball. */
void demoReset(void);

/* Moves the balls by the time since the last call and lets new ones in. */
void demoUpdate(void);

/* Draws the balls and their trails onto `s` (which is cleared by the caller). */
void demoDraw(SDL_Surface *s);

#endif /* __3DS__ */

#endif
