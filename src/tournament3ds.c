#ifdef __3DS__

#include "tournament3ds.h"

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "gfx.h"
#include "input.h"
#include "player.h"
#include "sound.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

/* ---- On the screen ---- */

/* In zatackax.c. */
int activeSlots(int idx[MAX_PLAYERS]);
void startDuel(int a, int b);

#define DUELS_TO_WIN    3       /* Points a duel is played to */

struct scene tourScene;

static bool active = false;
static struct {
    unsigned scorecap;
    bool duelmode;
} saved;                        /* The rules of the lineup */
static int fight[2];            /* The slots of the next duel */
static Uint32 shownAt = 0;      /* When the bracket was shown */
#define AUTO_FIGHT_MS 3000      /* A duel of two computer players starts by itself */

bool tournamentActive(void)
{
    return active;
}

void tournamentStop(void)
{
    if (!active)
        return;
    scorecap = saved.scorecap;
    duelmode = saved.duelmode;
    active = false;
}

void tournamentSwapRules(void)
{
    unsigned c = scorecap;
    bool d = duelmode;

    scorecap = saved.scorecap;
    duelmode = saved.duelmode;
    saved.scorecap = c;
    saved.duelmode = d;
}

/* Shows who plays next, or who has won. */
static void nextFight(void)
{
    if (!tourNextDuel(&fight[0], &fight[1]))
        playSound(SOUND_ROUND_BEGIN, sound);
}

bool tournamentBegin(void)
{
    int slots[MAX_PLAYERS];
    int n = activeSlots(slots);

    if (n < 3) {
        playSound(SOUND_BEP, sound);
        return false;
    }
    if (!active) {
        saved.scorecap = scorecap;
        saved.duelmode = duelmode;
    }
    /* Every duel is played to a few points, face to face. */
    scorecap = DUELS_TO_WIN;
    duelmode = true;
    active = true;

    tourStart(slots, n);
    nextFight();
    shownAt = SDL_GetTicks();
    playSound(SOUND_BEEP, sound);
    curScene = &tourScene;
    return true;
}

void tournamentDuelOver(int winner)
{
    tourDuelDone(winner);
    nextFight();
    shownAt = SDL_GetTicks();
    curScene = &tourScene;
}

static int logicTour(void)
{
    /* Nobody to press A for two computer players. */
    bool alone = tourChampion() < 0 && slotMode[fight[0]] == SLOT_AI
        && slotMode[fight[1]] == SLOT_AI;

    if (enterButtonDown() || (alone && SDL_GetTicks() - shownAt > AUTO_FIGHT_MS)) {
        clearEnterButtons();
        if (tourChampion() >= 0) {
            playSound(SOUND_BEEP, sound);
            tournamentStop();
            curScene = tourScene.parentScene;
        } else {
            playSound(SOUND_ROUND_BEGIN, sound);
            startDuel(fight[0], fight[1]);
        }
        return 1;
    }
    return 0;
}

/* The players of the lineup, as they are set up (packed or not). */

static const char *slotName(int slot)
{
    return slotConfig(slot)->name;
}

static SDL_Color slotColor(int slot)
{
    return colors[slotConfig(slot)->color];
}

/* The bracket on the top screen: a column per round, winners in green, the
 * rounds to come as empty boxes, and lines from each pair to the next round. */
static void drawBracket(void)
{
    useTopView();
    clearBackground(screen);

    const int colW = 100, entryW = 94, entryH = 18, top = 14, height = 212;
    int ys[TOUR_MAX_ROUNDS + 1][TOUR_MAX_PLAYERS];
    int count[TOUR_MAX_ROUNDS + 1];
    int played = tourRounds();

    /* The size of every round: those played, then the planned ones. */
    int shown = 0;
    for (int r = 0; r < TOUR_MAX_ROUNDS + 1; ++r) {
        if (r < played)
            count[r] = tourRoundAt(r)->n;
        else if (r > 0 && count[r - 1] > 1)
            count[r] = (count[r - 1] + 1) / 2;
        else
            break;
        ++shown;
    }

    for (int r = 0; r < shown; ++r) {
        for (int i = 0; i < count[r]; ++i) {
            if (r == 0)
                ys[r][i] = top + (2 * i + 1) * height / (2 * count[r])
                    - entryH / 2;
            else
                ys[r][i] = (ys[r - 1][2 * i]
                            + ys[r - 1][2 * i + 1 < count[r - 1] ? 2 * i + 1
                                                                 : 2 * i]) / 2;
        }
    }

    Uint32 line = uiMap(screen, uiMix(uiColPanel, uiColDim, 60));
    for (int r = 0; r + 1 < shown; ++r) {
        int x = 6 + r * colW + entryW;
        for (int i = 0; i < count[r]; ++i) {
            SDL_Rect stub = {x, ys[r][i] + entryH / 2, 3, 1};
            SDL_FillRect(screen, &stub, line);
        }
        for (int p = 0; p < count[r + 1]; ++p) {
            int a = ys[r][2 * p] + entryH / 2;
            int b = ys[r][2 * p + 1 < count[r] ? 2 * p + 1 : 2 * p] + entryH / 2;
            SDL_Rect bar = {x + 2, a, 1, b - a + 1};
            SDL_FillRect(screen, &bar, line);
            SDL_Rect out = {x + 3, ys[r + 1][p] + entryH / 2, colW - entryW - 3, 1};
            SDL_FillRect(screen, &out, line);
        }
    }

    for (int r = 0; r < shown; ++r) {
        const struct tourRound *R = r < played ? tourRoundAt(r) : NULL;
        for (int i = 0; i < count[r]; ++i) {
            SDL_Rect box = {6 + r * colW, ys[r][i], entryW, entryH};
            if (R == NULL) {
                uiPanel(screen, box, uiColPanel,
                        uiMix(uiColPanel, uiColDim, 25));
                uiText(screen, FONT_BROADCAST, "?", uiColOff, box.x + 12,
                       box.y + 5, UI_LEFT);
                continue;
            }
            int slot = R->slot[i];
            int won = R->winner[i / 2];     /* The result of this pair */
            bool champ = R->n == 1;
            bool done = !champ && won >= 0;
            SDL_Color c = slotColor(slot);
            bool lost = done && won != slot;
            uiPanel(screen, box, uiColPanel,
                    champ ? uiColAccent
                    : done && won == slot ? uiColGreen
                    : uiMix(uiColPanel, uiColDim, 50));
            uiBall(screen, box.x + 9, box.y + 9, 3, 1, lost ? uiColOff : c);
            char name[9];
            snprintf(name, sizeof(name), "%s", slotName(slot));
            uiText(screen, FONT_BROADCAST, name, lost ? uiColOff : c,
                   box.x + 18, box.y + 5, UI_LEFT);
        }
    }
    if (tourChampion() >= 0)
        uiText(screen, FONT_MENU, "CHAMPION", uiColAccent, screen->w - 80, 2,
               UI_CENTER);
}

static void displayTour(void)
{
    drawBracket();

    useBottomView();
    clearBackground(screen);
    touchReset();

    SDL_Rect go = {16, 110, 288, 70};
    touchAdd(go, 0, TOUCH_ROW);
    if (tourChampion() >= 0) {
        uiText(screen, FONT_MENU, "THE CHAMPION IS", uiColDim, 160, 30,
               UI_CENTER);
        uiText(screen, FONT_MENU, slotName(tourChampion()),
               slotColor(tourChampion()), 160, 62, UI_CENTER);
        uiTile(screen, go, "DONE", NULL, uiIconHome, uiColRed, true, true);
    } else {
        char line[2][12];
        for (int k = 0; k < 2; ++k)
            snprintf(line[k], sizeof(line[k]), "%s", slotName(fight[k]));
        uiText(screen, FONT_BROADCAST, "NEXT DUEL", uiColDim, 160, 14,
               UI_CENTER);
        uiBall(screen, 70, 62, 8, 2, slotColor(fight[0]));
        uiBall(screen, 250, 62, 8, 2, slotColor(fight[1]));
        uiText(screen, FONT_MENU, line[0], slotColor(fight[0]), 70, 90,
               UI_CENTER);
        uiText(screen, FONT_MENU, line[1], slotColor(fight[1]), 250, 90,
               UI_CENTER);
        uiText(screen, FONT_MENU, "VS", uiColAccent, 160, 54, UI_CENTER);
        go.y = 124;
        go.h = 56;
        uiTile(screen, go, "FIGHT", NULL, uiIconPlay, uiColGreen, true, true);
    }

    struct uiHint hints[] = {{"A", tourChampion() >= 0 ? "DONE" : "FIGHT", 5},
                             {"B", "LEAVE"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchTour(int id, int value, bool released)
{
    (void)id;
    (void)value;
    if (released)
        touchPressA();
}

void tournamentInit(struct scene *parent)
{
    tourScene = (struct scene){logicTour, displayTour, parent, touchTour,
                               NULL, tournamentStop};
}

#endif /* __3DS__ */
