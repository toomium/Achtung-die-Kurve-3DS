#ifndef CONFIG_H
#define CONFIG_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"

#ifdef __3DS__
#ifndef CONFIG_DIR_3DS
#define CONFIG_DIR_3DS "sdmc:/3ds/zatackax"
#endif
#define SETTINGS_FILE_SIZE 1024
#endif
#include "player.h"

#define DEFAULT_FULLSCREEN       false
#define DEFAULT_SOUND            true
#define DEFAULT_MUSIC            true
#define DEFAULT_WEAPONS          true
#define DEFAULT_HOLES            true
#define DEFAULT_BROADCASTS       true
#define DEFAULT_PARTICLE_EFFECTS true
#define DEFAULT_BORDER           false
#define DEFAULT_DUELMODE         false
#define DEFAULT_SHAKE            true
#define DEFAULT_SCORECAP         0

extern unsigned int scorecap;
extern int gameSpeed;      /* Percent, 100 is the normal speed */
extern bool fullscreen;
extern bool sound;
extern bool music;
extern bool weapons;
extern bool holes;
extern bool broadcasts;
extern bool particleEffects;
extern bool border;
extern bool duelmode;
extern bool debugLog;      /* Write sdmc:/zatackax.log (3DS), set in settings.cfg only */
extern bool shake;         /* The playfield shakes when somebody crashes (3DS) */
#ifdef __3DS__
extern unsigned long long shopOwned; /* Bit mask of the items bought in the shop */
extern int shopTheme;      /* The playfield theme in use, -1 for the normal one */
extern int shopPattern;    /* The playfield pattern in use, -1 for none */
extern int shopFrame;      /* The frame color in use, -1 for the normal one */
extern int shopBlast;      /* The crash effect in use, -1 for the normal one */
extern int testCoins;      /* Play Coins for a console without them, -1 for off */
#endif

int ensureConfigPath(void);
char *getConfigFile(void);
bool saveSettings(void);
void restoreSettings(void);

#endif
