#ifdef __3DS__

#include "profile3ds.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfx.h"
#include "input.h"
#include "slotmenu3ds.h"
#include "sound.h"
#include "stats3ds.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

static struct scene profileScene;

static int logicProfile(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        playSound(SOUND_BEEP, sound);
        curScene = profileScene.parentScene;
        return 1;
    }
    return 0;
}

static void drawProfileTop(void)
{
    useTopView();
    clearBackground(screen);

    int now = statsTitleNow();
    int cx = screen->w / 2;

    uiText(screen, FONT_MENU, "PROFILE", uiColBlue, 16, 10, UI_LEFT);
    uiText(screen, FONT_MENU, statsTitleName(now), uiColAccent, cx, 40, UI_CENTER);

    /* The way to the next title. */
    SDL_Rect track = {cx - 140, 74, 280, 12};
    uiPanel(screen, track, uiColPanel, uiMix(uiColPanel, uiColDim, 50));
    char line[48];
    if (now + 1 < statsTitleCount()) {
        unsigned from = statsTitleWins(now), to = statsTitleWins(now + 1);
        SDL_Rect fill = {track.x + 2, track.y + 2,
                         (track.w - 4) * (stats.roundWins - from) / (to - from),
                         track.h - 4};
        SDL_FillRect(screen, &fill, uiMap(screen, uiColAccent));
        snprintf(line, sizeof(line), "%u OF %u ROUNDS TO %s", stats.roundWins, to,
                 statsTitleName(now + 1));
    } else {
        snprintf(line, sizeof(line), "THE BEST TITLE, %u ROUNDS WON",
                 stats.roundWins);
    }
    uiText(screen, FONT_BROADCAST, line, uiColDim, cx, 94, UI_CENTER);

    /* All the titles: reached ones in green, the one one has in yellow. */
    for (int i = 0; i < statsTitleCount(); ++i) {
        SDL_Rect r = {cx - 140 + (i % 2) * 144, 124 + (i / 2) * 32, 136, 26};
        bool reached = stats.roundWins >= statsTitleWins(i);
        SDL_Color c = i == now ? uiColAccent : reached ? uiColGreen : uiColOff;
        uiPanel(screen, r, uiColPanel, uiMix(uiColPanel, c, 60));
        uiText(screen, FONT_BROADCAST, statsTitleName(i), c, r.x + 8, r.y + 9,
               UI_LEFT);
        snprintf(line, sizeof(line), "%u", statsTitleWins(i));
        uiText(screen, FONT_BROADCAST, line, c, r.x + r.w - 8, r.y + 9,
               UI_RIGHT);
    }
}

static void displayProfile(void)
{
    drawProfileTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    SDL_Rect everywhere = {0, 0, screen->w, screen->h - 20};
    touchAdd(everywhere, 0, TOUCH_ROW);

    static const char *const label[5] = {
        "ROUNDS PLAYED", "ROUNDS WON", "MATCHES PLAYED", "MATCHES WON",
        "BEST STREAK"
    };
    const unsigned value[5] = {stats.rounds, stats.roundWins, stats.matches,
                               stats.matchWins, stats.bestStreak};
    for (int i = 0; i < 5; ++i) {
        SDL_Rect r = lobbyRowPanel(12 + i * 32, false, label[i]);
        char number[12];
        snprintf(number, sizeof(number), "%u", value[i]);
        uiText(screen, FONT_MENU, number, uiColText, r.x + r.w - 10,
               r.y + (r.h - 16) / 2, UI_RIGHT);
    }

    struct uiHint hints[] = {{"B", "BACK"}};
    uiHintBar(screen, hints, 1);

    if (presentFrame() == -1)
        exit(1);
}

static void touchProfile(int id, int value, bool released)
{
    (void)id;
    (void)value;
    if (released)
        touchPressA();
}

void profileEnter(struct scene *parent)
{
    profileScene = (struct scene){logicProfile, displayProfile, parent,
                                  touchProfile, NULL, NULL};
    curScene = &profileScene;
}


#endif /* __3DS__ */
