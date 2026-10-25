/* bracket3ds -- the bracket of a knock-out tournament: who plays whom, byes,
 * and the champion. No screen and no game, so it can be tested alone.
 */
#ifndef BRACKET3DS_H
#define BRACKET3DS_H

#ifdef __3DS__

#include <stdbool.h>

#define TOUR_MAX_PLAYERS    8
#define TOUR_MAX_ROUNDS     4

/* One round of the bracket. Slots are the numbers of the lineup (0-7). */
struct tourRound {
    int n;                                  /* Players in this round */
    int slot[TOUR_MAX_PLAYERS];
    int winner[TOUR_MAX_PLAYERS / 2 + 1];   /* Per pair, -1: not played yet */
};

/* Begins a tournament of `n` (2 to 8) players. Returns false if n is wrong. */
bool tourStart(const int *slots, int n);

/* The next duel to play: the two slots. Byes are done on the way. Returns false
 * when the tournament is over (see tourChampion). */
bool tourNextDuel(int *a, int *b);

/* The duel that tourNextDuel() gave is over. */
void tourDuelDone(int winner);

/* The champion's slot, or -1 while the tournament goes on. */
int tourChampion(void);

int tourRounds(void);
const struct tourRound *tourRoundAt(int i);

#endif

#endif
