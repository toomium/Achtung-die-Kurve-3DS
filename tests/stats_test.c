/* Counting rounds and matches, the titles, and the line in settings.cfg. */
#include <assert.h>
#include <string.h>

#include "../src/stats3ds.c"

struct player players[MAX_PLAYERS];
unsigned char nPlayers;
static int role = NET_ROLE_NONE, seat = -1;
int netGameRole(void) { return role; }
int netGameMySeat(void) { return seat; }

int main(void)
{
    /* Player 0 is a human, player 1 the computer. */
    nPlayers = 2;
    players[1].ai = true;

    statsRoundEnd(0);
    statsRoundEnd(0);
    statsRoundEnd(1);           /* The computer wins: the streak ends */
    statsRoundEnd(0);
    statsRoundEnd(-1);          /* A draw ends it too */
    assert(stats.rounds == 5 && stats.roundWins == 3 && stats.bestStreak == 2);

    /* The title changes at 5 won rounds, and is told once. */
    assert(strcmp(statsTitle(), "ROOKIE") == 0 && statsNewTitle() == NULL);
    statsRoundEnd(0);
    statsRoundEnd(0);
    assert(strcmp(statsTitle(), "DRIVER") == 0);
    assert(statsNewTitle() != NULL && strcmp(statsNewTitle(), "DRIVER") == 0);
    statsRoundEnd(0);
    assert(statsNewTitle() == NULL);

    statsMatchEnd(0);
    statsMatchEnd(1);
    assert(stats.matches == 2 && stats.matchWins == 1);

    /* Only computer players: nothing counts. */
    players[0].ai = true;
    statsRoundEnd(0);
    statsMatchEnd(0);
    assert(stats.rounds == 8 && stats.matches == 2);

    /* With several consoles only the seat of this console counts. */
    players[0].ai = false;
    role = NET_ROLE_CLIENT;
    seat = 1;
    players[1].ai = false;
    statsRoundEnd(0);
    assert(stats.roundWins == 6);
    statsRoundEnd(1);
    assert(stats.roundWins == 7);

    /* The line in settings.cfg. */
    char line[48];
    statsFormat(line, sizeof(line));
    struct stats saved = stats;
    memset(&stats, 0, sizeof(stats));
    assert(statsParse(line) && memcmp(&stats, &saved, sizeof(stats)) == 0);
    assert(!statsParse("1,2,3") && memcmp(&stats, &saved, sizeof(stats)) == 0);
    assert(!statsParse("1,2,3,4,999999"));
    return 0;
}
