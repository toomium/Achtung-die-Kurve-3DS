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
void drawTitleScreen(void);

#define DUELS_TO_WIN    3       /* Points a duel is played to */

struct scene tourScene;

static bool active = false;
static struct {
    unsigned scorecap;
    bool duelmode;
} saved;                        /* The rules of the lineup */
static int fight[2];            /* The slots of the next duel */

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
    playSound(SOUND_BEEP, sound);
    curScene = &tourScene;
    return true;
}

void tournamentDuelOver(int winner)
{
    tourDuelDone(winner);
    nextFight();
    curScene = &tourScene;
}

static int logicTour(void)
{
    if (enterButtonDown()) {
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
extern struct player *slotConfig(int i);

static const char *slotName(int slot)
{
    return slotConfig(slot)->name;
}

static SDL_Color slotColor(int slot)
{
    return colors[slotConfig(slot)->color];
}

/* The bracket on the top screen: a column per round, winners in green. */
static void drawBracket(void)
{
    useTopView();
    clearBackground(screen);

    const int colW = 100, entryW = 94, entryH = 18, top = 14, height = 212;
    int ys[TOUR_MAX_ROUNDS][TOUR_MAX_PLAYERS];

    for (int r = 0; r < tourRounds(); ++r) {
        const struct tourRound *R = tourRoundAt(r);
        for (int i = 0; i < R->n; ++i) {
            if (r == 0)
                ys[r][i] = top + (2 * i + 1) * height / (2 * R->n) - entryH / 2;
            else
                ys[r][i] = (ys[r - 1][2 * i]
                            + ys[r - 1][2 * i + 1 < tourRoundAt(r - 1)->n
                                        ? 2 * i + 1 : 2 * i]) / 2;
        }
    }

    for (int r = 0; r < tourRounds(); ++r) {
        const struct tourRound *R = tourRoundAt(r);
        for (int i = 0; i < R->n; ++i) {
            SDL_Rect box = {6 + r * colW, ys[r][i], entryW, entryH};
            int slot = R->slot[i];
            int won = R->winner[i / 2];     /* The result of this pair */
            bool champ = R->n == 1;
            bool played = !champ && won >= 0;
            SDL_Color c = slotColor(slot);
            bool lost = played && won != slot;
            uiPanel(screen, box, uiColPanel,
                    champ ? uiColAccent
                    : played && won == slot ? uiColGreen
                    : uiMix(uiColPanel, uiColDim, 50));
            uiBall(screen, box.x + 9, box.y + 9, 3, 1, lost ? uiColOff : c);
            char name[9];
            snprintf(name, sizeof(name), "%s", slotName(slot));
            uiText(screen, FONT_BROADCAST, name, lost ? uiColOff : c,
                   box.x + 18, box.y + 5, UI_LEFT);
        }
    }
    if (tourChampion() >= 0)
        uiText(screen, FONT_MENU, "CHAMPION", uiColAccent, screen->w - 62, 4,
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
