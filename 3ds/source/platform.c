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
