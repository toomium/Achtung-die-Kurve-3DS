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
    double seek;        /* How far it goes for an item */
} levels[N_AI_LEVELS] = {
    [AI_NORMAL] = {85, 40, 60, 0, 0.0001, 140},
    [AI_EASY]   = {50, 24, 36, 12, 0.00005, 70},
    [AI_HARD]   = {120, 56, 84, 0, 0.0003, 220}
};

double aiSeekRange(int level)
{
    return levels[level % N_AI_LEVELS].seek;
}

char aiSteerTo(double posx, double posy, double dir, double tx, double ty)
{
    double diff = atan2(ty - posy, tx - posx) - dir;

    while (diff > M_PI)
        diff -= 2 * M_PI;
    while (diff < -M_PI)
        diff += 2 * M_PI;
    /* Turning right makes the direction grow. */
    return diff > 0.15 ? 'r' : diff < -0.15 ? 'l' : '0';
}

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
/* What the hitmap has at a place: -1 for a wall, or (with `wrap`) what is at
 * the place on the other side of the field. */
static int peek(const unsigned char *hitmap, unsigned int w, unsigned int h,
                int x, int y, bool wrap)
{
    if (wrap) {
        x = (int)((x % (int)w + (int)w) % (int)w);
        y = (int)((y % (int)h + (int)h) % (int)h);
    } else if (x < 0 || y < 0 || (unsigned)x >= w || (unsigned)y >= h) {
        return -1;
    }
    return hitmap[w * y + x];
}

static char look(double posx, double posy, double dir, unsigned char id,
                 const unsigned char *hitmap, unsigned int w, unsigned int h,
                 int level, bool wrap)
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
            result = peek(hitmap, w, h, checkx, checky, wrap);
            if (result < 0)
                return 'r';
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'r';
            /* right diagonal */
            checkx = posx + i*cos(dir + M_PI_4);
            checky = posy + i*sin(dir + M_PI_4);
            result = peek(hitmap, w, h, checkx, checky, wrap);
            if (result < 0)
                return 'r';
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'l';
        }
        if (i < COWARDNESS_SIDES) {
            /* right side */
            checkx = posx + i*cos(dir + M_PI_2);
            checky = posy + i*sin(dir + M_PI_2);
            result = peek(hitmap, w, h, checkx, checky, wrap);
            if (result < 0)
                return 'r';
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'l';
            /* left side */
            checkx = posx + i*cos(dir - M_PI_2);
            checky = posy + i*sin(dir - M_PI_2);
            result = peek(hitmap, w, h, checkx, checky, wrap);
            if (result < 0)
                return 'r';
            if (result != id + 8 && result != id + 16 && result != 0)
                return 'r';
        }
        /* front */
        checkx = posx + i * cos(dir);
        checky = posy + i * sin(dir);
        result = peek(hitmap, w, h, checkx, checky, wrap);
        if (result < 0)
            return 'r';
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
            const unsigned char *hitmap, unsigned int w, unsigned int h,
            int level, bool wrap)
{
    level %= N_AI_LEVELS;
    char turn = look(posx, posy, dir, id, hitmap, w, h, level, wrap);

    /* A weak player now and then does not see it coming. */
    if (turn != '0' && levels[level].lapse && rand() % levels[level].lapse == 0)
        return '0';
    return turn;
}
