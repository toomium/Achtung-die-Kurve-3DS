#ifdef __3DS__

#include "demo3ds.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "gfx.h"
#include "ui3ds.h"

/* The size of the top screen. */
#define W            ((int)DEFAULT_WINDOW_W)
#define H            ((int)DEFAULT_WINDOW_H)

#define MAX_WORMS    8
#define TRAIL        1200       /* Positions kept per ball: the line's length */
#define STEP         16         /* Milliseconds per movement step */
#define SPEED        0.05       /* Pixels per millisecond (the game: 0.045) */
#define MAX_TURN     0.0035     /* Radians per millisecond */
#define MARGIN       36         /* Distance to the edge where a ball turns away */
#define CAREFUL      80         /* Percent of the balls that avoid the walls */
#define MORE_EVERY   3000       /* Milliseconds until one more ball is wanted */
#define FADE_TIME    1500       /* Milliseconds a lost ball's line fades */
#define SELF_STEPS   24         /* Own line this young is not hit */

struct worm {
    bool used;                  /* In use: alive or still fading */
    bool alive;
    bool careful;
    bool entering;              /* Still coming in from outside the screen */
    double x, y, dir;
    double turn, turnGoal;      /* The turning rate, and the one it moves to */
    int changeIn;               /* Milliseconds until it picks a new goal */
    unsigned steps;
    short px[TRAIL], py[TRAIL];
    int head, count;
    int fade;                   /* Milliseconds since it was lost */
    int x0, y0, x1, y1;         /* Where its line is, for repairing crossings */
};

static struct worm worms[MAX_WORMS];
static SDL_Surface *canvas = NULL;      /* The lines, drawn once, like the game */
static unsigned char owner[W * H];      /* Who has a line on a pixel: 0 or 1..8 */
static unsigned short stamp[W * H];     /* ... and at which step */
static Uint32 lastTime = 0;
static int stepRest = 0;
static int wanted = 1;                  /* How many balls we want to see */
static int untilMore = MORE_EVERY;
static int untilSpawn = 0;

static double frand(double lo, double hi)
{
    return lo + (hi - lo) * (rand() / (double)RAND_MAX);
}

static Uint32 mapColor(SDL_Color c)
{
    return SDL_MapRGB(canvas->format, c.r, c.g, c.b);
}

static SDL_Color bgColor(void)
{
    SDL_Color bg = {BG_R, BG_G, BG_B, 0};
    return bg;
}

/* A 3x3 square, the thickness of a line in the game. */
static void plot(int x, int y, SDL_Color c)
{
    SDL_Rect r = {x - 1, y - 1, 3, 3};
    SDL_FillRect(canvas, &r, mapColor(c));
}

static void markCells(int id, unsigned steps, int x, int y)
{
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = x + dx, cy = y + dy;
            if (cx >= 0 && cx < W && cy >= 0 && cy < H) {
                owner[cy * W + cx] = (unsigned char)(id + 1);
                stamp[cy * W + cx] = (unsigned short)steps;
            }
        }
    }
}

static void clearCells(int id, int x, int y)
{
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = x + dx, cy = y + dy;
            if (cx >= 0 && cx < W && cy >= 0 && cy < H
                    && owner[cy * W + cx] == id + 1)
                owner[cy * W + cx] = 0;
        }
    }
}

/* A new ball just outside a random edge, heading inwards. */
static void spawn(int slot)
{
    struct worm *w = &worms[slot];
    int side = rand() % 4;
    const int out = 14;

    memset(w, 0, sizeof(*w));
    switch (side) {
    case 0: w->x = -out;            w->y = frand(30, H); w->dir = 0; break;
    case 1: w->x = W + out;         w->y = frand(30, H); w->dir = M_PI; break;
    case 2: w->x = frand(30, W); w->y = -out;            w->dir = M_PI / 2; break;
    default: w->x = frand(30, W); w->y = H + out;        w->dir = -M_PI / 2; break;
    }
    w->dir += frand(-0.4, 0.4);
    w->turnGoal = frand(-MAX_TURN, MAX_TURN);
    w->changeIn = 600 + rand() % 1200;
    w->careful = rand() % 100 < CAREFUL;
    w->entering = true;
    w->used = true;
    w->alive = true;
    w->x0 = W;
    w->y0 = H;
    w->x1 = 0;
    w->y1 = 0;
}

void demoReset(void)
{
    if (canvas == NULL) {
        SDL_PixelFormat *f = SDL_GetVideoSurface()->format;
        canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, W, H, f->BitsPerPixel,
                                      f->Rmask, f->Gmask, f->Bmask, 0);
    }
    SDL_FillRect(canvas, NULL, mapColor(bgColor()));
    memset(worms, 0, sizeof(worms));
    memset(owner, 0, sizeof(owner));
    memset(stamp, 0, sizeof(stamp));

    wanted = 1;
    untilMore = MORE_EVERY;
    untilSpawn = 0;
    lastTime = SDL_GetTicks();
    stepRest = 0;
}

/* The difference between two directions, in -pi..pi. */
static double angleDiff(double a, double b)
{
    double d = fmod(a - b, 2 * M_PI);
    if (d > M_PI)
        d -= 2 * M_PI;
    else if (d < -M_PI)
        d += 2 * M_PI;
    return d;
}

/* The ball hit something: it stops, its line fades, then it is gone. */
static void lose(struct worm *w)
{
    w->alive = false;
    w->fade = 0;
}

static void step(int slot)
{
    struct worm *w = &worms[slot];
    SDL_Color color = colors[slot % N_COLORS];

    /* Now and then a new turning rate to aim for. */
    w->changeIn -= STEP;
    if (w->changeIn <= 0) {
        w->turnGoal = frand(-MAX_TURN, MAX_TURN);
        w->changeIn = 600 + rand() % 1200;
    }
    w->turn += (w->turnGoal - w->turn) * 0.05;
    double turn = w->turn;

    /* Careful balls turn back towards the middle near an edge. A ball that
     * is still coming in just goes straight on. */
    bool inside = w->x > 1 && w->x < W - 1 && w->y > 1 && w->y < H - 1;
    if (w->entering && inside)
        w->entering = false;
    if (w->entering) {
        turn = 0;
    } else if (w->careful) {
        bool near = w->x < MARGIN || w->x > W - MARGIN || w->y < MARGIN
            || w->y > H - MARGIN;
        if (near) {
            double toMiddle = atan2(H / 2 - w->y, W / 2 - w->x);
            double d = angleDiff(toMiddle, w->dir);
            if (fabs(d) >= 0.15)
                turn = d > 0 ? MAX_TURN * 1.3 : -MAX_TURN * 1.3;
        }
    }

    w->dir += turn * STEP;
    w->x += cos(w->dir) * SPEED * STEP;
    w->y += sin(w->dir) * SPEED * STEP;
    ++w->steps;

    if (w->entering)
        return;

    /* The wall, or a line, just in front: out. */
    int fx = (int)(w->x + cos(w->dir) * 2);
    int fy = (int)(w->y + sin(w->dir) * 2);
    if (fx < 1 || fx >= W - 1 || fy < 1 || fy >= H - 1) {
        lose(w);
        return;
    }
    int at = fy * W + fx;
    if (owner[at] != 0) {
        bool mine = owner[at] == slot + 1;
        if (!mine || (unsigned short)(w->steps - stamp[at]) > SELF_STEPS) {
            lose(w);
            return;
        }
    }

    /* The line goes on; when it is full length, the oldest part is taken back. */
    if (w->count == TRAIL) {
        int old = w->head;
        plot(w->px[old], w->py[old], bgColor());
        clearCells(slot, w->px[old], w->py[old]);
        --w->count;
    }
    int x = (int)w->x, y = (int)w->y;
    w->px[w->head] = (short)x;
    w->py[w->head] = (short)y;
    w->head = (w->head + 1) % TRAIL;
    ++w->count;
    plot(x, y, color);
    markCells(slot, w->steps, x, y);
    if (x < w->x0) w->x0 = x;
    if (y < w->y0) w->y0 = y;
    if (x > w->x1) w->x1 = x;
    if (y > w->y1) w->y1 = y;
}

/* Draws the line of a ball in a color, every second position is enough. */
static void drawLine(struct worm *w, SDL_Color c)
{
    int first = (w->head - w->count + TRAIL) % TRAIL;
    for (int k = 0; k < w->count; k += 2) {
        int at = (first + k) % TRAIL;
        plot(w->px[at], w->py[at], c);
    }
}

/* The line of a lost ball has faded: take it away, and draw the lines of the
 * others again where they crossed it. */
static void dropLine(int slot)
{
    struct worm *w = &worms[slot];
    int first = (w->head - w->count + TRAIL) % TRAIL;

    for (int k = 0; k < w->count; ++k) {
        int at = (first + k) % TRAIL;
        plot(w->px[at], w->py[at], bgColor());
        clearCells(slot, w->px[at], w->py[at]);
    }

    for (int i = 0; i < MAX_WORMS; ++i) {
        struct worm *o = &worms[i];
        if (i == slot || !o->used || !o->alive)
            continue;
        int ofirst = (o->head - o->count + TRAIL) % TRAIL;
        for (int k = 0; k < o->count; ++k) {
            int at = (ofirst + k) % TRAIL;
            if (o->px[at] >= w->x0 - 2 && o->px[at] <= w->x1 + 2
                    && o->py[at] >= w->y0 - 2 && o->py[at] <= w->y1 + 2)
                plot(o->px[at], o->py[at], colors[i % N_COLORS]);
        }
    }
    w->used = false;
}

void demoUpdate(void)
{
    Uint32 now = SDL_GetTicks();
    int dt = now - lastTime;
    lastTime = now;
    if (dt > 100)
        dt = 100;

    /* One more ball is wanted every few seconds, up to all eight. */
    untilMore -= dt;
    if (untilMore <= 0 && wanted < MAX_WORMS) {
        ++wanted;
        untilMore = MORE_EVERY;
    }

    /* Balls that came in take the place of those that were lost. */
    int inUse = 0;
    for (int i = 0; i < MAX_WORMS; ++i)
        inUse += worms[i].used;
    untilSpawn -= dt;
    if (inUse < wanted && untilSpawn <= 0) {
        for (int i = 0; i < MAX_WORMS; ++i) {
            if (!worms[i].used) {
                spawn(i);
                untilSpawn = 600 + rand() % 1500;
                break;
            }
        }
    }

    stepRest += dt;
    while (stepRest >= STEP) {
        stepRest -= STEP;
        for (int i = 0; i < MAX_WORMS; ++i) {
            if (worms[i].used && worms[i].alive)
                step(i);
        }
    }

    /* The lines of lost balls fade into the background. */
    SDL_Color bg = bgColor();
    for (int i = 0; i < MAX_WORMS; ++i) {
        struct worm *w = &worms[i];
        if (!w->used || w->alive)
            continue;
        w->fade += dt;
        if (w->fade >= FADE_TIME)
            dropLine(i);
        else
            drawLine(w, uiMix(colors[i % N_COLORS], bg, 100 * w->fade / FADE_TIME));
    }
}

void demoDraw(SDL_Surface *s)
{
    SDL_BlitSurface(canvas, NULL, s, NULL);
}

#endif /* __3DS__ */
