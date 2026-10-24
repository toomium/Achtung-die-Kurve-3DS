#ifdef __3DS__

#include "touch3ds.h"

#define MAX_TARGETS 40

static struct touchTarget targets[MAX_TARGETS];
static int count = 0;

void touchReset(void)
{
    count = 0;
}

void touchAdd(SDL_Rect rect, int id, int value)
{
    if (count < MAX_TARGETS) {
        targets[count].rect = rect;
        targets[count].id = id;
        targets[count].value = value;
        ++count;
    }
}

const struct touchTarget *touchFind(int x, int y)
{
    for (int i = 0; i < count; ++i) {
        SDL_Rect *r = &targets[i].rect;
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)
            return &targets[i];
    }
    return NULL;
}

#endif /* __3DS__ */
