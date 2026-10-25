#ifdef __3DS__

#include "netmenu3ds.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "gfx.h"
#include "input.h"
#include "net.h"
#include "netgame3ds.h"
#include "netproto3ds.h"
#include "player.h"
#include "shop3ds.h"
#include "slotmenu3ds.h"
#include "stats3ds.h"
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
static struct scene rulesScene;
static struct scene waitScene;
static struct scene readyScene;

static const char *message = NULL;      /* Shown in red when something failed */

/* What searchers can see of a group. The last number goes up whenever a message
 * changes, so that consoles with another version stay apart. */
static const unsigned char groupInfo[] = {'Z', 'X', 3};

static bool sameVersion(const struct netGroup *g)
{
    return g->appdataSize == sizeof(groupInfo)
        && memcmp(g->appdata, groupInfo, sizeof(groupInfo)) == 0;
}

/* ---- Helpers ---- */

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
    if (!netHost(groupInfo, sizeof(groupInfo), NET_MAX_NODES)) {
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
        } else if (!sameVersion(&groups[browseSel])) {
            showMessage("THAT CONSOLE HAS ANOTHER VERSION");
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
        bool same = sameVersion(&groups[i]);
        uiText(screen, FONT_MENU, same ? count : "OLD", same ? uiColGreen : uiColRed,
               r.x + r.w - 10, r.y + 5, UI_RIGHT);
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

/* ---- The group: lobby, rules, ready ---- */

enum { PH_LOBBY, PH_RULES, PH_READY, PH_GAME };

/* A client tells the host how it wants to appear. */
struct msgProfile {
    unsigned char type;
    unsigned char color;
    unsigned char head;
    unsigned char title;
    char name[PLAYER_NAME_LEN];
};

/* The host tells everyone the whole state of the group. Sent whenever
 * something changes and now and then anyway, so a lost message does no harm. */
struct msgState {
    unsigned char type;
    unsigned char phase;
    unsigned short readyLeft;   /* Milliseconds of the READY phase that are left */
    struct netRules rules;
    struct member members[NET_MAX_NODES];
};

#define RESEND_MS    1000
#define READY_MS     30000

static const unsigned pointChoices[] = {10, 20, 30, 40, 50, 100, 0};
#define N_POINTS ((int)(sizeof(pointChoices) / sizeof(pointChoices[0])))
const int speedChoices[N_SPEEDS] = {75, 100, 125, 150};

enum { ROW_NAME, ROW_COLOR, ROW_HEAD, ROW_LEFT, ROW_RIGHT, ROW_START,
       N_ROWS };
enum { RULE_POINTS, RULE_HOLES, RULE_DUEL, RULE_SPEED, RULE_AI, RULE_GO,
       N_RULE_ROWS };
#define TOUCH_SWATCH 10

static struct member members[NET_MAX_NODES];
static struct netRules rules;
static int phase = PH_LOBBY;
static unsigned lobbyMask = 0;
static unsigned lockedMask = 0;         /* Who was there when the rules began */
static bool lobbyFresh = true;
static bool lobbyDirty = false;         /* Something to send to the others */
static Uint32 lastSend = 0;
static int lobbyRow = 0;
static int rulesRow = 0;
static int wantColor = 0;               /* The color this console asks for */
static int colorCursor = 0;
static bool wantReady = false;
static Uint32 readyDeadline = 0;        /* Host: when the READY phase ends */
static unsigned readyLeftAtRecv = 0;    /* Client: what the host said, and when */
static Uint32 readyRecvTime = 0;
static int shownSeconds = -1;

/* In zatackax.c: widgets of the rules page. */
void drawChoice(SDL_Rect row, int id, int w, int n, const char *labels[],
                int active, unsigned enabled);
void drawSwitch(SDL_Rect row, int id, bool on);

/* The profile of this console: name and buttons are those of slot 1. */
static struct player *me(void)
{
    return &players[0];
}

static int memberCount(void)
{
    int n = 0;
    for (int i = 0; i < NET_MAX_NODES; ++i)
        n += members[i].used;
    return n;
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
    for (int c = 0; c < N_BASE_COLORS; ++c) {
        if (!colorTaken(c, 0))
            return c;
    }
    return 0;
}

/* Host: takes over a node's profile. A color that someone else has is refused
 * (the node keeps its old one). */
static void applyProfile(int node, int color, const char *name,
                         unsigned char head, unsigned char title)
{
    struct member *m = &members[node - 1];

    m->head = head;
    m->title = title;
    if (color >= 0 && color < N_COLORS && !colorTaken(color, node))
        m->color = color;
    if (name != NULL && strncmp(m->name, name, PLAYER_NAME_LEN - 1) != 0)
        snprintf(m->name, sizeof(m->name), "%s", name);
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

static unsigned readyRemaining(void)
{
    return readyTimeLeft(netIsHost(), readyDeadline, readyLeftAtRecv,
                         readyRecvTime);
}

static void sendState(void)
{
    struct msgState msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_STATE;
    msg.phase = phase;
    msg.readyLeft = phase == PH_READY ? readyRemaining() : 0;
    msg.rules = rules;
    memcpy(msg.members, members, sizeof(members));
    netSend(NET_ALL_NODES, &msg, sizeof(msg));
}

static void sendProfile(void)
{
    struct msgProfile msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_PROFILE;
    msg.color = wantColor;
    msg.head = shopOwnedHead(me()->headStyle);
    msg.title = statsTitleNow();
    snprintf(msg.name, sizeof(msg.name), "%s", me()->name);
    netSend(NET_HOST_NODE, &msg, sizeof(msg));
}

static void sendReady(void)
{
    unsigned char msg = MSG_READY;
    netSend(NET_HOST_NODE, &msg, sizeof(msg));
}

/* This console changed its profile: the host applies it, a client sends it. */
static void profileChanged(void)
{
    if (netIsHost()) {
        applyProfile(NET_HOST_NODE, wantColor, me()->name,
                     shopOwnedHead(me()->headStyle), statsTitleNow());
        wantColor = members[NET_HOST_NODE - 1].color;
    } else {
        sendProfile();
        lastSend = SDL_GetTicks();
    }
}

/* Host: a new phase begins. */
static void setPhase(int next)
{
    phase = next;
    lobbyDirty = true;
    lastSend = 0;
}

static void initLobby(void)
{
    lobbyFresh = false;
    lobbyDirty = false;
    lobbyRow = 0;
    rulesRow = 0;
    phase = PH_LOBBY;
    wantReady = false;
    memset(members, 0, sizeof(members));
    wantColor = me()->color % N_COLORS;
    colorCursor = wantColor;
    lastSend = 0;
    if (netIsHost()) {
        rules.points = scorecap;
        rules.holes = holes;
        rules.duel = duelmode;
        rules.speed = 1;
        for (int i = 0; i < N_SPEEDS; ++i) {
            if (speedChoices[i] == gameSpeed)
                rules.speed = i;
        }
        rules.ai = 0;
        hostMembers();
        profileChanged();
    } else {
        sendProfile();
    }
}

/* Leaves the group for the first page. */
static void goToChoice(const char *why)
{
    netLeave();
    lobbyFresh = true;
    lobbyMask = 0;
    message = why;
    playSound(SOUND_BEP, sound);
    curScene = &choiceScene;
}

/* Host: someone left while the group was locked. Tells everyone and ends. */
static void abortAll(void)
{
    unsigned char msg = MSG_ABORT;
    for (int i = 0; i < 3; ++i) {
        netSend(NET_ALL_NODES, &msg, sizeof(msg));
        SDL_Delay(60);
    }
    goToChoice("A PLAYER LEFT");
}

static void enterScene(struct scene *s)
{
    curScene = s;
    shownSeconds = -1;
}

/* Back to the lobby (host: cancels the rules or the READY phase). */
static void cancelToLobby(void)
{
    if (!netIsHost())
        return;
    netLockJoining(false);
    for (int i = 0; i < NET_MAX_NODES; ++i)
        members[i].ready = 0;
    setPhase(PH_LOBBY);
}

/* The READY phase is over: everybody is ready, or the time has run out. The
 * match begins. */
static void beginMatch(void)
{
    for (int i = 0; i < NET_MAX_NODES; ++i)
        members[i].ready = 0;
    setPhase(PH_GAME);
    sendState();
    lobbyDirty = false;
    lastSend = SDL_GetTicks();
    playSound(SOUND_ROUND_BEGIN, sound);
    netGameBegin(members, &rules, netMyNode(), true, lockedMask);
}

/* Client: the host began the match. */
static void startClientGame(void)
{
    phase = PH_GAME;
    playSound(SOUND_ROUND_BEGIN, sound);
    netGameBegin(members, &rules, netMyNode(), false, 0);
}

/* Client: the host's phase decides which page is shown. */
static void followHost(void)
{
    if (phase == PH_GAME)
        return;
    struct scene *target = phase == PH_RULES ? &waitScene
        : phase == PH_READY ? &readyScene : &lobbyScene;
    if (curScene == target)
        return;
    if (target == &readyScene) {
        wantReady = false;
        readyScene.parentScene = &choiceScene;
    }
    enterScene(target);
}

/* Handles the messages that came in. Returns true if the page changed. */
static bool receiveMessages(void)
{
    unsigned char buf[NET_MAX_PACKET];
    bool changed = false;
    int from = 0;
    size_t len;

    while ((len = netRecv(buf, sizeof(buf), &from)) > 0) {
        if (netIsHost()) {
            if (from < 1 || from > NET_MAX_NODES || !members[from - 1].used)
                continue;
            if (buf[0] == MSG_PROFILE && len >= sizeof(struct msgProfile)) {
                struct msgProfile msg;
                memcpy(&msg, buf, sizeof(msg));
                msg.name[PLAYER_NAME_LEN - 1] = '\0';
                struct member before = members[from - 1];
                applyProfile(from, msg.color, msg.name, msg.head, msg.title);
                changed |= memcmp(&before, &members[from - 1],
                                  sizeof(before)) != 0;
            } else if (buf[0] == MSG_READY && phase == PH_READY
                       && !members[from - 1].ready) {
                members[from - 1].ready = 1;
                lobbyDirty = true;
                changed = true;
            }
        } else if (from == NET_HOST_NODE) {
            if (buf[0] == MSG_STATE && len >= sizeof(struct msgState)) {
                struct msgState msg;
                memcpy(&msg, buf, sizeof(msg));
                if (memcmp(msg.members, members, sizeof(members)) != 0
                        || memcmp(&msg.rules, &rules, sizeof(rules)) != 0
                        || msg.phase != phase)
                    changed = true;
                memcpy(members, msg.members, sizeof(members));
                for (int i = 0; i < NET_MAX_NODES; ++i)
                    members[i].name[PLAYER_NAME_LEN - 1] = '\0';
                rules = msg.rules;
                readyLeftAtRecv = msg.readyLeft;
                readyRecvTime = SDL_GetTicks();
                phase = msg.phase;
                if (phase == PH_GAME) {
                    startClientGame();
                    return true;
                }
            } else if (buf[0] == MSG_SNAP && phase != PH_GAME
                       && (phase == PH_READY || phase == PH_RULES)) {
                /* The first snapshot: the host has begun (the message that
                 * said so may have got lost). */
                startClientGame();
                return true;
            } else if (buf[0] == MSG_ABORT) {
                goToChoice("A PLAYER LEFT");
                return true;
            }
        }
    }
    return changed;
}

/*
 * What every page of the group does each frame: look after the connection,
 * receive, and (host) send the state. Returns true if the page has to be
 * redrawn. May change the page; the caller must then stop at once.
 */
static bool netPump(void)
{
    if (!netConnected()) {
        goToChoice("CONNECTION LOST");
        return true;
    }

    bool changed = false;
    if (lobbyFresh) {
        initLobby();
        changed = true;
    }

    struct scene *before = curScene;
    changed |= receiveMessages();
    if (curScene != before)
        return true;

    Uint32 now = SDL_GetTicks();
    if (netIsHost()) {
        unsigned mask = netNodeMask();
        if (mask != lobbyMask) {
            if (phase != PH_LOBBY && mask != lockedMask) {
                abortAll();
                return true;
            }
            lobbyMask = mask;
            hostMembers();
            playSound(SOUND_BEEP, sound);
            changed = true;
        }
        if (phase == PH_READY) {
            bool all = true;
            for (int i = 0; i < NET_MAX_NODES; ++i) {
                if (members[i].used && !members[i].ready)
                    all = false;
            }
            if (all || now >= readyDeadline) {
                beginMatch();
                return true;
            }
        }
        if (lobbyDirty || now - lastSend > RESEND_MS) {
            sendState();
            lobbyDirty = false;
            lastSend = now;
        }
    } else {
        lobbyMask = netNodeMask();
        if (now - lastSend > RESEND_MS || (wantReady && now - lastSend > 400)) {
            sendProfile();
            if (wantReady && phase == PH_READY)
                sendReady();
            lastSend = now;
        }
        before = curScene;
        followHost();
        if (curScene != before)
            return true;
    }
    return changed;
}

/* ---- The lobby ---- */

static void requestColor(int color)
{
    int node = netMyNode();
    if (node < 1 || node > NET_MAX_NODES)
        return;
    if (color == wantColor && members[node - 1].color == color)
        return;
    if (!shopColorOwned(color)) {
        message = "BUY THIS COLOR IN THE SHOP";
        playSound(SOUND_BEP, sound);
        return;
    }
    if (colorTaken(color, node)) {
        playSound(SOUND_BEP, sound);
        return;
    }
    message = NULL;
    playSound(SOUND_BEEP, sound);
    wantColor = color;
    /* A name that is just a color's name follows the color. */
    if (nameIsDefault(me()->name))
        snprintf(me()->name, PLAYER_NAME_LEN, "%s", colorName(color));
    profileChanged();
}

/* Host: closes the group and goes on to the rules. */
static void startRules(void)
{
    lockedMask = netNodeMask();
    netLockJoining(true);
    rulesRow = 0;
    if (rules.ai + memberCount() < MIN_PLAYERS)
        rules.ai = MIN_PLAYERS - memberCount();
    setPhase(PH_RULES);
    playSound(SOUND_BEEP, sound);
    rulesScene.parentScene = &lobbyScene;
    enterScene(&rulesScene);
}

/* The next head marker of the shop that this console owns. */
static void cycleHead(int dir)
{
    me()->headStyle = shopCycleHead(shopOwnedHead(me()->headStyle), dir);
    playSound(SOUND_BEEP, sound);
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
    case ROW_HEAD:
        cycleHead(1);
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
    case ROW_START:
        if (netIsHost())
            startRules();
        break;
    }
}

static int logicLobby(void)
{
    bool changed = netPump();
    if (curScene != &lobbyScene)
        return 1;

    if (enterButtonDown()) {
        clearEnterButtons();
        lobbyActivate();
        return 1;
    }

    int rows = netIsHost() ? N_ROWS : N_ROWS - 1;
    int oldRow = lobbyRow, oldCursor = colorCursor;
    if (menuButtonQuery(KEY_DOWN) && lobbyRow < rows - 1)
        ++lobbyRow;
    else if (menuButtonQuery(KEY_UP) && lobbyRow > 0)
        --lobbyRow;
    else {
        int dir = menuButtonQuery(KEY_RIGHT) ? 1
            : menuButtonQuery(KEY_LEFT) ? -1 : 0;
        if (dir != 0 && lobbyRow == ROW_COLOR)
            colorCursor = (colorCursor + dir + N_COLORS) % N_COLORS;
        else if (dir != 0 && lobbyRow == ROW_HEAD) {
            cycleHead(dir);
            changed = true;
        }
    }
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

/* The members on the top screen; in the READY phase with a mark when ready. */
static void drawGroupTop(bool showReady)
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
                      showReady && m->ready ? uiColGreen
                      : n == mine ? uiColAccent : uiMix(uiColPanel, c, 55),
                      n == mine);
        int cx = cell.x + w / 2;
        uiBall(screen, cx, cell.y + 30, 8, 2, c);

        char name[11];
        snprintf(name, sizeof(name), "%s", m->name);
        uiText(screen, FONT_BROADCAST, name[0] != '\0' ? name : "...", c, cx,
               cell.y + 52, UI_CENTER);
        /* The title of the player, as it came over the network. */
        int title = m->title < statsTitleCount() ? m->title : 0;
        uiText(screen, FONT_BROADCAST, statsTitleName(title), uiColDim, cx,
               cell.y + 66, UI_CENTER);
        if (showReady)
            uiText(screen, FONT_MENU, m->ready ? "READY" : "...",
                   m->ready ? uiColGreen : uiColOff, cx, cell.y + 78,
                   UI_CENTER);
        else
            uiText(screen, FONT_MENU, n == NET_HOST_NODE ? "HOST" : "",
                   uiColGreen, cx, cell.y + 78, UI_CENTER);
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
    drawGroupTop(false);

    useBottomView();
    clearBackground(screen);
    touchReset();

    int mine = netMyNode();
    int shownColor = mine >= 1 && mine <= NET_MAX_NODES && members[mine - 1].used
        ? members[mine - 1].color : wantColor;

    SDL_Rect r = profileRow(8, 26, ROW_NAME, "NAME");
    uiText(screen, FONT_MENU, me()->name, colors[shownColor % N_COLORS],
           r.x + r.w - 10, r.y + 5, UI_RIGHT);

    /* The colors in a row, eight at a time: the window follows the cursor.
     * Those of the others are dimmed, those not bought have a lock. */
    r = profileRow(40, 36, ROW_COLOR, "TRAIL");
    const int sw = 24, sgap = 3, shown = 8;
    int first = colorCursor - shown / 2;
    if (first < 0)
        first = 0;
    if (first > N_COLORS - shown)
        first = N_COLORS - shown;
    int x0 = r.x + r.w - 10 - (shown * sw + (shown - 1) * sgap);
    for (int k = 0; k < shown; ++k) {
        int i = first + k;
        SDL_Rect s = {x0 + k * (sw + sgap), r.y + 6, sw, sw};
        bool taken = colorTaken(i, mine);
        bool owned = shopColorOwned(i);
        bool cursor = lobbyRow == ROW_COLOR && i == colorCursor;
        SDL_Color c = colors[i];
        touchAdd(s, TOUCH_SWATCH + i, 0);
        uiPanel(screen, s, taken || !owned ? uiMix(uiColPanel, c, 25) : c,
                i == shownColor ? uiColText
                : cursor ? uiColAccent : uiMix(uiColPanel, c, 50));
        if (!owned)
            uiIcon(screen, uiIconLock, s.x + 4, s.y + 4, 2, uiColText);
    }

    r = profileRow(82, 26, ROW_HEAD, "HEAD");
    uiText(screen, FONT_MENU, shopHeadName(shopOwnedHead(me()->headStyle)),
           shopOwnedHead(me()->headStyle) ? uiColAccent : uiColText,
           r.x + r.w - 10, r.y + 5, UI_RIGHT);

    for (int row = ROW_LEFT; row <= ROW_RIGHT; ++row) {
        bool left = row == ROW_LEFT;
        r = profileRow(row == ROW_LEFT ? 112 : 142, 26, row,
                       left ? "LEFT" : "RIGHT");
        char *text = buttonName(left ? me()->lkey : me()->rkey);
        uiText(screen, FONT_MENU, text[0] != '\0' ? text : "...", uiColText,
               r.x + r.w - 10, r.y + 5, UI_RIGHT);
        free(text);
    }

    char line[48];
    snprintf(line, sizeof(line), "%d / %d  %s", memberCount(), NET_MAX_NODES,
             netIsHost() ? "PLAYERS" : "WAITING FOR THE HOST");
    uiText(screen, FONT_BROADCAST, line, uiColDim, 160, 174, UI_CENTER);

    if (netIsHost()) {
        SDL_Rect go = {16, 186, 288, 28};
        touchAdd(go, ROW_START, TOUCH_ROW);
        uiTile(screen, go, "START", NULL, NULL, uiColGreen,
               lobbyRow == ROW_START, true);
    }

    const char *action = lobbyRow == ROW_COLOR ? "PICK"
        : lobbyRow == ROW_START ? "START" : "CHANGE";
    struct uiHint hints[] = {{"A", action, 6}, {"B", "LEAVE"}};
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

/* ---- The rules (host) ---- */

static const char *const ruleTitle[N_RULE_ROWS] = {
    "MAX POINTS", "HOLES", "DUEL MODE", "SPEED", "AI PLAYERS", "GO"
};

static int pointsIndex(void)
{
    for (int i = 0; i < N_POINTS; ++i) {
        if (pointChoices[i] == rules.points)
            return i;
    }
    return 3;
}

/* AI players can fill up to the eight places. */
static int maxAi(void)
{
    return NET_MAX_NODES - memberCount();
}

static void ruleChange(int row, int dir)
{
    switch (row) {
    case RULE_POINTS:
        rules.points = pointChoices[(pointsIndex() + dir + N_POINTS) % N_POINTS];
        break;
    case RULE_HOLES:
        rules.holes = !rules.holes;
        break;
    case RULE_DUEL:
        rules.duel = !rules.duel;
        break;
    case RULE_SPEED:
        rules.speed = (rules.speed + dir + N_SPEEDS) % N_SPEEDS;
        break;
    case RULE_AI: {
        int ai = rules.ai + dir;
        if (ai < 0)
            ai = maxAi();
        if (ai > maxAi())
            ai = 0;
        rules.ai = ai;
        break;
    }
    }
    lobbyDirty = true;
}

/* Host: the rules are final. Everyone gets them, then READY. */
static void rulesGo(void)
{
    if (rules.ai + memberCount() < MIN_PLAYERS) {
        playSound(SOUND_BEP, sound);
        message = "NEED AT LEAST TWO PLAYERS";
        return;
    }
    if (rules.duel && rules.ai + memberCount() != 2) {
        playSound(SOUND_BEP, sound);
        message = "DUEL NEEDS EXACTLY TWO";
        return;
    }
    message = NULL;
    for (int i = 0; i < NET_MAX_NODES; ++i)
        members[i].ready = 0;
    wantReady = false;
    readyDeadline = SDL_GetTicks() + READY_MS;
    setPhase(PH_READY);
    playSound(SOUND_BEEP, sound);
    readyScene.parentScene = &lobbyScene;
    enterScene(&readyScene);
}

static int logicRules(void)
{
    bool changed = netPump();
    if (curScene != &rulesScene)
        return 1;

    if (enterButtonDown()) {
        clearEnterButtons();
        if (rulesRow == RULE_GO) {
            rulesGo();
        } else {
            playSound(SOUND_BEEP, sound);
            ruleChange(rulesRow, 1);
        }
        return 1;
    }

    if (menuButtonQuery(KEY_DOWN) && rulesRow < N_RULE_ROWS - 1)
        ++rulesRow;
    else if (menuButtonQuery(KEY_UP) && rulesRow > 0)
        --rulesRow;
    else if (menuButtonQuery(KEY_RIGHT) && rulesRow != RULE_GO)
        ruleChange(rulesRow, 1);
    else if (menuButtonQuery(KEY_LEFT) && rulesRow != RULE_GO)
        ruleChange(rulesRow, -1);
    else
        return changed;

    playSound(SOUND_BEEP, sound);
    return 1;
}

static void displayRules(void)
{
    drawGroupTop(false);

    useBottomView();
    clearBackground(screen);
    touchReset();

    uiText(screen, FONT_MENU, "RULES", uiColBlue, 16, 8, UI_LEFT);

    int y = 30;
    for (int row = 0; row < RULE_GO; ++row, y += 30) {
        SDL_Rect r = lobbyRowPanel(y, row == rulesRow, ruleTitle[row]);
        switch (row) {
        case RULE_POINTS:
        case RULE_AI: {
            char value[8];
            if (row == RULE_POINTS && rules.points == 0)
                snprintf(value, sizeof(value), "INF");
            else
                snprintf(value, sizeof(value), "%u",
                         row == RULE_POINTS ? rules.points : rules.ai);
            int cx = r.x + r.w - 70;
            SDL_Rect less = {cx - 54 - 20, r.y, 40, r.h};
            SDL_Rect more = {cx + 54 - 20, r.y, 40, r.h};
            touchAdd(less, row, -1);
            touchAdd(more, row, 1);
            uiText(screen, FONT_MENU, "<", uiColAccent, cx - 54, r.y + 5,
                   UI_CENTER);
            uiText(screen, FONT_MENU, value, uiColText, cx, r.y + 5, UI_CENTER);
            uiText(screen, FONT_MENU, ">", uiColAccent, cx + 54, r.y + 5,
                   UI_CENTER);
            break;
        }
        case RULE_HOLES:
            drawSwitch(r, RULE_HOLES, rules.holes);
            break;
        case RULE_DUEL:
            drawSwitch(r, RULE_DUEL, rules.duel);
            break;
        case RULE_SPEED: {
            const char *labels[4] = {"75%", "100%", "125%", "150%"};
            drawChoice(r, RULE_SPEED, 40, 4, labels, rules.speed, 0xF);
            break;
        }
        }
        touchAdd(r, row, TOUCH_ROW);
    }

    SDL_Rect go = {16, 182, 288, 30};
    touchAdd(go, RULE_GO, TOUCH_ROW);
    uiTile(screen, go, "GO", NULL, NULL, uiColGreen, rulesRow == RULE_GO, true);
    drawMessage(216);

    struct uiHint hints[] = {{"A", rulesRow == RULE_GO ? "GO" : "CHANGE", 6},
                             {"<>", "VALUE"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 3);

    if (presentFrame() == -1)
        exit(1);
}

static void touchRules(int id, int value, bool released)
{
    if (!released) {
        rulesRow = id;
        return;
    }

    if (id == RULE_GO) {
        rulesGo();
    } else if (id == RULE_SPEED && value >= 0 && value < N_SPEEDS) {
        playSound(SOUND_BEEP, sound);
        rules.speed = value;
        lobbyDirty = true;
    } else if (value == -1 || value == 1) {
        playSound(SOUND_BEEP, sound);
        ruleChange(id, value);
    } else {
        playSound(SOUND_BEEP, sound);
        ruleChange(id, 1);
    }
}

/* ---- Waiting for the host's rules (clients) ---- */

static int logicWait(void)
{
    netPump();
    return curScene != &waitScene;
}

static void displayWait(void)
{
    drawGroupTop(false);

    useBottomView();
    clearBackground(screen);
    touchReset();

    uiText(screen, FONT_MENU, "RULES", uiColBlue, 16, 8, UI_LEFT);
    uiText(screen, FONT_BROADCAST, "THE HOST IS CHOOSING", uiColText, 160, 100,
           UI_CENTER);
    uiText(screen, FONT_BROADCAST, "THE RULES...", uiColText, 160, 118,
           UI_CENTER);

    struct uiHint hints[] = {{"B", "LEAVE"}};
    uiHintBar(screen, hints, 1);

    if (presentFrame() == -1)
        exit(1);
}

/* ---- READY ---- */

static void pressReady(void)
{
    if (wantReady)
        return;
    int node = netMyNode();
    if (node < 1 || node > NET_MAX_NODES)
        return;
    playSound(SOUND_BEEP, sound);
    wantReady = true;
    if (netIsHost()) {
        members[node - 1].ready = 1;
        lobbyDirty = true;
    } else {
        sendReady();
        lastSend = SDL_GetTicks();
    }
}

static int logicReady(void)
{
    bool changed = netPump();
    if (curScene != &readyScene)
        return 1;

    int seconds = (readyRemaining() + 999) / 1000;
    if (seconds != shownSeconds) {
        shownSeconds = seconds;
        changed = true;
    }

    if (enterButtonDown()) {
        clearEnterButtons();
        pressReady();
        return 1;
    }
    return changed;
}

static void displayReady(void)
{
    drawGroupTop(true);

    useBottomView();
    clearBackground(screen);
    touchReset();

    uiText(screen, FONT_MENU, "GET READY", uiColBlue, 16, 8, UI_LEFT);
    char line[48];
    snprintf(line, sizeof(line), "%d", (int)((readyRemaining() + 999) / 1000));
    uiText(screen, FONT_MENU, line, uiColAccent, 304, 8, UI_RIGHT);

    if (rules.points == 0)
        snprintf(line, sizeof(line), "NO POINT LIMIT");
    else
        snprintf(line, sizeof(line), "FIRST TO %u POINTS", rules.points);
    uiText(screen, FONT_BROADCAST, line, uiColText, 16, 44, UI_LEFT);
    snprintf(line, sizeof(line), "HOLES %s   DUEL %s", rules.holes ? "ON" : "OFF",
             rules.duel ? "ON" : "OFF");
    uiText(screen, FONT_BROADCAST, line, uiColText, 16, 64, UI_LEFT);
    snprintf(line, sizeof(line), "SPEED %d%%   AI PLAYERS %u",
             speedChoices[rules.speed % N_SPEEDS], rules.ai);
    uiText(screen, FONT_BROADCAST, line, uiColText, 16, 84, UI_LEFT);

    SDL_Rect ready = {16, 120, 288, 70};
    touchAdd(ready, 0, TOUCH_ROW);
    uiTile(screen, ready, wantReady ? "WAITING..." : "READY", NULL, NULL,
           uiColGreen, !wantReady, !wantReady);

    struct uiHint hints[] = {{"A", "READY"}, {"B", netIsHost() ? "CANCEL" : "LEAVE"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}

static void touchReady(int id, int value, bool released)
{
    (void)id;
    (void)value;
    if (released)
        touchPressA();
}

static void leaveReady(void)
{
    if (netIsHost())
        cancelToLobby();
    else
        leaveLobby();
}

/* ---- From the match back to the group ---- */

void netLobbyResume(void)
{
    netGameStop();
    phase = PH_LOBBY;
    wantReady = false;
    for (int i = 0; i < NET_MAX_NODES; ++i)
        members[i].ready = 0;
    if (netIsHost())
        netLockJoining(false);
    lobbyMask = netNodeMask();
    lobbyDirty = true;
    lastSend = 0;
    lobbyFresh = false;
    lobbyRow = 0;
    enterScene(&lobbyScene);
    lobbyScene.parentScene = &choiceScene;
}

void netMenuAbort(const char *why, bool tellOthers)
{
    if (tellOthers && netIsHost()) {
        unsigned char msg = MSG_ABORT;
        for (int i = 0; i < 3; ++i) {
            netSend(NET_ALL_NODES, &msg, sizeof(msg));
            SDL_Delay(60);
        }
    }
    netGameStop();
    goToChoice(why);
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
    rulesScene = (struct scene){logicRules, displayRules, &lobbyScene,
                                touchRules, NULL, cancelToLobby};
    waitScene = (struct scene){logicWait, displayWait, &choiceScene,
                               touchLobby, NULL, leaveLobby};
    readyScene = (struct scene){logicReady, displayReady, &choiceScene,
                                touchReady, NULL, leaveReady};

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
