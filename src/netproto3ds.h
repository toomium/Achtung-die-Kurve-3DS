/* netproto3ds -- what the pages of the group (netmenu3ds.c) and the match
 * (netgame3ds.c) share: the group's state and the messages.
 */
#ifndef NETPROTO3DS_H
#define NETPROTO3DS_H

#ifdef __3DS__

#include <stdbool.h>
#include <SDL/SDL.h>

#include "net.h"
#include "player.h"

/* What the group knows about each node (index node - 1). The host decides, the
 * clients show what the host sends. */
struct member {
    unsigned char used;
    unsigned char color;
    unsigned char ready;
    unsigned char head;         /* Shop head marker, see player.h */
    unsigned char title;        /* The number of the title (stats3ds.h) */
    char name[PLAYER_NAME_LEN];
};

/* What the host chooses for the match. */
struct netRules {
    unsigned short points;      /* 0: no limit */
    unsigned char holes;
    unsigned char speed;        /* Index into speedChoices */
    unsigned char ai;           /* Computer players added to the humans */
};

enum { MSG_PROFILE = 1, MSG_STATE, MSG_READY, MSG_ABORT, MSG_INPUT, MSG_SNAP,
       MSG_TOLOBBY };

/* ---- The messages ---- */

/* One player in a snapshot. */
struct snapPlayer {
    float x, y, dir;
    unsigned char alive, hole, score, killer;
};

/* The host sends this every frame during a round: all the client needs. */
struct msgSnap {
    unsigned char type;
    unsigned char gstate;
    unsigned char readyMask;    /* Bit node - 1: that console is ready */
    unsigned char humanMask;    /* ... and who has to be */
    unsigned short seq;
    unsigned short countdown;
    unsigned short readyLeft;   /* Milliseconds of the READY phase left */
    signed char winner;         /* Match winner, -1 for none */
    unsigned char nPlayers;
    struct snapPlayer players[MAX_PLAYERS];
};

/* A client sends its steering to the host. */
struct msgInput {
    unsigned char type;
    unsigned char left, right;
};

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

/* A message has to fit in one packet. */
_Static_assert(sizeof(struct msgState) <= NET_MAX_PACKET, "msgState too big");
_Static_assert(sizeof(struct msgSnap) <= NET_MAX_PACKET, "msgSnap too big");

/* The size of a message as two bytes, for the group's version. */
#define SIZE_BYTES(type) (sizeof(type) & 0xFF), (sizeof(type) >> 8)

/* The speeds the host can choose from, in percent. */
#define N_SPEEDS 4
extern const int speedChoices[N_SPEEDS];

/* Milliseconds left of a READY phase: the host knows its deadline, a client
 * counts down from what the host last said (`leftAtRecv`, heard at `recvTime`). */
static inline unsigned readyTimeLeft(bool host, Uint32 deadline,
                                     unsigned leftAtRecv, Uint32 recvTime)
{
    Uint32 now = SDL_GetTicks();
    if (host)
        return now < deadline ? deadline - now : 0;
    Uint32 gone = now - recvTime;
    return gone < leftAtRecv ? leftAtRecv - gone : 0;
}

/* Starts the match (netgame3ds.c). `locked` is who is in the group. */
void netGameBegin(const struct member members[NET_MAX_NODES],
                  const struct netRules *rules, int myNode, bool host,
                  unsigned locked);

#endif

#endif
