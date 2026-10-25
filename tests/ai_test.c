/* A wall ahead is seen from further away by a harder computer player. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ai.c"

#define W 400
#define H 240

static unsigned char hitmap[W * H];

/* A computer player in the middle heading right; a vertical wall of player 2
 * stands `gap` pixels in front of it. */
static char poll(int gap, int level)
{
    memset(hitmap, 0, sizeof(hitmap));
    for (int y = 0; y < H; ++y)
        hitmap[W * y + 100 + gap] = 2;
    return pollAi(100, 120, 0, 1, hitmap, W, H, level, false);
}

int main(void)
{
    /* 100 pixels: only the hard player looks that far (120). */
    assert(poll(100, AI_EASY) == '0');
    assert(poll(100, AI_NORMAL) == '0');
    assert(poll(100, AI_HARD) != '0');
    /* 60 pixels: the normal player (85) sees it, the easy one (50) does not. */
    assert(poll(60, AI_EASY) == '0');
    assert(poll(60, AI_NORMAL) != '0');
    /* The easy player turns most of the time, but not always. */
    int turned = 0;
    for (int i = 0; i < 1000; ++i)
        turned += poll(30, AI_EASY) != '0';
    assert(turned > 800 && turned < 1000);
    /* An empty field: nothing to do. */
    memset(hitmap, 0, sizeof(hitmap));
    assert(pollAi(100, 120, 0, 1, hitmap, W, H, AI_HARD, false) == '0');
    /* A weaker player uses weapons less. */
    assert(aiWeaponProb(AI_EASY) < aiWeaponProb(AI_NORMAL));
    assert(aiWeaponProb(AI_NORMAL) < aiWeaponProb(AI_HARD));
    /* A wall of the field is seen as a wall, unless the field wraps around. */
    memset(hitmap, 0, sizeof(hitmap));
    assert(pollAi(W - 10, 120, 0, 1, hitmap, W, H, AI_NORMAL, false) == 'r');
    assert(pollAi(W - 10, 120, 0, 1, hitmap, W, H, AI_NORMAL, true) == '0');
    /* ... and what lies on the far side counts then. */
    hitmap[W * 120 + 5] = 2;
    assert(pollAi(W - 10, 120, 0, 1, hitmap, W, H, AI_NORMAL, true) != '0');

    /* Steering towards something: right is a larger direction. */
    assert(aiSteerTo(0, 0, 0, 50, 0) == '0');       /* Dead ahead */
    assert(aiSteerTo(0, 0, 0, 0, 50) == 'r');       /* Below, y grows downwards */
    assert(aiSteerTo(0, 0, 0, 0, -50) == 'l');
    assert(aiSteerTo(0, 0, 3.0, 50, -1) == 'l');    /* Behind: the short way round */
    assert(aiSteerTo(0, 0, 3.2, 50, -1) == 'r');
    assert(aiSeekRange(AI_EASY) < aiSeekRange(AI_NORMAL));
    assert(aiSeekRange(AI_NORMAL) < aiSeekRange(AI_HARD));
    return 0;
}
