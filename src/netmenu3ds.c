#ifdef __3DS__

#include "netmenu3ds.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "gfx.h"
#include "input.h"
#include "net.h"
#include "player.h"
#include "sound.h"
#include "touch3ds.h"
#include "ui3ds.h"
#include "video.h"

/* In zatackax.c: the logo over the animation, on the top screen. */
void drawTitleScreen(void);
void animTitle(void);
void setNextName(unsigned char pedit);
void setNextKey(unsigned char pedit, unsigned char key);

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

/* What the lobby knows about each node (index node - 1). The host decides, the
 * clients show what the host sends. */
struct member {
    unsigned char used;
    unsigned char color;
    char name[PLAYER_NAME_LEN];
};

enum { MSG_PROFILE = 1, MSG_LOBBY = 2 };

/* A client tells the host how it wants to appear. */
struct msgProfile {
    unsigned char type;
    unsigned char color;
    char name[PLAYER_NAME_LEN];
};

/* The host tells everyone who is in the group. Sent whenever something changes
 * and now and then anyway, so a lost message does no harm. */
struct msgLobby {
    unsigned char type;
    struct member members[NET_MAX_NODES];
};

#define RESEND_MS 1000

enum { ROW_NAME, ROW_COLOR, ROW_LEFT, ROW_RIGHT, N_ROWS };
#define TOUCH_SWATCH 10

static struct member members[NET_MAX_NODES];
static unsigned lobbyMask = 0;
static bool lobbyFresh = true;
static bool lobbyDirty = false;         /* Something to send to the others */
static Uint32 lastSend = 0;
static int lobbyRow = 0;
static int wantColor = 0;               /* The color this console asks for */
static int colorCursor = 0;

/* The profile of this console: name and buttons are those of slot 1. */
static struct player *me(void)
{
    return &players[0];
}

static bool colorTaken(int color, int exceptNode)
{
    for (int n = 1; n <= NET_MAX_NODES; ++n) {
        if (n != exceptNode && members[n - 1].used
                && members[n - 1].color == color)
            return true;
    }
    return false;
}

static int freeColor(void)
{
    for (int c = 0; c < N_COLORS; ++c) {
        if (!colorTaken(c, 0))
            return c;
    }
    return 0;
}

/* Host: takes over a node's profile. A color that someone else has is refused
 * (the node keeps its old one). */
static void applyProfile(int node, int color, const char *name)
{
    struct member *m = &members[node - 1];

    if (color >= 0 && color < N_COLORS && !colorTaken(color, node))
        m->color = color;
    if (name != NULL && strncmp(m->name, name, PLAYER_NAME_LEN - 1) != 0) {
        snprintf(m->name, sizeof(m->name), "%s", name);
    }
    m->used = 1;
    lobbyDirty = true;
}

/* Host: who is in the group has changed. */
static void hostMembers(void)
{
    unsigned mask = netNodeMask();
    for (int n = 1; n <= NET_MAX_NODES; ++n) {
        struct member *m = &members[n - 1];
        bool present = (mask >> (n - 1)) & 1;
        if (present && !m->used) {
            m->used = 1;
            m->color = freeColor();
            netNodeName(n, m->name, sizeof(m->name));
            lobbyDirty = true;
        } else if (!present && m->used) {
            memset(m, 0, sizeof(*m));
            lobbyDirty = true;
        }
    }
}

static void sendLobby(void)
{
    struct msgLobby msg;
    msg.type = MSG_LOBBY;
    memcpy(msg.members, members, sizeof(members));
    netSend(NET_ALL_NODES, &msg, sizeof(msg));
}

static void sendProfile(void)
{
    struct msgProfile msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_PROFILE;
    msg.color = wantColor;
    strncpy(msg.name, me()->name, PLAYER_NAME_LEN - 1);
    netSend(NET_HOST_NODE, &msg, sizeof(msg));
}

/* This console changed its profile: the host applies it, a client sends it. */
static void profileChanged(void)
{
    if (netIsHost()) {
        applyProfile(NET_HOST_NODE, wantColor, me()->name);
        wantColor = members[NET_HOST_NODE - 1].color;
    } else {
        sendProfile();
        lastSend = SDL_GetTicks();
    }
}

static void initLobby(void)
{
    lobbyFresh = false;
    lobbyDirty = false;
    lobbyRow = 0;
    memset(members, 0, sizeof(members));
    wantColor = me()->color % N_COLORS;
    colorCursor = wantColor;
    lastSend = 0;
    if (netIsHost()) {
        hostMembers();
        profileChanged();
    } else {
        sendProfile();
    }
}

/* Handles the messages that came in. Returns true if the lobby changed. */
static bool receiveLobby(void)
{
    unsigned char buf[NET_MAX_PACKET];
    bool changed = false;
    int from = 0;
    size_t len;

    while ((len = netRecv(buf, sizeof(buf), &from)) > 0) {
        if (netIsHost() && buf[0] == MSG_PROFILE && len >= sizeof(struct msgProfile)
                && from >= 1 && from <= NET_MAX_NODES) {
            struct msgProfile msg;
            memcpy(&msg, buf, sizeof(msg));
            msg.name[PLAYER_NAME_LEN - 1] = '\0';
            if (members[from - 1].used) {
                struct member before = members[from - 1];
                applyProfile(from, msg.color, msg.name);
                changed |= memcmp(&before, &members[from - 1],
                                  sizeof(before)) != 0;
            }
        } else if (!netIsHost() && buf[0] == MSG_LOBBY
                   && len >= sizeof(struct msgLobby) && from == NET_HOST_NODE) {
            struct msgLobby msg;
            memcpy(&msg, buf, sizeof(msg));
            if (memcmp(msg.members, members, sizeof(members)) != 0) {
                memcpy(members, msg.members, sizeof(members));
                changed = true;
            }
        }
    }
    return changed;
}

static void requestColor(int color)
{
    int node = netMyNode();
    if (node < 1 || node > NET_MAX_NODES)
        return;
    if (color == wantColor && members[node - 1].color == color)
        return;
    if (colorTaken(color, node)) {
        playSound(SOUND_BEP, sound);
        return;
    }
    playSound(SOUND_BEEP, sound);
    wantColor = color;
    profileChanged();
}

static void lobbyActivate(void)
{
    switch (lobbyRow) {
    case ROW_NAME:
        playSound(SOUND_BEEP, sound);
        setNextName(0);
        profileChanged();
        break;
    case ROW_COLOR:
        requestColor(colorCursor);
        break;
    case ROW_LEFT:
    case ROW_RIGHT:
        playSound(SOUND_BEEP, sound);
        if (lobbyRow == ROW_LEFT)
            me()->lkey = SDLK_CLEAR;
        else
            me()->rkey = SDLK_CLEAR;
        lobbyScene.displayFunc();       /* Show that a button is awaited */
        setNextKey(0, lobbyRow == ROW_LEFT ? 'l' : 'r');
        break;
    }
}

static int logicLobby(void)
{
    if (!netConnected()) {
        netLeave();
        lobbyFresh = true;
        message = "CONNECTION LOST";
        playSound(SOUND_BEP, sound);
        curScene = &choiceScene;
        return 1;
    }

    bool changed = false;
    if (lobbyFresh) {
        initLobby();
        changed = true;
    }

    changed |= receiveLobby();

    if (netIsHost()) {
        unsigned mask = netNodeMask();
        if (mask != lobbyMask) {
            lobbyMask = mask;
            hostMembers();
            playSound(SOUND_BEEP, sound);
            changed = true;
        }
        if (lobbyDirty || SDL_GetTicks() - lastSend > RESEND_MS) {
            sendLobby();
            lobbyDirty = false;
            lastSend = SDL_GetTicks();
        }
    } else {
        lobbyMask = netNodeMask();
        if (SDL_GetTicks() - lastSend > RESEND_MS) {
            sendProfile();
            lastSend = SDL_GetTicks();
        }
    }

    if (enterButtonDown()) {
        clearEnterButtons();
        lobbyActivate();
        return 1;
    }

    int oldRow = lobbyRow, oldCursor = colorCursor;
    if (menuButtonQuery(KEY_DOWN) && lobbyRow < N_ROWS - 1)
        ++lobbyRow;
    else if (menuButtonQuery(KEY_UP) && lobbyRow > 0)
        --lobbyRow;
    else if (lobbyRow == ROW_COLOR && menuButtonQuery(KEY_RIGHT))
        colorCursor = (colorCursor + 1) % N_COLORS;
    else if (lobbyRow == ROW_COLOR && menuButtonQuery(KEY_LEFT))
        colorCursor = (colorCursor + N_COLORS - 1) % N_COLORS;
    if (lobbyRow != oldRow || colorCursor != oldCursor) {
        playSound(SOUND_BEEP, sound);
        changed = true;
    }
    return changed;
}

static void leaveLobby(void)
{
    netLeave();
    lobbyFresh = true;
    lobbyMask = 0;
    message = NULL;
}

static void drawLobbyTop(void)
{
    const int w = 88, h = 100, gap = 8, x0 = 12, y0 = 14;
    int mine = netMyNode();
    int slot = 0;

    useTopView();
    clearBackground(screen);

    for (int n = 1; n <= NET_MAX_NODES; ++n) {
        const struct member *m = &members[n - 1];
        if (!m->used)
            continue;

        SDL_Color c = colors[m->color % N_COLORS];
        SDL_Rect cell = {x0 + (slot % 4) * (w + gap),
                         y0 + (slot / 4) * (h + gap), w, h};
        cell = uiCard(screen, cell, uiColPanel,
                      n == mine ? uiColAccent : uiMix(uiColPanel, c, 55),
                      n == mine);
        int cx = cell.x + w / 2;
        uiBall(screen, cx, cell.y + 34, 8, 2, c);

        char name[11];
        snprintf(name, sizeof(name), "%s", m->name);
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

/* A row of the profile: a panel with a label; the caller draws the value. */
static SDL_Rect profileRow(int y, int h, int row, const char *label)
{
    SDL_Rect r = {16, y, 288, h};
    bool focused = row == lobbyRow;
    touchAdd(r, row, TOUCH_ROW);
    uiPanel(screen, r, uiColPanel,
            focused ? uiColAccent : uiMix(uiColPanel, uiColDim, 50));
    uiText(screen, FONT_BROADCAST, label, uiColDim, r.x + 10,
           r.y + (h - 8) / 2, UI_LEFT);
    return r;
}

static void displayLobby(void)
{
    drawLobbyTop();

    useBottomView();
    clearBackground(screen);
    touchReset();

    int mine = netMyNode();
    int shownColor = mine >= 1 && mine <= NET_MAX_NODES && members[mine - 1].used
        ? members[mine - 1].color : wantColor;

    SDL_Rect r = profileRow(8, 26, ROW_NAME, "NAME");
    uiText(screen, FONT_MENU, me()->name, colors[shownColor % N_COLORS],
           r.x + r.w - 10, r.y + 5, UI_RIGHT);

    /* The colors in a row. Those of the others are dimmed. */
    r = profileRow(40, 36, ROW_COLOR, "COLOR");
    const int sw = 24, sgap = 3;
    int x0 = r.x + r.w - 10 - (N_COLORS * sw + (N_COLORS - 1) * sgap);
    for (int i = 0; i < N_COLORS; ++i) {
        SDL_Rect s = {x0 + i * (sw + sgap), r.y + 6, sw, sw};
        bool taken = colorTaken(i, mine);
        bool cursor = lobbyRow == ROW_COLOR && i == colorCursor;
        SDL_Color c = colors[i];
        touchAdd(s, TOUCH_SWATCH + i, 0);
        uiPanel(screen, s, taken ? uiMix(uiColPanel, c, 25) : c,
                i == shownColor ? uiColText
                : cursor ? uiColAccent : uiMix(uiColPanel, c, 50));
    }

    for (int row = ROW_LEFT; row <= ROW_RIGHT; ++row) {
        bool left = row == ROW_LEFT;
        r = profileRow(row == ROW_LEFT ? 80 : 110, 26, row,
                       left ? "LEFT" : "RIGHT");
        char *text = buttonName(left ? me()->lkey : me()->rkey);
        uiText(screen, FONT_MENU, text[0] != '\0' ? text : "...", uiColText,
               r.x + r.w - 10, r.y + 5, UI_RIGHT);
        free(text);
    }

    int members_n = 0;
    for (int n = 0; n < NET_MAX_NODES; ++n)
        members_n += members[n].used;
    char line[40];
    snprintf(line, sizeof(line), "%d / %d  %s", members_n, NET_MAX_NODES,
             netIsHost() ? "WAITING FOR PLAYERS" : "WAITING FOR THE HOST");
    uiText(screen, FONT_BROADCAST, line, uiColDim, 160, 150, UI_CENTER);

    struct uiHint hints[] = {{"A", lobbyRow == ROW_COLOR ? "PICK" : "CHANGE", 6},
                             {"B", "LEAVE"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchLobby(int id, int value, bool released)
{
    (void)value;
    if (id >= TOUCH_SWATCH) {
        lobbyRow = ROW_COLOR;
        colorCursor = id - TOUCH_SWATCH;
        if (released)
            requestColor(colorCursor);
    } else if (!released) {
        lobbyRow = id;
    } else {
        touchPressA();
    }
}

/* ---- Setup ---- */

void netMenuEnter(struct scene *parent)
{
    choiceScene = (struct scene){logicChoice, displayChoice, parent,
                                 touchChoice, animTitle, leaveChoice};
    browseScene = (struct scene){logicBrowse, displayBrowse, &choiceScene,
                                 touchBrowse, animTitle, NULL};
    lobbyScene = (struct scene){logicLobby, displayLobby, &choiceScene,
                                touchLobby, NULL, leaveLobby};

    message = NULL;
    choiceSel = 0;
    lobbyFresh = true;
    lobbyMask = 0;
    if (!netInit()) {
        fprintf(stderr, "net: init failed 0x%08x\n", netLastError());
        message = "NETWORK NOT AVAILABLE";
    }
    curScene = &choiceScene;
}

#endif /* __3DS__ */
