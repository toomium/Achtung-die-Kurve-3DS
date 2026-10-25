/* shop3ds -- the shop: Play Coins buy cosmetic items.
 *
 * The coins are the real Play Coins of the console. What has been bought lives in
 * settings.cfg (shopOwned and what is in use, see config.h).
 */
#ifndef SHOP3DS_H
#define SHOP3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <SDL/SDL.h>

#include "scene.h"

/* Opens the shop. `parent` is the page that B leads back to. */
void shopEnter(struct scene *parent);

/* Fills the playfield with the background of the chosen theme. */
void shopClearPlayfield(SDL_Surface *s);

/* The color of the frame chosen in the shop. False: the normal frame. */
bool shopFrameColor(SDL_Color *color);

/* Whether this console owns the color of the palette: the first eight come
 * with the game, the others are bought in the shop. */
bool shopColorOwned(int color);

/*
 * Head markers are chosen per player when the players are set up. A style is
 * the item's index plus one, 0 is no marker.
 *
 * shopCycleHead() gives the next (dir 1) or previous (dir -1) one the player
 * owns, with "none" in the cycle. shopOwnedHead() gives `style` if it is owned,
 * otherwise 0. shopOwnedHeads() lists the owned ones, 0 first. shopHeadName() is
 * for the lineup.
 */
unsigned char shopCycleHead(unsigned char style, int dir);
unsigned char shopOwnedHead(unsigned char style);
int shopOwnedHeads(unsigned char *list, int max);
const char *shopHeadName(unsigned char style);

/* A head marker as an icon, scaled by `scale`, centered on (cx, cy). */
void shopDrawHeadIcon(SDL_Surface *s, unsigned char style, int cx, int cy,
                      int scale, SDL_Color color);

/* Draws the head markers of the players that are alive; `at` is the shake. */
void shopDrawHeads(SDL_Surface *s, SDL_Rect at);

/*
 * The particles of a crash at (x, y), where the player was heading `dir`, in
 * the player's color: the chosen crash effect.
 */
void shopCrashBurst(double x, double y, double dir, SDL_Color color);

#endif

#endif
