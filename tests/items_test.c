/* Items appear on free ground, are taken by empty hands, and go away. */
#include <assert.h>
#include <string.h>

#include "../src/items3ds.c"

#define W 400
#define H 240

struct player players[MAX_PLAYERS];
unsigned char nPlayers;
struct weapon wep_list[N_WEAPONS];

static unsigned char hitmap[W * H];

static void run(int ms)
{
    for (int t = 0; t < ms; t += 30)
        itemsUpdate(30, hitmap, W, H);
}

int main(void)
{
    for (int i = 0; i < N_WEAPONS; ++i)
        wep_list[i].charges = 1 + i % 3;
    players[0] = (struct player){.active = 1, .alive = true, .posx = 10,
                                 .posy = 10, .wep_time = WEP_NONACTIVE};

    /* Items appear, never on the trail (the left half is blocked). */
    srand(7);
    memset(hitmap, 0, sizeof(hitmap));
    for (int y = 0; y < H; ++y)
        memset(&hitmap[W * y], 1, W / 2);
    itemsReset();
    assert(itemsCount() == 0);
    run(30000);
    assert(itemsCount() > 0 && itemsCount() <= MAX_ITEMS);
    for (int i = 0; i < itemsCount(); ++i) {
        int x, y, type;
        assert(itemsGet(i, &x, &y, &type));
        assert(x >= W / 2 + CLEAR && x < W - MARGIN);
        assert(y >= MARGIN && y < H - MARGIN && type >= 0 && type < N_WEAPONS);
    }

    /* A player with empty hands takes one that he drives over. */
    int x, y, type;
    assert(itemsGet(0, &x, &y, &type));
    int before = itemsCount();
    players[0].posx = x + 3;
    players[0].posy = y - 2;
    assert(itemsUpdate(1, hitmap, W, H));
    assert(players[0].weapon == type);
    assert(players[0].wep_count == wep_list[type].charges);
    assert(itemsCount() <= before - 1 + 1);     /* One less, or a new one */

    /* The nearest one within range, for a player that goes for it. */
    int tx, ty;
    assert(itemsGet(0, &x, &y, &type));
    assert(itemsNearest(x + 5, y, 20, &tx, &ty) && tx == x && ty == y);
    assert(!itemsNearest(x + 100, y, 20, &tx, &ty));

    /* With a weapon in the hand, or dead, he leaves them. */
    int n = itemsCount();
    assert(n > 0 && itemsGet(0, &x, &y, &type));
    players[0].posx = x;
    players[0].posy = y;
    assert(!itemsUpdate(0, hitmap, W, H));
    players[0].wep_count = 0;
    players[0].alive = false;
    assert(!itemsUpdate(0, hitmap, W, H));
    assert(itemsCount() == n);

    /* No room for new ones, and the old ones go away by themselves. */
    memset(hitmap, 1, sizeof(hitmap));
    run(ITEM_LIFE + 30);
    assert(itemsCount() == 0);
    return 0;
}
