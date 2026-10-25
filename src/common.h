#ifndef COMMON_H
#define COMMON_H

#define STRBUF  32

/* DEFAULT WINDOW DIMENSIONS */
#ifdef __3DS__
/* The playfield is the top screen. */
#define DEFAULT_WINDOW_W    400
#define DEFAULT_WINDOW_H    240
/* Menus and scores live on the bottom screen, which is 320 pixels wide. In the
 * 400 pixel wide dual screen surface it shows the columns from BOTTOM_VIEW_X. */
#define BOTTOM_VIEW_W       320
#define BOTTOM_VIEW_H       240
#define BOTTOM_VIEW_X       40
/* A calm dark blue instead of black behind menus, text and the playfield. The
 * trail surface keeps black as its transparent color, see gameScreen. */
/* The version of the 3DS port, shown on the title screen. */
#define PORT_VERSION        "0.4.1"

#define BG_R                0x1B
#define BG_G                0x21
#define BG_B                0x40
#else
#define BG_R                0x00
#define BG_G                0x00
#define BG_B                0x00
#define DEFAULT_WINDOW_W    800
#define DEFAULT_WINDOW_H    600
#endif
#define SCREEN_BPP          32

/* PLAYERS */
#define MIN_PLAYERS         2   /* Minimum number of players */
#define MAX_PLAYERS         8   /* Maximum number of players */
#define DEFAULT_N_PLAYERS   2   /* Default number of players */

/* SPAWNING */
#ifdef __3DS__
#define START_ROUND_WAIT    3000 /* Milliseconds until the game starts: 3, 2, 1 */
#else
#define START_ROUND_WAIT    1500 /* Milliseconds until the game starts */
#endif
#ifdef __3DS__
#define SPAWN_SPACE_MIN     50   /* Minimal spawn distance from screen edges */
#else
#define SPAWN_SPACE_MIN     100  /* Minimal spawn distance from screen edges */
#endif
#ifdef __3DS__
#define ZATA_SPAWN_SPACING  10   /* Minimal spawn distance between zatas */
#else
#define ZATA_SPAWN_SPACING  20   /* Minimal spawn distance between zatas */
#endif
#define TRY_SPAWN_THIS_HARD 40   /* Try this many times to make a fit */

/* OUTPUT LEVEL */
#define O_SILENT    0   /* Be absolutely quiet */
#define O_NORMAL    1   /* Standard */
#define O_VERBOSE   2   /* Important events (good for game logs, maybe?) */
#define O_DEBUG     3   /* Debug mode */

#ifdef SILENT
static const int olvl = O_SILENT;
#elif VERBOSE
static const int olvl = O_VERBOSE;
#elif DEBUG
static const int olvl = O_DEBUG;
#else
static const int olvl = O_NORMAL;
#endif

void refreshGameScreen(void);
void clearRecents(int fromX, int toX, int fromY, int toY);

#endif
