#ifdef __3DS__

#include "slotmenu3ds.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "config.h"
#include "gfx.h"
#include "input.h"
#include "player.h"
#include "shop3ds.h"
#include "sound.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

#define MAX_STYLES      16

static struct scene trailScene;
static struct scene headScene;
static struct scene controlsScene;

static int slot = 0;            /* The slot (index into players[]) */
static int sel = 0;             /* The cursor on the page */

/* ---- Trail: the color ---- */

#define COLOR_COLS  8

static const char *notice = NULL;       /* Shown in red under the colors */

static SDL_Rect colorRect(int i)
{
    return (SDL_Rect){16 + (i % COLOR_COLS) * 36, 42 + (i / COLOR_COLS) * 42,
                      32, 32};
}

static int logicTrail(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        if (!shopColorOwned(sel)) {
            notice = "BUY THIS COLOR IN THE SHOP";
            playSound(SOUND_BEP, sound);
        } else if (slotColorTaken(slot, sel)) {
            notice = "ANOTHER PLAYER HAS THIS COLOR";
            playSound(SOUND_BEP, sound);
        } else {
            notice = NULL;
            setSlotColor(slot, sel);
            playSound(SOUND_BEEP, sound);
        }
        return 1;
    }

    int old = sel;
    if (menuButtonQuery(KEY_RIGHT))
        sel = (sel + 1) % N_COLORS;
    else if (menuButtonQuery(KEY_LEFT))
        sel = (sel + N_COLORS - 1) % N_COLORS;
    else if (menuButtonQuery(KEY_DOWN) && sel + COLOR_COLS < N_COLORS)
        sel += COLOR_COLS;
    else if (menuButtonQuery(KEY_UP) && sel >= COLOR_COLS)
        sel -= COLOR_COLS;

    if (sel != old) {
        notice = NULL;
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static void displayTrail(void)
{
    struct player *p = &players[slot];

    drawLobbyTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    char title[32];
    snprintf(title, sizeof(title), "TRAIL OF SLOT %d", slot + 1);
    uiText(screen, FONT_BROADCAST, title, colors[p->color], 16, 10, UI_LEFT);
    uiText(screen, FONT_BROADCAST, colorName(sel), colors[sel], 304, 10,
           UI_RIGHT);

    for (int i = 0; i < N_COLORS; ++i) {
        SDL_Rect r = colorRect(i);
        touchAdd(r, i, TOUCH_ROW);
        bool unusable = slotColorTaken(slot, i) || !shopColorOwned(i);
        SDL_Color c = colors[i];
        SDL_Rect in = uiCard(screen, r, unusable ? uiMix(uiColPanel, c, 25) : c,
                             i == p->color ? uiColText
                             : i == sel ? uiColAccent : uiMix(uiColPanel, c, 50),
                             i == sel);
        if (!shopColorOwned(i))
            uiIcon(screen, uiIconLock, in.x + 8, in.y + 8, 2, uiColText);
    }

    /* A piece of trail as it will look. */
    for (int k = 0; k < 270; ++k) {
        int y = 190 - (int)(sin(k / 16.0) * 10);
        SDL_Rect px = {24 + k - 1, y - 1, 3, 3};
        SDL_FillRect(screen, &px, uiMap(screen, colors[p->color]));
    }
    if (notice != NULL)
        uiText(screen, FONT_BROADCAST, notice, uiColRed, 160, 170, UI_CENTER);

    struct uiHint hints[] = {{"A", "PICK"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchTrail(int id, int value, bool released)
{
    (void)value;
    if (!released)
        sel = id;
    else
        touchPressA();
}

/* ---- Head ---- */

static int logicHead(void)
{
    unsigned char list[MAX_STYLES];
    int n = shopOwnedHeads(list, MAX_STYLES);

    if (enterButtonDown()) {
        clearEnterButtons();
        if (sel < n) {
            players[slot].headStyle = list[sel];
            playSound(SOUND_BEEP, sound);
        }
        return 1;
    }

    int old = sel;
    if (menuButtonQuery(KEY_RIGHT)) {
        if (sel < n - 1)
            ++sel;
    } else if (menuButtonQuery(KEY_LEFT)) {
        if (sel > 0)
            --sel;
    } else if (menuButtonQuery(KEY_UP)) {
        if (sel >= 4)
            sel -= 4;
    } else if (menuButtonQuery(KEY_DOWN)) {
        if (sel + 4 < n)
            sel += 4;
    }

    if (sel != old) {
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static void displayHead(void)
{
    struct player *p = &players[slot];
    unsigned char list[MAX_STYLES];
    int n = shopOwnedHeads(list, MAX_STYLES);

    drawLobbyTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    char title[32];
    snprintf(title, sizeof(title), "HEAD OF SLOT %d", slot + 1);
    uiText(screen, FONT_BROADCAST, title, colors[p->color], 16, 10, UI_LEFT);

    for (int i = 0; i < n; ++i) {
        SDL_Rect r = {16 + (i % 4) * 73, 30 + (i / 4) * 56, 70, 50};
        touchAdd(r, i, TOUCH_ROW);
        bool cursor = i == sel;
        SDL_Rect in = uiCard(screen, r, uiColPanel,
                             list[i] == p->headStyle ? uiColGreen
                             : cursor ? uiColAccent : uiColDim, cursor);
        shopDrawHeadIcon(screen, list[i], in.x + in.w / 2, in.y + 17, 3,
                         colors[p->color]);
        uiText(screen, FONT_BROADCAST, shopHeadName(list[i]),
               list[i] == p->headStyle ? uiColGreen : uiColText,
               in.x + in.w / 2, in.y + 36, UI_CENTER);
    }
    if (n == 1)
        uiText(screen, FONT_BROADCAST, "MORE HEADS ARE IN THE SHOP", uiColDim,
               16, 90, UI_LEFT);

    struct uiHint hints[] = {{"A", "PICK"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchHead(int id, int value, bool released)
{
    (void)value;
    if (!released)
        sel = id;
    else
        touchPressA();
}

/* ---- Controls ---- */

static const char *const controlName[3] = {"LEFT", "RIGHT", "WEAPON"};
static const char controlKey[3] = {'l', 'r', 'w'};

static void displayControls(void);

static int logicControls(void)
{
    struct player *p = &players[slot];

    if (enterButtonDown()) {
        clearEnterButtons();
        playSound(SOUND_BEEP, sound);
        if (sel == 0)
            p->lkey = SDLK_CLEAR;
        else if (sel == 1)
            p->rkey = SDLK_CLEAR;
        else
            p->wkey = SDLK_CLEAR;
        displayControls();      /* Show that a button is awaited */
        setNextKey(slot, controlKey[sel]);
        return 1;
    }

    int old = sel;
    if (menuButtonQuery(KEY_DOWN) && sel < 2)
        ++sel;
    else if (menuButtonQuery(KEY_UP) && sel > 0)
        --sel;
    if (sel != old) {
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static void displayControls(void)
{
    struct player *p = &players[slot];

    drawLobbyTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    char title[32];
    snprintf(title, sizeof(title), "CONTROLS OF SLOT %d", slot + 1);
    uiText(screen, FONT_BROADCAST, title, colors[p->color], 16, 10, UI_LEFT);

    for (int i = 0; i < 3; ++i) {
        SDL_Rect r = lobbyRowPanel(34 + i * 30, i == sel, controlName[i]);
        touchAdd(r, i, TOUCH_ROW);
        char *text = buttonName(i == 0 ? p->lkey : i == 1 ? p->rkey : p->wkey);
        uiText(screen, FONT_MENU, text[0] != '\0' ? text : "...", uiColText,
               r.x + r.w - 10, r.y + (r.h - 16) / 2, UI_RIGHT);
        free(text);
    }
    uiText(screen, FONT_BROADCAST, "PRESS A, THEN THE NEW BUTTON", uiColDim, 160,
           136, UI_CENTER);

    struct uiHint hints[] = {{"A", "BIND"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchControls(int id, int value, bool released)
{
    (void)value;
    if (!released)
        sel = id;
    else
        touchPressA();
}

/* ---- Setup ---- */

void slotMenusInit(struct scene *lineup)
{
    trailScene = (struct scene){logicTrail, displayTrail, lineup, touchTrail,
                                NULL, NULL};
    headScene = (struct scene){logicHead, displayHead, lineup, touchHead,
                               NULL, NULL};
    controlsScene = (struct scene){logicControls, displayControls, lineup,
                                   touchControls, NULL, NULL};
}

void slotMenuOpen(int menu, int s)
{
    slot = s;
    sel = 0;
    if (menu == SLOTMENU_TRAIL) {
        sel = players[slot].color;
        notice = NULL;
        curScene = &trailScene;
    } else if (menu == SLOTMENU_HEAD) {
        unsigned char list[MAX_STYLES];
        int n = shopOwnedHeads(list, MAX_STYLES);
        for (int i = 0; i < n; ++i) {
            if (list[i] == players[slot].headStyle)
                sel = i;
        }
        curScene = &headScene;
    } else {
        curScene = &controlsScene;
    }
}

#endif /* __3DS__ */
