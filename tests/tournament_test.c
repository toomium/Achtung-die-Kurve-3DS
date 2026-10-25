/* The bracket of a tournament: who plays whom, byes, and the champion. */
#include <assert.h>
#include <string.h>

#include "../src/bracket3ds.c"

/* Plays the whole tournament; `pick` says who wins a duel. Returns the number
 * of duels played. */
static int playAll(const int *slots, int n, int (*pick)(int, int))
{
    int a, b, duels = 0;

    assert(tourStart(slots, n));
    while (tourNextDuel(&a, &b)) {
        assert(a != b);
        tourDuelDone(pick(a, b));
        ++duels;
        assert(duels <= 20);
    }
    return duels;
}

static int lower(int a, int b) { return a < b ? a : b; }
static int higher(int a, int b) { return a > b ? a : b; }

int main(void)
{
    const int slots[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    /* A knock-out has n - 1 duels, whatever n is. */
    for (int n = 2; n <= 8; ++n) {
        assert(playAll(slots, n, lower) == n - 1);
        assert(tourChampion() == 0);
    }
    assert(playAll(slots, 8, higher) == 7 && tourChampion() == 7);

    /* Five players: the first round is 2 duels and a bye, so slot 4 waits. */
    assert(tourStart(slots, 5));
    int a, b;
    assert(tourNextDuel(&a, &b) && a == 0 && b == 1);
    tourDuelDone(1);
    assert(tourNextDuel(&a, &b) && a == 2 && b == 3);
    tourDuelDone(3);
    assert(tourNextDuel(&a, &b));       /* The bye of slot 4 is done */
    assert(tourRounds() == 2 && tourRoundAt(0)->winner[2] == 4);
    assert(tourRoundAt(1)->n == 3 && tourRoundAt(1)->slot[2] == 4);
    assert(a == 1 && b == 3);
    tourDuelDone(3);
    assert(tourNextDuel(&a, &b) && tourRounds() == 3);  /* 3 against 4 */
    assert(a == 3 && b == 4);
    tourDuelDone(4);
    assert(!tourNextDuel(&a, &b) && tourChampion() == 4);

    /* Wrong numbers of players, and a result without a duel. */
    assert(!tourStart(slots, 1) && !tourStart(slots, 9));
    assert(tourStart(slots, 3));
    tourDuelDone(2);                    /* No duel is open: nothing happens */
    assert(tourNextDuel(&a, &b) && a == 0 && b == 1);
    return 0;
}
