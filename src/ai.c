/* ai -- artificial intelligence experimentation.
 * Copyright (C) 2010-2021 Simen Heggestøyl <simenheg@runbox.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ai.h"

#include <stdlib.h>

/* How far a computer player looks ahead, per level. */
static const struct {
    int front, sides, diags;
    int lapse;          /* One in this many turns is missed, 0: never */
    double weaponProb;
} levels[N_AI_LEVELS] = {
    [AI_NORMAL] = {85, 40, 60, 0, 0.0001},
    [AI_EASY]   = {50, 24, 36, 12, 0.00005},
    [AI_HARD]   = {120, 56, 84, 0, 0.0003}
};

double aiWeaponProb(int level)
{
    return levels[level % N_AI_LEVELS].weaponProb;
}

/**
 * NOTE: This is only a quick doodle, and is not to be considered final!
 *
 * Polls the AI to decide what action to do.
 *
 * @param posx Current x position.
 * @param posy Current y position.
 * @param dir Current direction.
 * @param id ID of the player to decide actions for.
 * @param hitmap A pointer to the whole level hitmap.
 * @param w Width of the playfield.
 * @param h Height of the playfield.
 * @param level AI_NORMAL, AI_EASY or AI_HARD.
 * @return 'l' to turn left, 'r' to turn right, '0' else.
 */
static char look(double posx, double posy, double dir, unsigned char id,
                 unsigned char *hitmap, unsigned int w, unsigned int h,
                 int level)
{
    const int COWARDNESS_FRONT = levels[level].front;
    const int COWARDNESS_SIDES = levels[level].sides;
    const int COWARDNESS_DIAGS = levels[level].diags;

    for (int i = 0; i < COWARDNESS_FRONT; ++i) {
        int checkx, checky, result;
        if (i < COWARDNESS_DIAGS) {
            /* left diagonal */
            checkx = posx + i*cos(dir - M_PI_4);
            checky = posy + i*sin(dir - M_PI_4);
            if (checkx < 0 || checky < 0 || (unsigned)checkx >= w ||
                    (unsigned)checky >= h)
                return 'r';
            result = hitmap[w * checky + checkx];
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'r';
            /* right diagonal */
            checkx = posx + i*cos(dir + M_PI_4);
            checky = posy + i*sin(dir + M_PI_4);
            if (checkx < 0 || checky < 0 || (unsigned)checkx >= w ||
                    (unsigned)checky >= h)
                return 'r';
            result = hitmap[w * checky + checkx];
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'l';
        }
        if (i < COWARDNESS_SIDES) {
            /* right side */
            checkx = posx + i*cos(dir + M_PI_2);
            checky = posy + i*sin(dir + M_PI_2);
            if (checkx < 0 || checky < 0 || (unsigned)checkx >= w ||
                    (unsigned)checky >= h)
                return 'r';
            result = hitmap[w * checky + checkx];
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'l';
            /* left side */
            checkx = posx + i*cos(dir - M_PI_2);
            checky = posy + i*sin(dir - M_PI_2);
            if (checkx < 0 || checky < 0 || (unsigned)checkx >= w ||
                    (unsigned)checky >= h)
                return 'r';
            result = hitmap[w * checky + checkx];
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'r';
        }
        /* front */
        checkx = posx + i * cos(dir);
        checky = posy + i * sin(dir);
        if (checkx < 0 || checky < 0 || (unsigned)checkx >= w ||
                (unsigned)checky >= h)
            return 'r';
        result = hitmap[w * checky + checkx];
        if (result != id + 8 && result != id + 16 && result != 0) {
            if (result == id)
                return 'l';
            else
                return 'r';
        }
    }

    return '0';
}

char pollAi(double posx, double posy, double dir, unsigned char id,
            unsigned char *hitmap, unsigned int w, unsigned int h, int level)
{
    level %= N_AI_LEVELS;
    char turn = look(posx, posy, dir, id, hitmap, w, h, level);

    /* A weak player now and then does not see it coming. */
    if (turn != '0' && levels[level].lapse && rand() % levels[level].lapse == 0)
        return '0';
    return turn;
}
