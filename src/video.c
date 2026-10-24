/* video -- window and screen handling.
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

#include "video.h"

#ifdef __3DS__
#include "platform.h"
#endif

SDL_Surface *screen = NULL;

#ifdef __3DS__
/* The real display: one tall surface, upper half on the top screen, lower half
 * on the bottom screen (SDL_DUALSCR). The game draws into the two views, which
 * use the display's pixels. */
static SDL_Surface *display = NULL;
static SDL_Surface *topView = NULL;
SDL_Surface *bottomView = NULL;
#endif

SDL_Surface *gameScreen = NULL;

unsigned int WINDOW_W = DEFAULT_WINDOW_W;

unsigned int WINDOW_H = DEFAULT_WINDOW_H;

bool screenFreeze = false;

#ifdef __3DS__
/**
 * Open the dual screen display and create the two views, in the display's
 * pixel format. `screen` is left pointing at the top view.
 *
 * @return 1 if successful, 0 if not.
 */
static int initViews(void)
{
    SDL_FreeSurface(topView);
    SDL_FreeSurface(bottomView);
    topView = bottomView = NULL;

    platformTrace("initViews start");
    display = SDL_SetVideoMode(WINDOW_W, WINDOW_H * 2, SCREEN_BPP,
                               SDL_SWSURFACE | SDL_DUALSCR);
    platformTrace("SDL_SetVideoMode");
    if (display == NULL)
        return 0;

    /* The views are windows into the display's pixels, so nothing has to be
     * copied when a frame is presented. The rows of the display are longer
     * than the screen (its pitch), the views must keep that. */
    SDL_PixelFormat *f = display->format;
    Uint8 *pixels = display->pixels;
    topView = SDL_CreateRGBSurfaceFrom(pixels, WINDOW_W, WINDOW_H,
                                       f->BitsPerPixel, display->pitch,
                                       f->Rmask, f->Gmask, f->Bmask, f->Amask);
    bottomView = SDL_CreateRGBSurfaceFrom(
        pixels + WINDOW_H * display->pitch + BOTTOM_VIEW_X * f->BytesPerPixel,
        BOTTOM_VIEW_W, BOTTOM_VIEW_H, f->BitsPerPixel, display->pitch,
        f->Rmask, f->Gmask, f->Bmask, f->Amask);
    screen = topView;
    platformTrace("views created");

    return (topView == NULL || bottomView == NULL) ? 0 : 1;
}
#endif

/**
 * Initialize the application window and main screen surfaces.
 *
 * @return 1 if the initialization was successful, 0 if not.
 */
int initScreen(void)
{
#ifdef __3DS__
    if (!initViews())
        return 0;
#else
    SDL_FreeSurface(screen);
    screen = SDL_SetVideoMode(WINDOW_W, WINDOW_H, SCREEN_BPP,
                              SDL_SWSURFACE | (fullscreen
                                               ? SDL_FULLSCREEN
                                               : SDL_RESIZABLE));
#endif

    SDL_FreeSurface(gameScreen);
    gameScreen = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                      screen->w,
                                      screen->h,
                                      screen->format->BitsPerPixel,
                                      screen->format->Rmask,
                                      screen->format->Gmask,
                                      screen->format->Bmask,
                                      0);
#ifdef __3DS__
    platformTrace("gameScreen created");
#endif
    SDL_SetColorKey(gameScreen, SDL_SRCCOLORKEY, 0);

    initParticleScreen(WINDOW_W, WINDOW_H);
#ifdef __3DS__
    platformTrace("particleScreen");
#endif

#ifdef __3DS__
    useBottomView();
#endif

    return (screen == NULL || gameScreen == NULL) ? 0 : 1;
}

int presentFrame(void)
{
#ifdef __3DS__
    /* The views already are the display's pixels. */
    return SDL_Flip(display);
#else
    return SDL_Flip(screen);
#endif
}

#ifdef __3DS__
void useTopView(void)
{
    screen = topView;
}

void useBottomView(void)
{
    screen = bottomView;
}
#endif
