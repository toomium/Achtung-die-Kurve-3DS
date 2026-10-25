/* stats3ds -- what this console's players have won, and the title that goes
 * with it. Only humans on this console count (in a match with several
 * consoles: the player of this console).
 */
#ifndef STATS3DS_H
#define STATS3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <stddef.h>

struct stats {
    unsigned matches, matchWins, rounds, roundWins, bestStreak;
};

extern struct stats stats;

/* A round is over. `winner` is an index into players[], or -1 for a draw. */
void statsRoundEnd(int winner);

/* A match is over. */
void statsMatchEnd(int winner);

/* The title for the rounds won so far. */
const char *statsTitle(void);

/* The title reached with the last round, or NULL if there was none. */
const char *statsNewTitle(void);

/* The titles, in the order of the rounds one has to win for them. */
int statsTitleCount(void);
const char *statsTitleName(int i);
unsigned statsTitleWins(int i);
int statsTitleNow(void);

/* The numbers as one line of settings.cfg ("5,2,30,12,3"), and back. Parsing
 * keeps the old numbers and returns false if the line is wrong. */
void statsFormat(char *buf, size_t size);
bool statsParse(const char *text);

#endif

#endif
