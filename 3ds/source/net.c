#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"

/* Identifies this game among the others that use Local Play. */
#define WLAN_COMM_ID    0x5A4B5833
#define NETWORK_ID8     1
#define DATA_CHANNEL    1
#define SHARED_MEM_SIZE 0x3000
#define SCAN_BUF_SIZE   0x4000

static const char passphrase[] = "zatackax";

static bool started = false;
static bool inGroup = false;
static bool hosting = false;
static udsBindContext udsBinding;
static Result lastResult = 0;

/* The groups of the last scan, kept so that netJoin() can use them. */
static udsNetworkScanInfo scanned[NET_MAX_NODES * 2];
static int nScanned = 0;

#define MAX_GROUPS ((int)(sizeof(scanned) / sizeof(scanned[0])))

static bool failed(Result r)
{
    lastResult = r;
    return R_FAILED(r);
}

bool netInit(void)
{
    if (started)
        return true;
    if (failed(udsInit(SHARED_MEM_SIZE, NULL)))
        return false;
    started = true;
    return true;
}

void netExit(void)
{
    netLeave();
    if (started) {
        udsExit();
        started = false;
    }
}

bool netHost(const void *appdata, size_t size, int maxNodes)
{
    if (!started || inGroup)
        return false;
    if (maxNodes > NET_MAX_NODES)
        maxNodes = NET_MAX_NODES;

    udsNetworkStruct network;
    udsGenerateDefaultNetworkStruct(&network, WLAN_COMM_ID, NETWORK_ID8,
                                    maxNodes);
    if (failed(udsCreateNetwork(&network, passphrase, sizeof(passphrase),
                                &udsBinding, DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE)))
        return false;

    inGroup = true;
    hosting = true;
    if (appdata != NULL)
        netSetAppdata(appdata, size);
    return true;
}

bool netSetAppdata(const void *appdata, size_t size)
{
    if (!inGroup || !hosting || size > NET_APPDATA_MAX)
        return false;
    return !failed(udsSetApplicationData(appdata, size));
}

int netScan(struct netGroup *groups, int max)
{
    if (!started || inGroup)
        return 0;

    void *buf = malloc(SCAN_BUF_SIZE);
    if (buf == NULL)
        return 0;

    udsNetworkScanInfo *found = NULL;
    size_t total = 0;
    Result r = udsScanBeacons(buf, SCAN_BUF_SIZE, &found, &total, WLAN_COMM_ID,
                              NETWORK_ID8, NULL, false);
    free(buf);
    nScanned = 0;
    if (failed(r) || found == NULL)
        return 0;

    for (size_t i = 0; i < total && nScanned < MAX_GROUPS && nScanned < max;
         ++i) {
        struct netGroup *g = &groups[nScanned];
        memset(g, 0, sizeof(*g));

        size_t got = 0;
        if (failed(udsGetNetworkStructApplicationData(&found[i].network,
                                                      g->appdata,
                                                      sizeof(g->appdata), &got)))
            continue;
        g->appdataSize = got;
        g->nodes = found[i].network.total_nodes;
        g->maxNodes = found[i].network.max_nodes;
        if (udsCheckNodeInfoInitialized(&found[i].nodes[0]))
            udsGetNodeInfoUsername(&found[i].nodes[0], g->hostName);

        scanned[nScanned] = found[i];
        ++nScanned;
    }
    free(found);
    return nScanned;
}

bool netJoin(int index)
{
    if (!started || inGroup || index < 0 || index >= nScanned)
        return false;
    if (failed(udsConnectNetwork(&scanned[index].network, passphrase,
                                 sizeof(passphrase), &udsBinding,
                                 UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client,
                                 DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE)))
        return false;

    inGroup = true;
    hosting = false;
    return true;
}

bool netLockJoining(bool lock)
{
    if (!inGroup || !hosting)
        return false;
    return !failed(udsSetNewConnectionsBlocked(lock, true, false));
}

void netLeave(void)
{
    if (!inGroup)
        return;
    udsUnbind(&udsBinding);
    if (hosting)
        udsDestroyNetwork();
    else
        udsDisconnectNetwork();
    inGroup = false;
    hosting = false;
}

bool netConnected(void)
{
    if (!inGroup)
        return false;
    if (hosting)
        return true;

    udsConnectionStatus status;
    if (failed(udsGetConnectionStatus(&status)))
        return false;
    /* The host is node 1: bit 0. */
    return (status.node_bitmask & 1) != 0;
}

int netMyNode(void)
{
    udsConnectionStatus status;
    if (!inGroup || failed(udsGetConnectionStatus(&status)))
        return 0;
    return status.cur_NetworkNodeID;
}

unsigned netNodeMask(void)
{
    udsConnectionStatus status;
    if (!inGroup || failed(udsGetConnectionStatus(&status)))
        return 0;
    return status.node_bitmask;
}

bool netIsHost(void)
{
    return inGroup && hosting;
}

bool netNodeName(int node, char *name, size_t size)
{
    udsNodeInfo info;
    char tmp[NET_NAME_MAX * 4 + 1] = "";

    if (size > 0)
        name[0] = '\0';
    if (!inGroup || failed(udsGetNodeInformation(node, &info)))
        return false;
    if (failed(udsGetNodeInfoUsername(&info, tmp)))
        return false;
    snprintf(name, size, "%s", tmp);
    return true;
}

bool netSend(int node, const void *data, size_t size)
{
    if (!inGroup || size > NET_MAX_PACKET)
        return false;
    u16 dst = node == NET_ALL_NODES ? UDS_BROADCAST_NETWORKNODEID : node;
    Result r = udsSendTo(dst, DATA_CHANNEL, UDS_SENDFLAG_Default, data, size);
    lastResult = r;
    return !UDS_CHECK_SENDTO_FATALERROR(r);
}

size_t netRecv(void *buf, size_t size, int *from)
{
    size_t got = 0;
    u16 src = 0;

    if (!inGroup || !udsWaitDataAvailable(&udsBinding, false, false))
        return 0;
    if (failed(udsPullPacket(&udsBinding, buf, size, &got, &src)))
        return 0;
    if (from != NULL)
        *from = src;
    return got;
}

unsigned netLastError(void)
{
    return (unsigned)lastResult;
}
