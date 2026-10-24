#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>

#include <SDL/SDL.h>

#include "common.h"
#include "config.h"
#include "particle.h"

/*
 * The main framebuffer surface.
 */
extern SDL_Surface *screen;

/*
 * Surface holding graphics that don't need to change every frame
 * during a game round (i.e. everything except particles).
 */
extern SDL_Surface *gameScreen;

/*
 * Current window width and height.
 */
extern unsigned int WINDOW_W;
extern unsigned int WINDOW_H;

/*
 * True to temporarily freeze what's currently being displayed.
 */
extern bool screenFreeze;

int initScreen(void);

/*
 * Show the finished frame. On the 3DS the game draws into two views (top and
 * bottom screen) which are combined here; elsewhere it is SDL_Flip(screen).
 * Returns -1 on error.
 */
int presentFrame(void);

/*
 * Select the surface that `screen` points to, i.e. the one the current scene
 * draws into. Only meaningful on the 3DS.
 */
#ifdef __3DS__
/* The bottom screen: menus, scores and messages. */
extern SDL_Surface *bottomView;

void useTopView(void);
void useBottomView(void);
#else
#define useTopView() ((void)0)
#define useBottomView() ((void)0)
#endif

#endif
