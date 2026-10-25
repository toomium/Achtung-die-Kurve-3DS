#ifdef __3DS__

#include "items3ds.h"

#include <stdlib.h>

#include "player.h"

bool itemMode = false;

#define ITEM_LIFE       14000   /* ms before an item goes away */
#define ITEM_BLINK      3000    /* ... and blinks for the last of them */
#define FIRST_ITEM_MS   2500
#define MARGIN          24      /* Distance from the walls */
#define CLEAR           8       /* Free space around a new item */
#define FAR_FROM_HEAD   40

struct item {
    int x, y, type;
    int age;
};

static struct item items[MAX_ITEMS];
static int count = 0;
static int spawnIn = 0;

void itemsReset(void)
{
    count = 0;
    spawnIn = FIRST_ITEM_MS + rand() % 2000;
}

int itemsCount(void)
{
    return count;
}

bool itemsGet(int i, int *x, int *y, int *type)
{
    if (i < 0 || i >= count)
        return false;
    *x = items[i].x;
    *y = items[i].y;
    *type = items[i].type;
    return true;
}

static void dropItem(int i)
{
    items[i] = items[--count];
}

/* A place with nothing around it: no trail, no wall, no player close by. */
static bool freeSpot(int x, int y, const unsigned char *hitmap, unsigned w)
{
    for (int dy = -CLEAR; dy <= CLEAR; dy += 4) {
        for (int dx = -CLEAR; dx <= CLEAR; dx += 4) {
            if (hitmap[w * (y + dy) + x + dx] != 0)
                return false;
        }
    }
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const struct player *p = &players[i];
        if (p->active && p->alive && abs((int)p->posx - x) < FAR_FROM_HEAD
            && abs((int)p->posy - y) < FAR_FROM_HEAD)
            return false;
    }
    return true;
}

static void spawn(const unsigned char *hitmap, unsigned w, unsigned h)
{
    if (count >= MAX_ITEMS || w < 2 * MARGIN + 1 || h < 2 * MARGIN + 1)
        return;
    for (int tries = 0; tries < 30; ++tries) {
        int x = MARGIN + rand() % (w - 2 * MARGIN);
        int y = MARGIN + rand() % (h - 2 * MARGIN);
        if (freeSpot(x, y, hitmap, w)) {
            items[count++] = (struct item){x, y, rand() % N_WEAPONS, 0};
            return;
        }
    }
}

bool itemsUpdate(Uint32 delta, const unsigned char *hitmap, unsigned w,
                 unsigned h)
{
    bool took = false;

    for (int i = 0; i < count; ++i) {
        items[i].age += delta;
        if (items[i].age >= ITEM_LIFE)
            dropItem(i--);
    }

    spawnIn -= delta;
    if (spawnIn <= 0) {
        spawn(hitmap, w, h);
        spawnIn = 4000 + rand() % 4000;
    }

    for (int k = 0; k < MAX_PLAYERS; ++k) {
        struct player *p = &players[k];
        /* Only somebody with an empty hand takes one. */
        if (!p->active || !p->alive || p->wep_count > 0
            || p->wep_time != WEP_NONACTIVE)
            continue;
        for (int i = 0; i < count; ++i) {
            int dx = (int)p->posx - items[i].x;
            int dy = (int)p->posy - items[i].y;
            if (dx * dx + dy * dy <= ITEM_RADIUS * ITEM_RADIUS) {
                p->weapon = items[i].type;
                p->wep_count = wep_list[p->weapon].charges;
                dropItem(i);
                took = true;
                break;
            }
        }
    }
    return took;
}

void itemsDraw(SDL_Surface *s, SDL_Rect at, SDL_Surface *const icons[N_WEAPONS])
{
    Uint32 ticks = SDL_GetTicks();

    for (int i = 0; i < count; ++i) {
        /* In the last seconds an item blinks. */
        if (items[i].age > ITEM_LIFE - ITEM_BLINK && (ticks / 150) % 2)
            continue;

        /* A frame that pulses, so that an item is easy to see. */
        int grow = (ticks / 200) % 2;
        SDL_Rect frame = {items[i].x - 10 - grow + at.x,
                          items[i].y - 10 - grow + at.y, 20 + 2 * grow,
                          20 + 2 * grow};
        Uint32 c = SDL_MapRGB(s->format, 0xFF, 0xC8, 0x57);
        SDL_Rect sides[4] = {
            {frame.x, frame.y, frame.w, 1},
            {frame.x, frame.y + frame.h - 1, frame.w, 1},
            {frame.x, frame.y, 1, frame.h},
            {frame.x + frame.w - 1, frame.y, 1, frame.h}
        };
        for (int k = 0; k < 4; ++k)
            SDL_FillRect(s, &sides[k], c);

        SDL_Surface *icon = icons[items[i].type];
        SDL_Rect at2 = {items[i].x - icon->w / 2 + at.x,
                        items[i].y - icon->h / 2 + at.y, 0, 0};
        SDL_BlitSurface(icon, NULL, s, &at2);
    }
}

#endif /* __3DS__ */
