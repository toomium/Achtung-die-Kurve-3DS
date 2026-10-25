/* tournament3ds -- a knock-out tournament on one console: the players of the
 * lineup meet two by two in duels, the winners go on, the last one is the
 * champion. With an odd number somebody gets a bye.
 */
#ifndef TOURNAMENT3DS_H
#define TOURNAMENT3DS_H

#ifdef __3DS__

#include <stdbool.h>

#include "bracket3ds.h"
#include "scene.h"

/* ---- On the screen, in zatackax.c and here ---- */

extern struct scene tourScene;

/* The lineup is final: begins the tournament. Returns false (with a sound) if it
 * cannot, for example with fewer than three players. */
bool tournamentBegin(void);

/* True from tournamentBegin() until the tournament is left. */
bool tournamentActive(void);

/* A duel is over; `winner` is the slot of the winner. Shows the bracket. */
void tournamentDuelOver(int winner);

/* The tournament ends, whatever the state; the rules of the lineup come back. */
void tournamentStop(void);

/* Swaps the rules of the lineup and those of the tournament, so that the
 * settings file gets the first while a tournament runs. Call it twice. */
void tournamentSwapRules(void);

/* Sets up the page; `parent` is where B leads back to. */
void tournamentInit(struct scene *parent);

#endif

#endif
