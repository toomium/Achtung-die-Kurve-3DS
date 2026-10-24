#ifdef __3DS__

#include "netmenu3ds.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "gfx.h"
#include "input.h"
#include "net.h"
#include "sound.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

/* In zatackax.c: the logo over the animation, on the top screen. */
void drawTitleScreen(void);

#define MAX_LISTED  6           /* Groups shown in the list */
#define TOUCH_REFRESH 100

static struct scene choiceScene;
static struct scene browseScene;
static struct scene lobbyScene;

static const char *message = NULL;      /* Shown in red when something failed */

/* ---- Helpers ---- */

/* Presses the A button for the page, as touch does elsewhere. */
static void touchPressA(void)
{
    joyButtonDown[0][N3DS_BUTTON_A] = true;
}

static void showMessage(const char *text)
{
    message = text;
    playSound(SOUND_BEP, sound);
}

static void drawMessage(int y)
{
    if (message != NULL)
        uiText(screen, FONT_BROADCAST, message, uiColRed, 160, y, UI_CENTER);
}

/* ---- Create or join ---- */

static int choiceSel = 0;

static void startHosting(void)
{
    if (!netInit()) {
        fprintf(stderr, "net: init failed 0x%08x\n", netLastError());
        showMessage("NETWORK NOT AVAILABLE");
        return;
    }

    /* What searchers can see of the group. */
    const unsigned char info[] = {'Z', 'X', 1};
    if (!netHost(info, sizeof(info), NET_MAX_NODES)) {
        fprintf(stderr, "net: host failed 0x%08x\n", netLastError());
        showMessage("COULD NOT CREATE A GROUP");
        return;
    }
    playSound(SOUND_BEEP, sound);
    curScene = &lobbyScene;
}

static struct netGroup groups[MAX_LISTED];
static int nGroups = 0;
static int browseSel = 0;
static bool searching = false;
static bool wantRescan = false;

static void enterBrowse(void)
{
    if (!netInit()) {
        fprintf(stderr, "net: init failed 0x%08x\n", netLastError());
        showMessage("NETWORK NOT AVAILABLE");
        return;
    }
    playSound(SOUND_BEEP, sound);
    nGroups = 0;
    browseSel = 0;
    wantRescan = true;
    curScene = &browseScene;
}

static int logicChoice(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        message = NULL;
        if (choiceSel == 0)
            startHosting();
        else
            enterBrowse();
        return 1;
    }

    if (menuButtonQuery(KEY_DOWN) || menuButtonQuery(KEY_UP)) {
        choiceSel = 1 - choiceSel;
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static void displayChoice(void)
{
    drawTitleScreen();
    useBottomView();
    clearBackground(screen);
    touchReset();

    SDL_Rect create = {16, 20, 288, 80};
    SDL_Rect join = {16, 112, 288, 80};
    touchAdd(create, 0, TOUCH_ROW);
    touchAdd(join, 1, TOUCH_ROW);

    uiTile(screen, create, "CREATE GROUP", NULL, uiIconManyConsoles, uiColGreen,
           choiceSel == 0, true);
    uiTile(screen, join, "JOIN GROUP", NULL, uiIconManyConsoles, uiColBlue,
           choiceSel == 1, true);
    drawMessage(200);

    struct uiHint hints[] = {{"A", "OK"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchChoice(int id, int value, bool released)
{
    (void)value;
    if (!released)
        choiceSel = id;
    else
        touchPressA();
}

static void leaveChoice(void)
{
    netExit();
}

/* ---- The list of groups ---- */

static int logicBrowse(void)
{
    if (wantRescan) {
        wantRescan = false;
        searching = true;
        browseScene.displayFunc();      /* Show "SEARCHING" first */
        nGroups = netScan(groups, MAX_LISTED);
        searching = false;
        if (browseSel >= nGroups)
            browseSel = 0;
        fprintf(stderr, "net: found %d groups\n", nGroups);
        return 1;
    }

    if (enterButtonDown()) {
        clearEnterButtons();
        message = NULL;
        if (nGroups == 0) {
            wantRescan = true;
        } else if (netJoin(browseSel)) {
            playSound(SOUND_BEEP, sound);
            curScene = &lobbyScene;
        } else {
            fprintf(stderr, "net: join failed 0x%08x\n", netLastError());
            showMessage("COULD NOT JOIN");
            wantRescan = true;
        }
        return 1;
    }

    if (joyButtonDown[0][N3DS_BUTTON_X]) {
        joyButtonDown[0][N3DS_BUTTON_X] = false;
        wantRescan = true;
        return 0;
    }

    int old = browseSel;
    if (menuButtonQuery(KEY_DOWN) && browseSel < nGroups - 1)
        ++browseSel;
    else if (menuButtonQuery(KEY_UP) && browseSel > 0)
        --browseSel;
    if (browseSel != old) {
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

static void displayBrowse(void)
{
    drawTitleScreen();
    useBottomView();
    clearBackground(screen);
    touchReset();

    uiText(screen, FONT_MENU, "GROUPS", uiColBlue, 16, 8, UI_LEFT);

    if (searching) {
        uiText(screen, FONT_MENU, "SEARCHING...", uiColDim, 160, 100, UI_CENTER);
    } else if (nGroups == 0) {
        uiText(screen, FONT_MENU, "NO GROUPS FOUND", uiColDim, 160, 90,
               UI_CENTER);
        uiText(screen, FONT_BROADCAST, "A: SEARCH AGAIN", uiColDim, 160, 120,
               UI_CENTER);
    }

    for (int i = 0; i < nGroups; ++i) {
        SDL_Rect r = {16, 34 + i * 30, 288, 26};
        bool sel = i == browseSel;
        touchAdd(r, i, TOUCH_ROW);
        uiPanel(screen, r, uiColPanel,
                sel ? uiColAccent : uiMix(uiColPanel, uiColDim, 50));
        const char *host = groups[i].hostName[0] != '\0' ? groups[i].hostName
                                                         : "?";
        uiText(screen, FONT_MENU, host, sel ? uiColText : uiColDim, r.x + 10,
               r.y + 5, UI_LEFT);
        char count[8];
        snprintf(count, sizeof(count), "%d/%d", groups[i].nodes,
                 groups[i].maxNodes);
        uiText(screen, FONT_MENU, count, uiColGreen, r.x + r.w - 10, r.y + 5,
               UI_RIGHT);
    }
    drawMessage(206);

    SDL_Rect refresh = {212, 2, 100, 28};
    touchAdd(refresh, TOUCH_REFRESH, 0);
    uiText(screen, FONT_BROADCAST, "X: REFRESH", uiColAccent, refresh.x + 50, 12,
           UI_CENTER);

    struct uiHint hints[] = {{"A", nGroups > 0 ? "JOIN" : "SEARCH"},
                             {"X", "REFRESH"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 3);

    if (presentFrame() == -1)
        exit(1);
}

static void touchBrowse(int id, int value, bool released)
{
    (void)value;
    if (id == TOUCH_REFRESH) {
        if (released)
            wantRescan = true;
        return;
    }
    if (!released)
        browseSel = id;
    else
        touchPressA();
}

/* ---- The lobby ---- */

#define NAME_LEN (NET_NAME_MAX * 4 + 1)

static unsigned lobbyMask = 0;
static char names[NET_MAX_NODES][NAME_LEN];
static unsigned lastNameTry = 0;
static bool lobbyFresh = true;

static void refreshMembers(void)
{
    lobbyMask = netNodeMask();
    for (int n = 1; n <= NET_MAX_NODES; ++n) {
        names[n - 1][0] = '\0';
        if (lobbyMask & (1u << (n - 1)))
            netNodeName(n, names[n - 1], NAME_LEN);
    }
    lastNameTry = SDL_GetTicks();
}

static int logicLobby(void)
{
    if (!netConnected()) {
        netLeave();
        lobbyFresh = true;
        message = netIsHost() ? NULL : "CONNECTION LOST";
        playSound(SOUND_BEP, sound);
        curScene = &choiceScene;
        return 1;
    }

    if (lobbyFresh) {
        lobbyFresh = false;
        refreshMembers();
        return 1;
    }

    if (netNodeMask() != lobbyMask) {
        refreshMembers();
        playSound(SOUND_BEEP, sound);
        return 1;
    }

    /* A new member's name may arrive a moment after it joined. */
    bool missing = false;
    for (int n = 0; n < NET_MAX_NODES; ++n) {
        if ((lobbyMask & (1u << n)) && names[n][0] == '\0')
            missing = true;
    }
    if (missing && SDL_GetTicks() - lastNameTry > 500) {
        refreshMembers();
        return 1;
    }
    return 0;
}

static void leaveLobby(void)
{
    netLeave();
    lobbyFresh = true;
    message = NULL;
}

static void drawLobbyTop(void)
{
    const int w = 88, h = 100, gap = 8, x0 = 12, y0 = 14;
    int me = netMyNode();
    int slot = 0;

    useTopView();
    clearBackground(screen);

    for (int n = 1; n <= NET_MAX_NODES; ++n) {
        if (!(lobbyMask & (1u << (n - 1))))
            continue;

        SDL_Color c = colors[slot % N_COLORS];
        SDL_Rect cell = {x0 + (slot % 4) * (w + gap),
                         y0 + (slot / 4) * (h + gap), w, h};
        cell = uiCard(screen, cell, uiColPanel,
                      n == me ? uiColAccent : uiMix(uiColPanel, c, 55),
                      n == me);
        int cx = cell.x + w / 2;
        uiBall(screen, cx, cell.y + 34, 8, 2, c);

        char name[11];
        snprintf(name, sizeof(name), "%s", names[n - 1]);
        uiText(screen, FONT_BROADCAST, name[0] != '\0' ? name : "...", c, cx,
               cell.y + 60, UI_CENTER);
        uiText(screen, FONT_MENU, n == NET_HOST_NODE ? "HOST" : "",
               uiColGreen, cx, cell.y + 76, UI_CENTER);
        ++slot;
    }

    /* The places that are still free. */
    for (; slot < NET_MAX_NODES; ++slot) {
        SDL_Rect cell = {x0 + (slot % 4) * (w + gap),
                         y0 + (slot / 4) * (h + gap), w, h};
        cell = uiCard(screen, cell, uiColPanel,
                      uiMix(uiColPanel, uiColOff, 40), false);
        uiBall(screen, cell.x + w / 2, cell.y + 34, 8, 2,
               uiMix(uiColPanel, uiColOff, 60));
    }
}

static void displayLobby(void)
{
    drawLobbyTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    int members = 0;
    for (int n = 0; n < NET_MAX_NODES; ++n)
        members += (lobbyMask >> n) & 1;

    uiText(screen, FONT_MENU, netIsHost() ? "YOUR GROUP" : "GROUP", uiColBlue,
           16, 8, UI_LEFT);

    char line[32];
    snprintf(line, sizeof(line), "PLAYERS  %d / %d", members, NET_MAX_NODES);
    uiText(screen, FONT_MENU, line, uiColText, 16, 50, UI_LEFT);

    int me = netMyNode();
    if (me >= 1 && me <= NET_MAX_NODES && names[me - 1][0] != '\0') {
        snprintf(line, sizeof(line), "YOU  %s", names[me - 1]);
        uiText(screen, FONT_MENU, line, uiColDim, 16, 80, UI_LEFT);
    }

    uiText(screen, FONT_BROADCAST,
           netIsHost() ? "WAITING FOR PLAYERS..." : "WAITING FOR THE HOST...",
           uiColDim, 16, 120, UI_LEFT);

    struct uiHint hints[] = {{"B", "LEAVE"}};
    uiHintBar(screen, hints, 1);

    if (presentFrame() == -1)
        exit(1);
}

static void touchLobby(int id, int value, bool released)
{
    (void)id;
    (void)value;
    (void)released;
}

/* ---- Setup ---- */

void netMenuEnter(struct scene *parent)
{
    choiceScene = (struct scene){logicChoice, displayChoice, parent,
                                 touchChoice, NULL, leaveChoice};
    browseScene = (struct scene){logicBrowse, displayBrowse, &choiceScene,
                                 touchBrowse, NULL, NULL};
    lobbyScene = (struct scene){logicLobby, displayLobby, &choiceScene,
                                touchLobby, NULL, leaveLobby};

    message = NULL;
    choiceSel = 0;
    lobbyFresh = true;
    if (!netInit()) {
        fprintf(stderr, "net: init failed 0x%08x\n", netLastError());
        message = "NETWORK NOT AVAILABLE";
    }
    curScene = &choiceScene;
}

#endif /* __3DS__ */
