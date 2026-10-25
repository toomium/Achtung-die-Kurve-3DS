#include <3ds.h>
#include <stdio.h>

#include "platform.h"

/* The system tells when the game is suspended; the hook runs on the main thread
 * inside aptMainLoop(), which SDL calls whenever it looks for events. */
static aptHookCookie aptCookie;
static volatile bool wasSuspended = false;

static void aptEvent(APT_HookType hook, void *param)
{
    (void)param;
    if (hook == APTHOOK_ONSUSPEND || hook == APTHOOK_ONSLEEP)
        wasSuspended = true;
}

bool platformWasSuspended(void)
{
    bool was = wasSuspended;
    wasSuspended = false;
    return was;
}

unsigned int platformMillis(void)
{
    return (unsigned int)osGetTime();
}

#define TRACE_MAX 40
static const char *traceLabel[TRACE_MAX];
static unsigned int traceTime[TRACE_MAX];
static int traceCount;

void platformTrace(const char *label)
{
    if (traceCount < TRACE_MAX) {
        traceLabel[traceCount] = label;
        traceTime[traceCount] = platformMillis();
        ++traceCount;
    }
}

void platformTraceDump(void)
{
    for (int i = 1; i < traceCount; ++i)
        fprintf(stderr, "trace: %-22s +%u ms\n", traceLabel[i],
                traceTime[i] - traceTime[i - 1]);
    traceCount = 0;
}

bool platformInit(void)
{
    /* No effect on the old 3DS; the New 3DS runs at 804 instead of 268 MHz. */
    osSetSpeedupEnable(true);

    aptHook(&aptCookie, aptEvent, NULL);

    /* The game data lives in romfs: (see DATADIR in 3ds/Makefile). */
    Result rc = romfsInit();

    return R_SUCCEEDED(rc);
}

bool platformTextInput(char *text, size_t size, const char *hint)
{
    char input[64] = "";
    SwkbdState swkbd;

    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, size - 1);
    swkbdSetHintText(&swkbd, hint);
    swkbdSetInitialText(&swkbd, text);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);

    if (swkbdInputText(&swkbd, input, sizeof(input)) != SWKBD_BUTTON_CONFIRM)
        return false;

    snprintf(text, size, "%s", input);
    return true;
}

/* ---- Play Coins ---- */

#define COIN_EXTDATA_ID 0xF000000Bu
#define COIN_FILE       "/gamecoin.dat"
#define COIN_MAGIC      0x4F00u
#define COIN_HEADER     0x14        /* Bytes we look at: magic, total, day, steps */
#define COIN_TOTAL_AT   4           /* u16: the total number of coins */
#define COIN_MAX        300

/* Opens the file with the given flags; the archive stays open until the file
 * is closed with coinsClose(). */
static bool coinsOpen(FS_Archive *archive, Handle *file, u32 flags)
{
    /* The high word of the id is 0x00048000 on a console. Try 0 as well, which
     * some emulators expect. */
    static const u32 highWords[] = {0x00048000, 0};

    for (unsigned int i = 0; i < sizeof(highWords) / sizeof(highWords[0]); ++i) {
        u32 id[3] = {MEDIATYPE_NAND, COIN_EXTDATA_ID, highWords[i]};
        FS_Path archivePath = {PATH_BINARY, sizeof(id), id};
        Result rc = FSUSER_OpenArchive(archive, ARCHIVE_SHARED_EXTDATA,
                                       archivePath);
        if (R_FAILED(rc)) {
            fprintf(stderr, "coins: open archive 0x%08x: 0x%08lx\n",
                    (unsigned)highWords[i], (unsigned long)rc);
            continue;
        }
        rc = FSUSER_OpenFile(file, *archive, fsMakePath(PATH_ASCII, COIN_FILE),
                             flags, 0);
        if (R_SUCCEEDED(rc))
            return true;
        fprintf(stderr, "coins: open file: 0x%08lx\n", (unsigned long)rc);
        FSUSER_CloseArchive(*archive);
    }
    return false;
}

static void coinsClose(FS_Archive archive, Handle file)
{
    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);
}

/* Reads the header and checks it: the magic number and a sane total. */
static bool coinsReadTotal(Handle file, unsigned int *total)
{
    u8 head[COIN_HEADER];
    u64 size = 0;
    u32 got = 0;

    if (R_FAILED(FSFILE_GetSize(file, &size)) || size < COIN_HEADER)
        return false;
    if (R_FAILED(FSFILE_Read(file, &got, 0, head, sizeof(head)))
        || got != sizeof(head))
        return false;

    u32 magic = head[0] | head[1] << 8 | head[2] << 16 | (u32)head[3] << 24;
    unsigned int count = head[COIN_TOTAL_AT] | head[COIN_TOTAL_AT + 1] << 8;
    if (magic != COIN_MAGIC || count > COIN_MAX) {
        fprintf(stderr, "coins: unexpected file, magic 0x%x total %u\n",
                (unsigned)magic, count);
        return false;
    }
    *total = count;
    return true;
}

bool platformCoinsRead(unsigned int *coins)
{
    FS_Archive archive;
    Handle file;
    unsigned int total = 0;

    if (!coinsOpen(&archive, &file, FS_OPEN_READ))
        return false;
    bool ok = coinsReadTotal(file, &total);
    coinsClose(archive, file);

    if (ok)
        *coins = total;
    return ok;
}

bool platformCoinsSpend(unsigned int amount)
{
    FS_Archive archive;
    Handle file;
    unsigned int total = 0;
    unsigned int after = 0;
    bool ok = false;

    if (!coinsOpen(&archive, &file, FS_OPEN_READ | FS_OPEN_WRITE))
        return false;

    if (coinsReadTotal(file, &total) && total >= amount) {
        unsigned int left = total - amount;
        u8 bytes[2] = {left & 0xFF, left >> 8};
        u32 written = 0;
        Result rc = FSFILE_Write(file, &written, COIN_TOTAL_AT, bytes,
                                 sizeof(bytes), FS_WRITE_FLUSH);
        ok = R_SUCCEEDED(rc) && written == sizeof(bytes)
            && coinsReadTotal(file, &after) && after == left;
        fprintf(stderr, "coins: spend %u of %u: %s (now %u)\n", amount, total,
                ok ? "ok" : "FAILED", after);
    }
    coinsClose(archive, file);
    return ok;
}

void platformOpenLog(void)
{
    if (freopen("sdmc:/zatackax.log", "w", stderr) != NULL)
        setvbuf(stderr, NULL, _IONBF, 0);
}

void platformExit(void)
{
    fprintf(stderr, "exit: platformExit t=%u\n", platformMillis());
    aptUnhook(&aptCookie);
    romfsExit();
}
