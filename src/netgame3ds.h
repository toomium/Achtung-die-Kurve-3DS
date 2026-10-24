/* netgame3ds -- playing a match with several consoles.
 *
 * The host runs the game as usual; the other consoles send their steering to
 * it and show what it sends back (positions, scores) with the same pages as a
 * local game. Between rounds everybody presses READY.
 */
#ifndef NETGAME3DS_H
#define NETGAME3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <stddef.h>

enum { NET_ROLE_NONE = 0, NET_ROLE_HOST, NET_ROLE_CLIENT };

/* The pages of a match, as the host sends them. */
enum { GS_COUNTDOWN = 0, GS_PLAY, GS_ROUNDEND, GS_MATCHEND };

/* What netGameTick() tells the page that called it. */
enum { NET_TICK_CONTINUE = 0,   /* Host: go on with the normal logic */
       NET_TICK_REDRAW,         /* Something changed: draw the page */
       NET_TICK_IDLE };         /* Nothing to do */

/* A player of a match with several consoles. */
struct netSeat {
    char name[13];
    unsigned char color;
    bool ai;
    bool local;                 /* Steered on this console */
    int node;                   /* The console it belongs to, 0 for AI */
};

/* One player as it is sent from the host to the others. */
struct netPlayerFrame {
    float x, y, dir;
    unsigned char alive, hole, score, killer;
};

/* ---- Implemented in zatackax.c ---- */

/* Starts a match with `n` players; `role` is NET_ROLE_HOST or _CLIENT. */
void startNetGame(const struct netSeat *seats, int n, int role,
                  unsigned points, bool holes, bool duel, int speed);

/* The page the host is on, as GS_*. */
int netSceneState(void);

/* Host: the state of the match, to send. Returns the number of players. */
int netGetFrame(struct netPlayerFrame *frames, int *countdown, int *winner);

/* Client: shows the state the host sent. */
void netApplyFrame(int gstate, int countdown, int winner,
                   const struct netPlayerFrame *frames, int n);

/* Brings the setup of the local game back after a match. */
void netEndGame(void);

/* ---- Implemented in netgame3ds.c ---- */

int netGameRole(void);

/* Every frame of every page of a match. See NET_TICK_*. */
int netGameTick(void);

/* The round (or the match) is over: READY begins. Host only. */
void netGameRoundEndBegin(void);

/* This console is ready. */
void netGameReadyPress(void);

/* Host: everybody is ready, or the time is up. */
bool netGameReadyComplete(void);

/* The READY situation as text, and whether it changed since the last call. */
void netGameStatus(char *buf, size_t size);
bool netGameStatusChanged(void);

/* The match is over on this console (restores the setup of the local game). */
void netGameStop(void);

/* Host: ends the match for everybody, back to the lobby. */
void netGameToLobby(void);

#endif

#endif
