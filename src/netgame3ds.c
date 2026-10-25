#ifdef __3DS__

#include "netgame3ds.h"

#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>

#include "common.h"
#include "gfx.h"
#include "input.h"
#include "net.h"
#include "netmenu3ds.h"
#include "netproto3ds.h"
#include "player.h"
#include "sound.h"
#include "config.h"

#define READY_MS        30000
#define STILL_SNAP_MS   250     /* Snapshot interval between the rounds */
#define INPUT_MS        100     /* Steering is sent at least this often */
#define INPUT_LOST_MS   400     /* A silent console stops steering */
#define READY_RESEND_MS 400

static int role = NET_ROLE_NONE;
static int myNode = 0;
static int mySeat = -1;
static int nSeats = 0;
static int seatOfNode[NET_MAX_NODES + 1];
static unsigned lockedMask = 0;
static unsigned humanMask = 0;
static unsigned readyMask = 0;
static Uint32 readyDeadline = 0;
static bool wantReady = false;
static Uint32 lastReadySent = 0;
static Uint32 lastSnapSent = 0;
static unsigned short seq = 0;
static unsigned short lastSeq = 0;
static bool haveSeq = false;
static Uint32 lastInputFrom[NET_MAX_NODES + 1];
static Uint32 lastInputSent = 0;
static bool sentLeft = false, sentRight = false;
static unsigned readyLeftRecv = 0;
static Uint32 readyRecvTime = 0;
static int prevGs = -1;
static char lastStatus[32];

int netGameRole(void)
{
    return role;
}

int netGameMySeat(void)
{
    return mySeat;
}

/* ---- Beginning and end ---- */

void netGameBegin(const struct member members[NET_MAX_NODES],
                  const struct netRules *rules, int node, bool host,
                  unsigned locked)
{
    struct netSeat seats[MAX_PLAYERS];
    int n = 0;
    bool colorUsed[N_COLORS] = {false};

    memset(seats, 0, sizeof(seats));
    for (int i = 0; i <= NET_MAX_NODES; ++i)
        seatOfNode[i] = -1;
    humanMask = 0;
    mySeat = -1;

    /* The consoles first, in the order they joined, then the computer. */
    for (int i = 1; i <= NET_MAX_NODES && n < MAX_PLAYERS; ++i) {
        const struct member *m = &members[i - 1];
        if (!m->used)
            continue;
        struct netSeat *s = &seats[n];
        snprintf(s->name, sizeof(s->name), "%s", m->name);
        s->color = m->color % N_COLORS;
        s->head = m->head;
        s->node = i;
        s->local = i == node;
        colorUsed[s->color] = true;
        seatOfNode[i] = n;
        humanMask |= 1u << (i - 1);
        if (s->local)
            mySeat = n;
        ++n;
    }
    for (int k = 0; k < rules->ai && n < MAX_PLAYERS; ++k) {
        struct netSeat *s = &seats[n];
        snprintf(s->name, sizeof(s->name), "AI %d", k + 1);
        s->ai = true;
        int c = 0;
        while (c < N_BASE_COLORS - 1 && colorUsed[c])
            ++c;
        s->color = c;
        colorUsed[c] = true;
        ++n;
    }
    if (rules->duel && n > 2)
        n = 2;

    myNode = node;
    nSeats = n;
    lockedMask = locked;
    readyMask = 0;
    wantReady = false;
    seq = 0;
    haveSeq = false;
    prevGs = -1;
    sentLeft = sentRight = false;
    lastStatus[0] = '\0';
    for (int i = 0; i <= NET_MAX_NODES; ++i)
        lastInputFrom[i] = SDL_GetTicks();
    role = host ? NET_ROLE_HOST : NET_ROLE_CLIENT;

    startNetGame(seats, n, role, rules->points, rules->holes, rules->duel,
                 speedChoices[rules->speed % N_SPEEDS]);
}

static void endGame(void)
{
    if (role == NET_ROLE_NONE)
        return;
    role = NET_ROLE_NONE;
    netEndGame();
}

/* Called by the lobby pages when they take over again. */
void netGameStop(void)
{
    endGame();
}

void netGameToLobby(void)
{
    unsigned char msg = MSG_TOLOBBY;
    for (int i = 0; i < 3; ++i) {
        netSend(NET_ALL_NODES, &msg, sizeof(msg));
        SDL_Delay(60);
    }
    netLobbyResume();
}

/* ---- READY ---- */

void netGameRoundEndBegin(void)
{
    readyMask = 0;
    wantReady = false;
    readyDeadline = SDL_GetTicks() + READY_MS;
}

void netGameReadyPress(void)
{
    if (myNode < 1)
        return;
    if (role == NET_ROLE_HOST) {
        if (!(readyMask & (1u << (myNode - 1))))
            playSound(SOUND_BEEP, sound);
        readyMask |= 1u << (myNode - 1);
    } else if (role == NET_ROLE_CLIENT && !wantReady) {
        playSound(SOUND_BEEP, sound);
        wantReady = true;
        unsigned char msg = MSG_READY;
        netSend(NET_HOST_NODE, &msg, sizeof(msg));
        lastReadySent = SDL_GetTicks();
    }
}

bool netGameReadyComplete(void)
{
    return (readyMask & humanMask) == humanMask
        || SDL_GetTicks() >= readyDeadline;
}

static unsigned readyRemaining(void)
{
    return readyTimeLeft(role == NET_ROLE_HOST, readyDeadline, readyLeftRecv,
                         readyRecvTime);
}

static int bits(unsigned v)
{
    int n = 0;
    for (; v != 0; v >>= 1)
        n += v & 1;
    return n;
}

void netGameStatus(char *buf, size_t size)
{
    snprintf(buf, size, "READY %d/%d   %u", bits(readyMask & humanMask),
             bits(humanMask), (readyRemaining() + 999) / 1000);
}

bool netGameStatusChanged(void)
{
    char now[sizeof(lastStatus)];
    netGameStatus(now, sizeof(now));
    if (strcmp(now, lastStatus) == 0)
        return false;
    memcpy(lastStatus, now, sizeof(lastStatus));
    return true;
}

/* ---- Host ---- */

static void sendSnapshot(Uint32 now)
{
    struct msgSnap msg;
    struct netPlayerFrame frames[MAX_PLAYERS];
    int countdown = 0, winner = -1;

    memset(&msg, 0, sizeof(msg));
    int n = netGetFrame(frames, &countdown, &winner);
    msg.type = MSG_SNAP;
    msg.gstate = netSceneState();
    msg.readyMask = readyMask;
    msg.humanMask = humanMask;
    msg.seq = ++seq;
    msg.countdown = countdown > 0 ? countdown : 0;
    msg.readyLeft = msg.gstate >= GS_ROUNDEND ? readyRemaining() : 0;
    msg.winner = winner;
    msg.nPlayers = n;
    for (int i = 0; i < n; ++i) {
        msg.players[i].x = frames[i].x;
        msg.players[i].y = frames[i].y;
        msg.players[i].dir = frames[i].dir;
        msg.players[i].alive = frames[i].alive;
        msg.players[i].hole = frames[i].hole;
        msg.players[i].score = frames[i].score;
        msg.players[i].killer = frames[i].killer;
    }
    netSend(NET_ALL_NODES, &msg, sizeof(msg));
    lastSnapSent = now;
}

static void hostReceive(Uint32 now)
{
    unsigned char buf[NET_MAX_PACKET];
    int from = 0;
    size_t len;

    while ((len = netRecv(buf, sizeof(buf), &from)) > 0) {
        if (from < 1 || from > NET_MAX_NODES || seatOfNode[from] < 0)
            continue;
        struct player *p = &players[seatOfNode[from]];

        if (buf[0] == MSG_INPUT && len >= sizeof(struct msgInput)) {
            struct msgInput in;
            memcpy(&in, buf, sizeof(in));
            keyDown[p->lkey] = in.left;
            keyDown[p->rkey] = in.right;
            lastInputFrom[from] = now;
        } else if (buf[0] == MSG_READY && (humanMask & (1u << (from - 1)))) {
            readyMask |= 1u << (from - 1);
        }
    }

    /* A console that has gone silent stops steering. */
    for (int node = 1; node <= NET_MAX_NODES; ++node) {
        if (seatOfNode[node] >= 0 && node != myNode
                && now - lastInputFrom[node] > INPUT_LOST_MS) {
            struct player *p = &players[seatOfNode[node]];
            keyDown[p->lkey] = false;
            keyDown[p->rkey] = false;
        }
    }
}

static int hostTick(Uint32 now)
{
    if (netNodeMask() != lockedMask) {
        netMenuAbort("A PLAYER LEFT", true);
        return NET_TICK_REDRAW;
    }

    hostReceive(now);

    int gs = netSceneState();
    if (gs <= GS_PLAY || now - lastSnapSent >= STILL_SNAP_MS)
        sendSnapshot(now);
    return NET_TICK_CONTINUE;
}

/* ---- Client ---- */

/* For the debug log: how many snapshots came in, and how many frames used one. */
static unsigned statStart = 0, statSnaps = 0, statFrames = 0, statLate = 0;

static int clientTick(Uint32 now)
{
    unsigned char buf[NET_MAX_PACKET];
    struct msgSnap latest;
    bool got = false;
    int from = 0;
    size_t len;

    while ((len = netRecv(buf, sizeof(buf), &from)) > 0) {
        if (from != NET_HOST_NODE)
            continue;
        if (buf[0] == MSG_SNAP && len >= sizeof(struct msgSnap)) {
            struct msgSnap msg;
            memcpy(&msg, buf, sizeof(msg));
            /* Only newer snapshots count (the number wraps around). */
            if (haveSeq && (short)(msg.seq - lastSeq) <= 0)
                continue;
            if (haveSeq && (unsigned short)(msg.seq - lastSeq) > 1)
                statLate += (unsigned short)(msg.seq - lastSeq) - 1;
            ++statSnaps;
            haveSeq = true;
            lastSeq = msg.seq;
            latest = msg;
            got = true;
        } else if (buf[0] == MSG_ABORT) {
            netMenuAbort("A PLAYER LEFT", false);
            return NET_TICK_REDRAW;
        } else if (buf[0] == MSG_TOLOBBY) {
            netLobbyResume();
            return NET_TICK_REDRAW;
        }
    }

    ++statFrames;
    if (now - statStart >= 2000) {
        fprintf(stderr, "net client: %u snapshots, %u missing, %u frames in %u ms\n",
                statSnaps, statLate, statFrames, (unsigned)(now - statStart));
        statStart = now;
        statSnaps = statLate = statFrames = 0;
    }

    bool redraw = false;
    if (got) {
        if (latest.gstate != prevGs)
            wantReady = false;
        prevGs = latest.gstate;
        readyMask = latest.readyMask;
        humanMask = latest.humanMask;
        readyLeftRecv = latest.readyLeft;
        readyRecvTime = now;

        struct netPlayerFrame frames[MAX_PLAYERS];
        int n = latest.nPlayers < MAX_PLAYERS ? latest.nPlayers : MAX_PLAYERS;
        for (int i = 0; i < n; ++i) {
            frames[i].x = latest.players[i].x;
            frames[i].y = latest.players[i].y;
            frames[i].dir = latest.players[i].dir;
            frames[i].alive = latest.players[i].alive;
            frames[i].hole = latest.players[i].hole;
            frames[i].score = latest.players[i].score;
            frames[i].killer = latest.players[i].killer;
        }
        netApplyFrame(latest.gstate, latest.countdown, latest.winner, frames, n);
        redraw = true;
    }

    /* Steering goes to the host when it changes, and now and then. */
    if (prevGs == GS_COUNTDOWN || prevGs == GS_PLAY) {
        if (mySeat >= 0) {
            bool left = buttonDown(players[mySeat].lkey);
            bool right = buttonDown(players[mySeat].rkey);
            if (left != sentLeft || right != sentRight
                    || now - lastInputSent >= INPUT_MS) {
                struct msgInput in = {MSG_INPUT, left, right};
                netSend(NET_HOST_NODE, &in, sizeof(in));
                sentLeft = left;
                sentRight = right;
                lastInputSent = now;
            }
        }
    } else if (prevGs >= GS_ROUNDEND) {
        if (enterButtonDown()) {
            clearEnterButtons();
            netGameReadyPress();
            redraw = true;
        }
        if (wantReady && !(readyMask & (1u << (myNode - 1)))
                && now - lastReadySent >= READY_RESEND_MS) {
            unsigned char msg = MSG_READY;
            netSend(NET_HOST_NODE, &msg, sizeof(msg));
            lastReadySent = now;
        }
        if (netGameStatusChanged())
            redraw = true;
    }
    return redraw ? NET_TICK_REDRAW : NET_TICK_IDLE;
}

int netGameTick(void)
{
    if (role == NET_ROLE_NONE)
        return NET_TICK_CONTINUE;
    if (!netConnected()) {
        netMenuAbort("CONNECTION LOST", false);
        return NET_TICK_REDRAW;
    }

    Uint32 now = SDL_GetTicks();
    return role == NET_ROLE_HOST ? hostTick(now) : clientTick(now);
}

#endif /* __3DS__ */
