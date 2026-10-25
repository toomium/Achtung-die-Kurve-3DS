/* config -- game configurations.
 * Copyright (C) 2010-2021 Simen Heggestøyl <simenheg@runbox.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gfx.h"

#ifdef __3DS__
#include "platform.h"
#endif

unsigned int scorecap = DEFAULT_SCORECAP;
int gameSpeed = 100;
bool fullscreen = DEFAULT_FULLSCREEN;
bool sound = DEFAULT_SOUND;
bool music = DEFAULT_MUSIC;
bool weapons = DEFAULT_WEAPONS;
bool holes = DEFAULT_HOLES;
bool broadcasts = DEFAULT_BROADCASTS;
bool particleEffects = DEFAULT_PARTICLE_EFFECTS;
bool border = DEFAULT_BORDER;
bool duelmode = DEFAULT_DUELMODE;
bool shake = DEFAULT_SHAKE;
bool debugLog = false;
#ifdef __3DS__
unsigned long long shopOwned = 0;
int shopTheme = -1;
int shopFrame = -1;
int shopBlast = -1;
int testCoins = -1;
#endif
const char *settingNames[] = {"fullscreen", "sound", "music", "weapons",
                              "holes", "broadcasts", "particleEffects",
                              "border", "duelmode", "shake", "debug"};
bool *settings[11] = {&fullscreen, &sound, &music, &weapons, &holes,
                      &broadcasts, &particleEffects, &border, &duelmode,
                      &shake, &debugLog};

/**
 * Return that path of the directory where the config file resides.
 */
char *getConfigPath(void)
{
#ifdef __3DS__
    /* There is no HOME on the 3DS; settings live on the SD card. */
    return strdup(CONFIG_DIR_3DS);
#else
    char *home = getenv("HOME");
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");

    int strlen = snprintf(NULL, 0, "%s/%szatackax",
                          xdg_config_home ? xdg_config_home : home,
                          xdg_config_home ? "" : ".config/");
    char *config_path = calloc(strlen + 1, sizeof(char));
    snprintf(config_path, strlen + 1, "%s/%szatackax",
             xdg_config_home ? xdg_config_home : home,
             xdg_config_home ? "" : ".config/");

    return config_path;
#endif
}

/**
 * Ensure that the directory where the config file resides exists.
 *
 * Return zero on success, or -1 if an error occurred (in which case,
 * errno is set appropriately).
 */
int ensureConfigPath(void)
{
#ifdef __3DS__
    /* mkdir() doesn't create parents, and the emulator's SD card may lack it. */
    mkdir("sdmc:/3ds", S_IRWXU);
#endif
    char *configPath = getConfigPath();
    int status = mkdir(configPath, S_IRWXU);
    free(configPath);
    return status;
}

/**
 * Return the full config file name.
 */
char *getConfigFile(void)
{
    char *configPath = getConfigPath();

    int strlen = snprintf(NULL, 0, "%s/settings.cfg", configPath);
    char *configFile = calloc(strlen + 1, sizeof(char));
    snprintf(configFile, strlen + 1, "%s/settings.cfg", configPath);

    free(configPath);

    return configFile;
}

/**
 * Save the current settings to the config file. Return false if that failed.
 */
bool saveSettings(void)
{
#ifdef __3DS__
    platformTrace("save: start");
#endif
    ensureConfigPath();
#ifdef __3DS__
    platformTrace("save: mkdir");
#endif
    char *filename = getConfigFile();
#ifdef __3DS__
    /* The first write to a newly created or truncated file on the SD card
     * takes about ten seconds. So overwrite an existing file in place (no
     * truncating, it never shrinks, see below) and only create it once. */
    FILE *file = fopen(filename, "r+");
    long oldSize = 0;
    if (file != NULL) {
        /* A file that someone edited may be longer than we write: remember how
         * long, to cover the old rest with blanks. */
        fseek(file, 0, SEEK_END);
        oldSize = ftell(file);
        fseek(file, 0, SEEK_SET);
    } else {
        file = fopen(filename, "w");
    }
    platformTrace("save: fopen");
#else
    FILE *file = fopen(filename, "w");
#endif

    if (file == NULL) {
        if (olvl >= O_NORMAL) {
            fprintf(stderr, "Couldn't open file '%s' for writing.\n",
                    filename);
        }
        free(filename);
        return false;
    }

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(bool*); ++i) {
        fprintf(file, "%s = %d\n", settingNames[i], *settings[i]);
    }

    fprintf(file, "scorecap = %d\n", scorecap);
    fprintf(file, "gamespeed = %d\n", gameSpeed);
#ifdef __3DS__
    fprintf(file, "owned = %llx\n", shopOwned);
    fprintf(file, "theme = %d\n", shopTheme);
    fprintf(file, "frame = %d\n", shopFrame);
    fprintf(file, "blast = %d\n", shopBlast);
    if (testCoins >= 0)
        fprintf(file, "testcoins = %d\n", testCoins);
#endif
    for (unsigned int i = 0; i < MAX_PLAYERS; ++i) {
#ifdef __3DS__
        struct player *sp = slotConfig(i);
        fprintf(file, "%dm = %d\n", i + 1, slotMode[i]);
#else
        struct player *sp = &players[i];
#endif
        fprintf(file, "%dn = %s\n", i + 1, sp->name);
        fprintf(file, "%dc = %d\n", i + 1, sp->color);
#ifdef __3DS__
        fprintf(file, "%dh = %d\n", i + 1, sp->headStyle);
#endif
        fprintf(file, "%da = %d\n", i + 1, sp->ai);
        fprintf(file, "%dl = %d\n", i + 1, sp->lkey);
        fprintf(file, "%dw = %d\n", i + 1, sp->wkey);
        fprintf(file, "%dr = %d\n", i + 1, sp->rkey);
    }

    if (olvl >= O_NORMAL && ferror(file)) {
        fprintf(stderr, "Error writing to file '%s'.\n", filename);
    }
#ifdef __3DS__
    /* Fill the file up to a fixed size with blanks, so that it never shrinks
     * and the old content is overwritten completely. restoreSettings() stops
     * at the blanks. */
    long fill = oldSize > SETTINGS_FILE_SIZE ? oldSize : SETTINGS_FILE_SIZE;
    for (long size = ftell(file); size < fill - 1; ++size)
        fputc(' ', file);
    fputc('\n', file);
    platformTrace("save: fprintf");
#endif

    free(filename);
#ifdef __3DS__
    fflush(file);
    platformTrace("save: fflush (write)");
#endif
    bool ok = !ferror(file);
    ok = fclose(file) == 0 && ok;
#ifdef __3DS__
    platformTrace("save: fclose");
#endif
    return ok;
}

/**
 * Restore previous settings from the config file.
 */
void restoreSettings(void)
{
    char *filename = getConfigFile();
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        if (olvl >= O_DEBUG) {
            fprintf(stderr, "Couldn't restore settings from file %s.\n",
                    filename);
        }
        free(filename);
        return;
    }

    char setting[STRBUF];
    char value[STRBUF];
    int line = 0;
    bool valid = false;

    while ((fscanf(file, "%31s = %31[^\n]\n", setting, value)) != EOF) {
        valid = false;
        for (unsigned int i = 0; i < sizeof(settings)/sizeof(bool*); ++i) {
            if (strncmp(settingNames[i], setting, STRBUF) == 0) {
                /* We have a matched setting */
                *settings[i] = atoi(value);
                valid = true;
                break;
            }
        }
        if (!valid && strncmp("scorecap", setting, STRBUF) == 0) {
            scorecap = atoi(value);
            valid = true;
        }
        else if (!valid && strncmp("gamespeed", setting, STRBUF) == 0) {
            gameSpeed = atoi(value);
            valid = true;
        }
#ifdef __3DS__
        else if (!valid && strncmp("owned", setting, STRBUF) == 0) {
            shopOwned = strtoull(value, NULL, 16);
            valid = true;
        }
        else if (!valid && strncmp("theme", setting, STRBUF) == 0) {
            shopTheme = atoi(value);
            valid = true;
        }
        else if (!valid && strncmp("frame", setting, STRBUF) == 0) {
            shopFrame = atoi(value);
            valid = true;
        }
        else if (!valid && strncmp("blast", setting, STRBUF) == 0) {
            shopBlast = atoi(value);
            valid = true;
        }
        else if (!valid && strncmp("testcoins", setting, STRBUF) == 0) {
            testCoins = atoi(value);
            valid = true;
        }
#endif
        else if (!valid && setting[0] >= '1' && setting[0] <= '0' + MAX_PLAYERS) {
            struct player *p = &players[setting[0] - '0' - 1];
            switch (setting[1]) {
#ifdef __3DS__
            case 'm': /* Slot mode: off, AI, human */
                slotMode[setting[0] - '0' - 1] = atoi(value);
                valid = true;
                break;
#endif
            case 'n': /* Name */
                snprintf(p->name, sizeof(p->name), "%.*s", PLAYER_NAME_LEN - 1,
                         value);
                valid = true;
                break;
#ifdef __3DS__
            case 'h': /* Head marker from the shop */
                p->headStyle = atoi(value);
                valid = true;
                break;
#endif
            case 'c': /* Color */
                p->color = (unsigned)atoi(value) < N_COLORS ? atoi(value)
                                                            : setting[0] - '1';
                valid = true;
                break;
            case 'a': /* AI */
                p->ai = atoi(value);
                valid = true;
                break;
            case 'l': /* Left key */
                p->lkey = atoi(value);
                valid = true;
                break;
            case 'w': /* Weapon key */
                p->wkey = atoi(value);
                valid = true;
                break;
            case 'r': /* Right key */
                p->rkey = atoi(value);
                valid = true;
                break;
            default:
                break;
            }
        }
        if (!valid && olvl >= O_NORMAL) {
            fprintf(stderr, "Unknown config format in '%s', line "
                    "%d: %s\n", filename, line, setting);
        }
        ++line;
    }

    free(filename);
    fclose(file);
}
