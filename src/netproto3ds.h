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
    char name[PLAYER_NAME_LEN];
};

/* What the host chooses for the match. */
struct netRules {
    unsigned short points;      /* 0: no limit */
    unsigned char holes;
    unsigned char duel;
    unsigned char speed;        /* Index into speedChoices */
    unsigned char ai;           /* Computer players added to the humans */
};

enum { MSG_PROFILE = 1, MSG_STATE, MSG_READY, MSG_ABORT, MSG_INPUT, MSG_SNAP,
       MSG_TOLOBBY };

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
