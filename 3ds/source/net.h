/* net -- Local Play between 3DS consoles (libctru's UDS), kept apart from the
 * game code. Like platform.c, only net.c may include <3ds.h>.
 *
 * A console either hosts a group or joins one. Nodes are numbered: the host is
 * always node 1, the clients get 2, 3, ... (at most NET_MAX_NODES in all).
 * Messages are small (NET_MAX_PACKET bytes at most) and may get lost; the game
 * layer takes care of that.
 */
#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stddef.h>

#define NET_MAX_NODES   8
#define NET_HOST_NODE   1
#define NET_ALL_NODES   0xFFFF      /* Destination: every other node */
#define NET_MAX_PACKET  1400
#define NET_APPDATA_MAX 64          /* Bytes the host can show to searchers */
#define NET_NAME_MAX    10          /* Characters of a console's user name */

/* A group found by netScan(). */
struct netGroup {
    unsigned char appdata[NET_APPDATA_MAX];
    size_t appdataSize;
    int nodes;                      /* Consoles in the group, host included */
    int maxNodes;
    char hostName[NET_NAME_MAX * 4 + 1];    /* The console's user name */
};

/* Starts / stops the network service. Returns false if it can't be started. */
bool netInit(void);
void netExit(void);

/*
 * Opens a group with room for `maxNodes` consoles. `appdata` (at most
 * NET_APPDATA_MAX bytes) is shown to those who search; change it later with
 * netSetAppdata().
 */
bool netHost(const void *appdata, size_t size, int maxNodes);
bool netSetAppdata(const void *appdata, size_t size);

/* Searches for groups (takes a moment). Returns how many were found, at most
 * `max`. */
int netScan(struct netGroup *groups, int max);

/* Joins group number `index` of the last netScan(). */
bool netJoin(int index);

/* Host: stop (true) or allow (false) new consoles joining the group. */
bool netLockJoining(bool lock);

/* Leaves the group; the host closes it for everyone. */
void netLeave(void);

/* True while the connection exists. For a client false means it was lost or
 * the host has gone. */
bool netConnected(void);

/* Own node number (1 for the host), and who is in the group right now: bit
 * n - 1 is set for node n. */
int netMyNode(void);
unsigned netNodeMask(void);
bool netIsHost(void);

/* The console's user name of a node. */
bool netNodeName(int node, char *name, size_t size);

/* Sends to one node, or to NET_ALL_NODES. Returns false on a fatal error. */
bool netSend(int node, const void *data, size_t size);

/* The next received message, or 0 if there is none. Stores the sender in
 * `*from`. */
size_t netRecv(void *buf, size_t size, int *from);

/* The last error code of the system (0 = none), for the debug log. */
unsigned netLastError(void);

#endif
