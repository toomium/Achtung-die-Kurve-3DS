#ifdef __3DS__

#include "bracket3ds.h"

#include <string.h>

/* ---- The bracket ---- */

static struct tourRound rounds[TOUR_MAX_ROUNDS];
static int nRounds = 0;
static int pair = 0;            /* The next pair of the last round */
static int champion = -1;
static int duelPair = -1;       /* The pair that tourNextDuel() gave, or -1 */

bool tourStart(const int *slots, int n)
{
    if (n < 2 || n > TOUR_MAX_PLAYERS)
        return false;
    memset(rounds, 0, sizeof(rounds));
    rounds[0].n = n;
    for (int i = 0; i < n; ++i)
        rounds[0].slot[i] = slots[i];
    for (int i = 0; i < TOUR_MAX_PLAYERS / 2 + 1; ++i)
        rounds[0].winner[i] = -1;
    nRounds = 1;
    pair = 0;
    champion = -1;
    duelPair = -1;
    return true;
}

/* The winners of the last round are the players of a new one. */
static void nextRound(void)
{
    struct tourRound *last = &rounds[nRounds - 1];
    struct tourRound *next = &rounds[nRounds];

    memset(next, 0, sizeof(*next));
    next->n = (last->n + 1) / 2;
    for (int i = 0; i < next->n; ++i) {
        next->slot[i] = last->winner[i];
        next->winner[i] = -1;
    }
    ++nRounds;
    pair = 0;
}

bool tourNextDuel(int *a, int *b)
{
    while (nRounds > 0 && champion < 0) {
        struct tourRound *r = &rounds[nRounds - 1];

        if (r->n == 1) {
            champion = r->slot[0];
            break;
        }
        if (pair >= (r->n + 1) / 2) {
            nextRound();
            continue;
        }
        if (2 * pair + 1 >= r->n) {         /* Nobody to play against: a bye */
            r->winner[pair] = r->slot[2 * pair];
            ++pair;
            continue;
        }
        *a = r->slot[2 * pair];
        *b = r->slot[2 * pair + 1];
        duelPair = pair;
        return true;
    }
    return false;
}

void tourDuelDone(int winner)
{
    if (duelPair < 0 || nRounds == 0)
        return;
    rounds[nRounds - 1].winner[duelPair] = winner;
    duelPair = -1;
    ++pair;
}

int tourChampion(void)
{
    return champion;
}

int tourRounds(void)
{
    return nRounds;
}

const struct tourRound *tourRoundAt(int i)
{
    return i >= 0 && i < nRounds ? &rounds[i] : NULL;
}

#endif /* __3DS__ */
