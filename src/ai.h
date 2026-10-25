#ifndef AI_H
#define AI_H

#include <math.h>
#include "common.h"

/* How well a computer player plays. NORMAL is 0, so a zeroed player is normal. */
enum { AI_NORMAL = 0, AI_EASY, AI_HARD, N_AI_LEVELS };

/* The chance per frame that a computer player fires its weapon. */
double aiWeaponProb(int level);

char pollAi(double posx, double posy, double dir, unsigned char id,
            unsigned char *hitmap, unsigned int w, unsigned int h, int level);

#endif
