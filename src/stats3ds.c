#ifdef __3DS__

#include "stats3ds.h"

#include <stdbool.h>
#include <stdio.h>

#include "netgame3ds.h"
#include "player.h"

struct stats stats;

/* Keeps the line in settings.cfg short. */
#define STAT_MAX 99999

static const struct {
    unsigned wins;
    const char *name;
} titles[] = {
    {0, "ROOKIE"}, {5, "DRIVER"}, {20, "RACER"}, {50, "HUNTER"},
    {100, "CHAMPION"}, {250, "LEGEND"}
};
#define N_TITLES ((int)(sizeof(titles) / sizeof(titles[0])))

static unsigned streak = 0;             /* Rounds won in a row, not saved */
static const char *newTitle = NULL;

static int titleOf(unsigned wins)
{
    int t = 0;
    for (int i = 0; i < N_TITLES; ++i) {
        if (wins >= titles[i].wins)
            t = i;
    }
    return t;
}

static bool mine(int i)
{
    return !players[i].ai
        && (netGameRole() == NET_ROLE_NONE || i == netGameMySeat());
}

static bool anyMine(void)
{
    for (int i = 0; i < nPlayers; ++i) {
        if (mine(i))
            return true;
    }
    return false;
}

static void count(unsigned *n)
{
    if (*n < STAT_MAX)
        ++*n;
}

void statsRoundEnd(int winner)
{
    newTitle = NULL;
    if (!anyMine())
        return;

    count(&stats.rounds);
    if (winner >= 0 && mine(winner)) {
        int before = titleOf(stats.roundWins);
        count(&stats.roundWins);
        if (++streak > stats.bestStreak && streak <= STAT_MAX)
            stats.bestStreak = streak;
        if (titleOf(stats.roundWins) > before)
            newTitle = titles[titleOf(stats.roundWins)].name;
    } else {
        streak = 0;
    }
}

void statsMatchEnd(int winner)
{
    if (!anyMine())
        return;
    count(&stats.matches);
    if (winner >= 0 && mine(winner))
        count(&stats.matchWins);
}

const char *statsTitle(void)
{
    return titles[titleOf(stats.roundWins)].name;
}

const char *statsNewTitle(void)
{
    return newTitle;
}

void statsFormat(char *buf, size_t size)
{
    snprintf(buf, size, "%u,%u,%u,%u,%u", stats.matches, stats.matchWins,
             stats.rounds, stats.roundWins, stats.bestStreak);
}

bool statsParse(const char *text)
{
    unsigned v[5];
    if (sscanf(text, "%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4]) != 5)
        return false;
    for (int i = 0; i < 5; ++i) {
        if (v[i] > STAT_MAX)
            return false;
    }
    stats = (struct stats){v[0], v[1], v[2], v[3], v[4]};
    return true;
}

#endif /* __3DS__ */
