#ifndef AI_H
#define AI_H

#include <math.h>
#include <stdbool.h>
#include "common.h"

/* How well a computer player plays. NORMAL is 0, so a zeroed player is normal. */
enum { AI_NORMAL = 0, AI_EASY, AI_HARD, N_AI_LEVELS };

/* The chance per frame that a computer player fires its weapon. */
double aiWeaponProb(int level);

/* How far (in pixels) a computer player of this level goes for an item. */
double aiSeekRange(int level);

/* The turn that leads towards (tx, ty): 'l', 'r', or '0' if it is ahead. */
char aiSteerTo(double posx, double posy, double dir, double tx, double ty);

/* What a computer player has to know of the walls. With `wrap` there are none:
 * what lies beyond one edge is the far one. Otherwise (mx, my) is how far the
 * walls have closed in from the sides and from the top and bottom. */
struct aiField {
    bool wrap;
    int mx, my;
};

char pollAi(double posx, double posy, double dir, unsigned char id,
            const unsigned char *hitmap, unsigned int w, unsigned int h,
            int level, const struct aiField *field);

#endif
