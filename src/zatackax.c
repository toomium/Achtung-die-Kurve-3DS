/* zatackax -- main game module.
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

#include <strings.h>
#include <time.h>

#include "zatackax.h"

#ifdef __3DS__
#include "platform.h"
#include "ui3ds.h"
#include "touch3ds.h"
#include "netmenu3ds.h"
#include "netgame3ds.h"
#include "demo3ds.h"
#include "shop3ds.h"
#include "slotmenu3ds.h"
#include "stats3ds.h"
#include "profile3ds.h"
#include "items3ds.h"
#include "tournament3ds.h"
#endif

static const unsigned int FPS_CAP = 100;

/* In the bounce mode a head turns back this far from the edge. */
#define BOUNCE_EDGE 2

/* With closing walls: after this long they start to come in, this fast. */
#define CLOSE_AFTER_MS  10000
#define CLOSE_SPEED     4.0     /* Pixels per second at the sides */

/* A computer player uses a weapon that it found much sooner than one it was
 * given at the start. */
#ifdef __3DS__
#define aiWeaponRate() (itemMode ? 40.0 : 1.0)
#else
#define aiWeaponRate() 1.0
#endif

/* The background of the playfield: a theme from the shop on the 3DS. */
#ifdef __3DS__
#define clearPlayfield(s) shopClearPlayfield(s)
#else
#define clearPlayfield(s) clearBackground(s)
#endif

/* On the 3DS: 0 PLAY, 1 OPTIONS, 2 EXIT, 3 SHOP, 4 PROFILE. */
struct menu menuMain = {
#ifdef __3DS__
    5,
#else
    3,
#endif
    0,
};

struct menu menuSettings = {
    12,
    0,
};

struct menu menuPlayer = {
    MAX_PLAYERS + 1,
    0,
};

struct menu menuPConf = {
    7,
    0,
};

static struct scene mainMenu = {
    logicMainMenu,
    displayMainMenu,
    NULL
};

static struct scene wepMenu = {
    logicWepMenu,
    displayWepMenu,
    &mainMenu
};

static struct scene gameStart = {
    logicGameStart,
    displayGameStart,
    &mainMenu
};

static struct scene game = {
    logicGame,
    displayGame,
    &mainMenu
};

#ifdef __3DS__
/* The pause menu; parentScene is not used, see pausedScene. */
static struct scene pauseMenu = {
    logicPause,
    displayPause,
    NULL
};

/* The count down 3, 2, 1 after the pause, before the game goes on. */
static struct scene resumeScene = {
    logicResume,
    displayResume,
    NULL
};
#endif

#ifdef __3DS__
static struct scene modeMenu = {
    logicModeMenu,
    displayModeMenu,
    &mainMenu
};

/* The first page: the logo over the animation, "press A". */
static struct scene titleScene = {
    logicTitle,
    displayTitle,
    NULL
};

static struct scene lobbyMenu = {
    logicLobby,
    displayLobby,
    &modeMenu
};

static struct scene rulesMenu = {
    logicRules,
    displayRules,
    &lobbyMenu
};

/* After a round and after a match; no way back, so no parent scene. */
static struct scene roundEndScene = {
    logicRoundEnd,
    displayRoundEnd,
    NULL
};

static struct scene matchEndScene = {
    logicMatchEnd,
    displayMatchEnd,
    NULL
};
#endif

static struct scene settingsMenu = {
    logicSettingsMenu,
    displaySettingsMenu,
    &mainMenu
};

static struct scene playerMenu = {
    logicPlayerMenu,
    displayPlayerMenu,
    &settingsMenu
};

static struct scene pConfMenu = {
    logicPConfMenu,
    displayPConfMenu,
    &playerMenu
};

struct scene *curScene = NULL;

struct player players[MAX_PLAYERS];
unsigned char nPlayers = DEFAULT_N_PLAYERS;

#ifdef __3DS__
unsigned char slotMode[MAX_PLAYERS];

/* The player who has reached the score cap, or -1: the match end page follows. */
static int matchWinner = -1;

/* Screen shake after a crash: time left (ms) and the offset of the playfield. */
#define SHAKE_TIME 300
static int shakeLeft = 0;
static int shakeX = 0;
static int shakeY = 0;
#endif

/**
 * Stage 1 of player initialization.
 * Assigns keys and colors.
 */
void initPlayers1(void)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
        resetPlayer(i);

#ifdef __3DS__
    /* Default lineup: you against one computer player. */
    memset(slotMode, SLOT_OFF, sizeof(slotMode));
    slotMode[0] = SLOT_HUMAN;
    slotMode[1] = SLOT_AI;
#endif
}

/**
 * Copies a sprite into `dst`, which has the same size.
 *
 * The balls and arrows are gray images drawn for a black background. On the 3DS
 * the background is blue, so the brightness of each pixel becomes its alpha
 * value and the color becomes white: black turns transparent, and the soft edge
 * fades into the background. colorFill() tints the result.
 */
static void copySprite(SDL_Surface *src, SDL_Surface *dst)
{
#ifdef __3DS__
    SDL_SetAlpha(src, 0, SDL_ALPHA_OPAQUE);  /* copy, don't blend */
    SDL_BlitSurface(src, NULL, dst, NULL);
    SDL_SetAlpha(src, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);

    SDL_LockSurface(dst);
    Uint32 *pixel = dst->pixels;
    for (int i = 0; i < dst->w * dst->h; ++i, ++pixel) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(*pixel, dst->format, &r, &g, &b, &a);
        Uint8 brightness = r > g ? (r > b ? r : b) : (g > b ? g : b);
        *pixel = SDL_MapRGBA(dst->format, 255, 255, 255, brightness);
    }
    SDL_UnlockSurface(dst);
#else
    SDL_BlitSurface(src, NULL, dst, NULL);
#endif
}

/**
 * Stage 2 of player initialization.
 * Called at a later point than stage 1. Assigns arrow sprites and resets
 * scores.
 */
void initPlayers2(void)
{
    int i;
    struct player *p = &players[0];
    SDL_Surface **s = parrows;

    for (i = 0; i < nPlayers; ++i, ++p, ++s) {

        p->active = i + 1;
        p->score = 0;

        /* Assign arrows */
        copySprite(images[IMG_ARROWS], *s);
        SDL_LockSurface(*s);
        colorFill(colors[p->color], *s);
        SDL_UnlockSurface(*s);
        p->arrow = *s;
    }

    for (; i < MAX_PLAYERS; ++i, ++p) {
        p->active = 0;
    }
}

/**
 * Reset a player struct.
 *
 * @param player ID of the player to be reset.
 */
static const char *const colorNames[N_COLORS] = {
    "Red", "Blue", "Green", "Yellow", "Pink", "Cyan", "White", "Orange"
#ifdef __3DS__
    , "Purple", "Rose", "Teal", "Lime", "Gold", "Violet", "Sky", "Coral",
    "Mint", "Peach", "Lavender", "Crimson", "Tan", "Silver", "Azure", "Olive"
#endif
};

const char *colorName(int color)
{
    return colorNames[color % N_COLORS];
}

/* A name that is just the name of a color: the player has not chosen one. */
bool nameIsDefault(const char *name)
{
    for (int i = 0; i < N_COLORS; ++i) {
        if (strcasecmp(name, colorNames[i]) == 0)
            return true;
    }
    return false;
}

void resetPlayer(int player)
{
    struct player *p = &(players[player]);
    p->color = player;
    p->speed = 1.0;
    p->invertedKeys = false;
    p->weapon = false;
    p->ai = false;
    strncpy(p->name, colorNames[player], PLAYER_NAME_LEN - 1);

    switch (player) {
    case 0:
#ifdef __3DS__
        p->lkey = JOY_BUTTON(0, N3DS_BUTTON_L);
        p->rkey = JOY_BUTTON(0, N3DS_BUTTON_R);
        p->wkey = JOY_BUTTON(0, N3DS_BUTTON_X);
#else
        p->lkey = SDLK_LEFT; p->rkey = SDLK_RIGHT;
        p->wkey = SDLK_UP;
#endif
        break;
    case 1:
        p->lkey = 'z'; p->rkey = 'c'; p->wkey = 'x';
        break;
    case 2:
        p->lkey = 'v'; p->rkey = 'n'; p->wkey = 'b';
        break;
    case 3:
        p->lkey = ','; p->rkey = '-'; p->wkey = '.';
        break;
    case 4:
        p->lkey = 'q'; p->rkey = 'e'; p->wkey = 'w';
        break;
    case 5:
        p->lkey = 'r'; p->rkey = 'y'; p->wkey = 't';
        break;
    case 6:
        p->lkey = 'i'; p->rkey = 'p'; p->wkey = 'o';
        break;
    case 7:
        p->lkey = SDLK_F1; p->rkey = SDLK_F3; p->wkey = SDLK_F2;
        break;
    default:
        break;
    }
}

/**
 * Sets all selected weapons back to first.
 */
void deselectWeapons(void)
{
    for (int i = 0; i < nPlayers; ++i)
        (&players[i])->weapon = 0;
}

/**
 * Extract and return an array containing the current player colors.
 *
 * @return Current player colors.
 */
SDL_Color *extractPColors(void)
{
    SDL_Color *pcolors = malloc(sizeof(SDL_Color) * MAX_PLAYERS);
    for (int i = 0; i < MAX_PLAYERS; ++i)
        pcolors[i] = colors[players[i].color];
    return pcolors;
}

/**
 * Kills target player.
 *
 * @param victim ID of the player that was victim.
 * @param killer ID of the player that was crashed into.
 */
void killPlayer(unsigned char victim, unsigned char killer)
{
    playSound(SOUND_CRASH, sound);

    if (broadcasts) {
        char msg[BROADC_BUF];

        if (killer == 0)
            snprintf(msg, BROADC_BUF, "%d; hit the wall", victim);
        else if (killer == victim)
            snprintf(msg, BROADC_BUF, "%d; committed suicide", victim);
        else
            snprintf(msg, BROADC_BUF, "%d; crashed into ;%d", victim, killer);

        /* Send forward those colors */
        newBroadcast(msg);
    }

    struct player *p = &players[victim - 1];
    --alivecount;
    p->alive = false;
#ifdef __3DS__
    netKiller[victim - 1] = killer;
#endif

#ifdef __3DS__
    if (shake)
        shakeLeft = SHAKE_TIME;
#endif

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        struct player *pt = &players[i];
        if (pt->active) {
            if (pt->alive) {
                pt->score++;
                if (pt->score == scorecap) {
#ifdef __3DS__
                    /* logicGame() shows the match end page. */
                    matchWinner = i;
                    winnerDeclared = true;
#else
                    playerWon(i);
                    screenFreeze = true;
#endif
                }
            }
        } else {
            break;
        }
    }

    if (particleEffects) {
        SDL_Color color = colors[p->color];
#ifdef __3DS__
        shopCrashBurst(p->posx, p->posy, p->dir, color);
#else
        addParticles(50,
                     p->posx, p->posy,
                     0, 0.5,
                     p->dir + M_PI - M_PI_4, p->dir + M_PI + M_PI_4,
                     1, 1, 0.0005,
                     color.r, color.r,
                     color.b, color.b,
                     color.g, color.g);
#endif
    }

    refreshGameScreen(); /* Update scores */
}

/**
 * Add a new broadcast message to the queue.
 *
 * @param msg Message to broadcast
 */
void newBroadcast(char *msg)
{
    SDL_Color *pcolors = extractPColors();
    colorAddBroadcast(msg, pcolors);
    free(pcolors);
}

/**
 * Shows which player won the game.
 * TODO: Handle ties.
 *
 * @param id ID of the player that won.
 */
void playerWon(unsigned char id)
{
    if (broadcasts) {
        char msg[BROADC_BUF];
        snprintf(msg, BROADC_BUF, "%d; won the game! (press RETURN to play "
                 "again, or ESC to exit)", id + 1);
        newBroadcast(msg);
        /* Make sure the winning announcement is drawn. */
        refreshGameScreen();
        displayGame();
    }

    winnerDeclared = true;

    if (olvl >= O_VERBOSE)
        printf(" -- Player %d won! --\n", id + 1);
}

/**
 * Provides a random position/direction vector.
 *
 * @return The velocity vector of the newly spawned player.
 */
struct vel spawn(void)
{
    double rnd;
    struct vel initPos;

    rnd = (double)rand() / RAND_MAX;
    initPos.x = SPAWN_SPACE_MIN
        + (rnd * (WINDOW_W - (2 * SPAWN_SPACE_MIN)));
    rnd = (double)rand() / RAND_MAX;
    initPos.y = SPAWN_SPACE_MIN
        + (rnd * (WINDOW_H - (2 * SPAWN_SPACE_MIN)));
    rnd = (double)rand() / RAND_MAX;
    initPos.holecount = (rnd * (HOLE_FREQ - HOLE_FIRST_DELAY));
    rnd = (double)rand() / RAND_MAX;
    initPos.dir = rnd * (2 * M_PI);
    if (initPos.dir < 0)
        initPos.dir *= -1;

    return initPos;
}

/**
 * Spawns a player randomly on the map.
 *
 * @param p The player which is to be spawned.
 */
void respawn(struct player *p)
{
    struct vel initPos;
    int posOK = 1;
    int n = TRY_SPAWN_THIS_HARD;

    while (posOK && n != 0) {
        posOK = p->active - 1;
        initPos = spawn();
        p->posx = initPos.x;
        p->posy = initPos.y;
        for (int i = 0; i < p->active - 1; ++i) {
            struct player *comp = &players[i];
            /* Securing spawning space between zatas. This may cause
             * trouble at small maps (will never get a fit).
             * ZATA_SPAWN_SPACING cuts off the waiting. */
            if (fabs(p->posx - comp->posx) > ZATA_SPAWN_SPACING
                && fabs(p->posy - comp->posy) > ZATA_SPAWN_SPACING) {
                --posOK;
            }
        }
        --n;
    }

    p->alive = true;
    p->dir = initPos.dir;

    p->initposx = p->posx;
    p->initposy = p->posy;
    p->initdir = p->dir;

    p->prevx = p->posx;
    p->prevy = p->posy;

    p->holecount = initPos.holecount;
}

/**
 * Spawns a player for duel mode.
 *
 * @param p The player which is to be spawned.
 */
void drespawn(struct player *p)
{
    p->alive = true;
    p->posx = (WINDOW_W / 2) + (p->active == 1 ? -120 : 120);
    p->posy = WINDOW_H / 2;
    p->dir = p->active == 1 ? 0 : M_PI;

    p->initposx = p->posx;
    p->initposy = p->posy;
    p->initdir = p->dir;

    p->prevx = p->posx;
    p->prevy = p->posy;
    p->holecount = (rand() % (HOLE_FREQ - HOLE_FIRST_DELAY));
}

/**
 * Sets a player's color index up or down.
 *
 * @param pedit ID of the player which color is to be changed.
 * @param up 1 if the color should be set one higher, 0 if one lower.
 */
void setColor(unsigned char pedit, bool up)
{
    if (up) {
        if ((&players[pedit])->color == N_COLORS - 1)
            (&players[pedit])->color = 0;
        else
            ++((&players[pedit])->color);
    } else {
        if ((&players[pedit])->color == 0)
            (&players[pedit])->color = N_COLORS - 1;
        else
            --((&players[pedit])->color);
    }
}

/**
 * Capture and set the next key pressed as a player's directional or
 * weapon key.
 *
 * @param pedit ID of the player to edit.
 * @param key l - left key, r - right key, w - weapon key
 */
void setNextKey(unsigned char pedit, unsigned char key)
{
    for (;;) {
        while (SDL_PollEvent(&event)) {
#ifdef __3DS__
            /* A touch is no button to bind. */
            if (event.type == SDL_MOUSEBUTTONDOWN)
                continue;
#endif
            if (event.type == SDL_KEYDOWN
                    || event.type == SDL_MOUSEBUTTONDOWN
                    || event.type == SDL_JOYBUTTONDOWN
                    || event.type == SDL_JOYAXISMOTION) {

                int k;
                char *keyname;

                if (event.type == SDL_JOYBUTTONDOWN) {
                    Uint8 joyIndex = event.jbutton.which + 1;
                    k = event.jbutton.button | joyIndex << 9;
                    keyname = buttonName(k);
                }
                else if (event.type == SDL_JOYAXISMOTION) {
                    Uint8 joyIndex = event.jaxis.which + 1;
                    k = axisNumber(event.jaxis);
                    k = (k << 4) | (joyIndex << 9);
                    keyname = buttonName(k);
                }
                else {
                    if (event.type == SDL_KEYDOWN) {
                        k = event.key.keysym.sym;
                    }
                    else {
                        k = event.button.button;
                    }

                    keyname = buttonName(k);
                }

                if (keyname[0] == '\0') {
                    if (olvl >= O_DEBUG)
                        fprintf(stderr, "Unknown key\n");
                } else {
                    if (olvl >= O_DEBUG)
                        fprintf(stderr, "Set new key: %s\n", keyname);
                }

                switch (key) {
                case 'l':
                    (&players[pedit])->lkey = k;
                    break;
                case 'r':
                    (&players[pedit])->rkey = k;
                    break;
                case 'w':
                    (&players[pedit])->wkey = k;
                default:
                    break;
                }

                free(keyname);
                return;
            }
        }
    }
}

/**
 * Catches and sets the next string entered as the given players name.
 *
 * @param pedit ID of the player to rename.
 */
void setNextName(unsigned char pedit)
{
    struct player *p = &players[pedit];
#ifdef __3DS__
    /* There is no keyboard: use the system's on-screen one. */
    platformTextInput(p->name, PLAYER_NAME_LEN, "Player name");
    return;
#endif
    bool keyDown[322];

    memset(keyDown, '\0', 322);
    memset(p->name, '\0', PLAYER_NAME_LEN);
    displayPConfMenu();
    int chars = 0;

    for (;;) {

        SDL_PollEvent(&event);

        if (chars >= PLAYER_NAME_LEN - 1) {
            playSound(SOUND_BEEP, sound);
            return;
        }

        if (event.type == SDL_KEYDOWN) {

            int k = event.key.keysym.sym;

            if (!keyDown[k]) {
                keyDown[k] = 1;

                if (k >= SDLK_EXCLAIM && k <= SDLK_z) {
                    if (event.key.keysym.mod & KMOD_LSHIFT ||
                        event.key.keysym.mod & KMOD_RSHIFT)
                        snprintf(p->name + chars, PLAYER_NAME_LEN, "%c", k - 32);
                    else
                        snprintf(p->name + chars, PLAYER_NAME_LEN, "%c", k);
                    ++chars;
                } else if (k == SDLK_BACKSPACE && chars > 0) {
                    --chars;
                    snprintf(p->name + chars, PLAYER_NAME_LEN, "%c", '\0');
                }

                displayPConfMenu();
            }

            if (chars > 0 &&
                (k == SDLK_ESCAPE || k == SDLK_RETURN ||
                 k == SDLK_DOWN   || k == SDLK_UP)) {

                playSound(SOUND_BEEP, sound);

                if (k == SDLK_DOWN)
                    menuPConf.choice++;
                else if (k == SDLK_UP)
                    menuPConf.choice = menuPConf.choices - 1;
                return;
            }
        } else if (event.type == SDL_KEYUP)
            keyDown[event.key.keysym.sym] = 0;
    }
}

/**
 * Initializes the hitmap.
 *
 * @param w Width of the map.
 * @param h Height of the map.
 */
void initHitMap(unsigned int w, unsigned int h)
{
    size_t len = sizeof(bool) * w * h;
    free(hitmap);
    hitmap = calloc(len, sizeof(unsigned char));
    recents = NULL;
}

/**
 * Adds a new piece to a queue, later to be added to the hitmap.
 *
 * Also draws the corresponding pixel onto the screen, and detects when
 * players collide. (This function should be considered split up.)
 * updateHitMap() actually puts the pieces into the map.
 *
 * @param x x coordinate of the piece.
 * @param y y coordinate of the piece.
 * @param player ID of the player who owns the piece.
 * @param modifier Used to create special offset values on the piece, for
 * example for hole detection.
 * @see updateHitMap
 */
void addToHitMap(unsigned int x, unsigned int y, unsigned char player,
                 unsigned char modifier)
{
    if (olvl >= O_DEBUG)
        fprintf(stderr, "Added to hitmap: %d, %d...\n", x, y);

    SDL_LockSurface(gameScreen);

    for (int i = -TOLERANCE; i <= TOLERANCE; ++i) {
        for (int j = -TOLERANCE; j <= TOLERANCE; ++j) {

            if (abs(i) + abs(j) > TOLERANCE + 1)
                continue;

            int xpx = x + i;
            int ypx = y + j;

            if (xpx >= 0 && xpx < (int)WINDOW_W && ypx >= 0
                && ypx < (int)WINDOW_H) {

                unsigned char *hit =
                    &hitmap[sizeof(bool) * ((WINDOW_W * ypx) + xpx)];
                struct player *p = &players[player - 1];

                putPixel(xpx, ypx, colors[p->color], gameScreen);

                if (*hit == 0) {
                    struct recentMapPiece *new
                        = malloc(sizeof(struct recentMapPiece));
                    *hit = player + MAX_PLAYERS + modifier;
                    new->count = COUNTDOWN_TOLERANCE;
                    new->x = xpx;
                    new->y = ypx;
                    new->next = recents;
                    recents = new;
                } else if (*hit != player + MAX_PLAYERS &&
                           *hit != player + MAX_PLAYERS * 2) {
                    if (p->inv_self && player == *hit)
                        break;
                    if (player == *hit) {
                        if (olvl >= O_VERBOSE)
                            printf("Player %d committed suicide!\n", player);
                        killPlayer(player, *hit);
                    } else if (!p->inv_others) {
                        int killer = *hit;
                        while (killer > MAX_PLAYERS) {
                            killer -= MAX_PLAYERS;
                        }
                        if (olvl >= O_VERBOSE)
                            printf("Player %d crashed into Player %d!\n",
                                   player, killer);
                        killPlayer(player, killer);
                    }
                    if (olvl >= O_DEBUG)
                        fprintf(stderr, "Player %d crashed at: (%d, %d)\n",
                                player, xpx, ypx);
                    return;
                }
            } else if (wallMode != WALLS_WRAP) {
                if (olvl >= O_VERBOSE)
                    printf("Player %d hit the wall!\n", player);
                if (olvl >= O_DEBUG)
                    fprintf(stderr, "Player %d walled at: (%d, %d)\n",
                            player, xpx, ypx);
                killPlayer(player, 0);
                return;
            }
        }
    }

    SDL_UnlockSurface(gameScreen);
}

/**
 * Update the hitmap.
 * Actually puts the pieces from the addToHitMap()-queue into the map.
 * Also handles the creation of holes.
 *
 * @param delta Time since last update.
 * @see addToHitMap
 */
void updateHitMap(Uint32 delta)
{
    if (olvl >= O_DEBUG)
        fprintf(stderr, "Updating hitmap...\n");

    struct recentMapPiece *cur;
    struct recentMapPiece *prev = recents;

    if (prev == NULL || prev->next == NULL)
        return;

    cur = prev->next;
    prev->count -= delta;

    SDL_LockSurface(gameScreen);

    while (cur != NULL) {
        cur->count -= delta;
        unsigned char *at =
            &hitmap[sizeof(bool) * ((WINDOW_W * cur->y) + cur->x)];
        if (holes && cur->count <= HOLE_DELAY && *at > MAX_PLAYERS * 2) {
            *at = 0;
            putPixel(cur->x, cur->y, cTransparent, gameScreen);
            prev->next = cur->next;
            free(cur);
            cur = prev->next;
        } else if (cur->count <= 0) {
            *at -= MAX_PLAYERS;
            prev->next = cur->next;
            free(cur);
            cur = prev->next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    SDL_UnlockSurface(gameScreen);
}

/**
 * Empties the hitmap.
 */
void cleanHitMap(void)
{
    if (olvl >= O_DEBUG)
        fprintf(stderr, "Cleaning hitmap...\n");

    struct recentMapPiece *cur = recents;
    struct recentMapPiece *tmp;

    if (cur == NULL)
        return;

    while (cur->next != NULL) {
        tmp = cur->next;
        free(cur);
        cur = tmp;
    }
    free(cur);

    recents = NULL;
}

/**
 * Delete recent player pieces within a given rectangle region.
 */
void clearRecents(int fromX, int toX, int fromY, int toY)
{
    while (recents && recents->x >= fromX && recents->x <= toX &&
           recents->y >= fromY && recents->y <= toY) {
        recents = recents->next;
    }

    if (recents == NULL) {
        return;
    }

    struct recentMapPiece *prev = recents;
    struct recentMapPiece *cur = prev->next;

    while (cur) {
        if (cur->x >= fromX && cur->x <= toX &&
                cur->y >= fromY && cur->y <= toY) {
            prev->next = cur->next;
            free(cur);
            cur = prev->next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

/* How long the round has gone on, for the walls that close in. */
static int closeTime = 0;

/* How far the walls have closed in: at the sides, and at top and bottom. */
static void closedIn(int *mx, int *my)
{
    *mx = *my = 0;
    if (wallMode != WALLS_CLOSE || closeTime < CLOSE_AFTER_MS)
        return;
    int m = (int)((closeTime - CLOSE_AFTER_MS) / 1000.0 * CLOSE_SPEED);
    if (m > (int)WINDOW_W / 2 - 24)
        m = (int)WINDOW_W / 2 - 24;
    *mx = m;
    *my = m * WINDOW_H / WINDOW_W;
}

#ifdef __3DS__
/* Paints what is out of bounds, under the trails. */
static void drawClosedIn(SDL_Surface *s)
{
    int mx, my;
    closedIn(&mx, &my);
    if (mx == 0)
        return;
    Uint32 dark = SDL_MapRGB(s->format, 0x4A, 0x0E, 0x1C);
    Uint32 edge = SDL_MapRGB(s->format, 0xFF, 0x4A, 0x5A);
    int w = s->w, h = s->h;
    SDL_Rect zone[4] = {{0, 0, w, my}, {0, h - my, w, my},
                        {0, my, mx, h - 2 * my}, {w - mx, my, mx, h - 2 * my}};
    SDL_Rect line[4] = {{mx, my, w - 2 * mx, 1}, {mx, h - my - 1, w - 2 * mx, 1},
                        {mx, my, 1, h - 2 * my}, {w - mx - 1, my, 1, h - 2 * my}};
    for (int k = 0; k < 4; ++k) {
        SDL_FillRect(s, &zone[k], dark);
        SDL_FillRect(s, &line[k], edge);
    }
}
#endif

/**
 * Logic of the main gameplay.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicGame(void)
{
#ifdef __3DS__
    if (netGameRole() != NET_ROLE_NONE) {
        int t = netGameTick();
        if (t != NET_TICK_CONTINUE)
            return t == NET_TICK_REDRAW;
    }
#endif
    Uint32 timenow = SDL_GetTicks();
    Uint32 delta = timenow - prevtime;
    prevtime = timenow;

    if (olvl >= O_DEBUG) {
        fprintf(stderr, "delta: %d    prevtime: %d\n", delta, prevtime);
    }

    if (delta > 35) {
        delta = 35;
    }
    else if (delta == 0) {
        return 0;
    }

#ifdef __3DS__
    if (matchWinner >= 0) {
        beginMatchEnd();
        return 1;
    }
    closeTime += delta;

    /* The shake dies down: the offset gets smaller as the time runs out. */
    if (shakeLeft > 0) {
        shakeLeft -= delta;
        int amplitude = shakeLeft > 0 ? 1 + 3 * shakeLeft / SHAKE_TIME : 0;
        shakeX = amplitude > 0 ? rand() % (2 * amplitude + 1) - amplitude : 0;
        shakeY = amplitude > 0 ? rand() % (2 * amplitude + 1) - amplitude : 0;
    }
#endif

    int closeX, closeY;
    closedIn(&closeX, &closeY);
    const struct aiField field = {wallMode == WALLS_WRAP, closeX, closeY};

#ifdef __3DS__
    if (weapons && itemMode
        && itemsUpdate(delta, hitmap, WINDOW_W, WINDOW_H, closeX, closeY)) {
        playSound(SOUND_BEEEEP, sound);
        refreshGameScreen();
    }
#endif

    /* Movement and turning follow the game speed rule, timers don't. */
    double mdelta = delta * gameSpeed / 100.0;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (winnerDeclared)
            return 0;

        if (players[i].active) {

            struct player *p = &players[i];

            if (weapons) {
                if (p->wep_time > 0) {
                    p->wep_time -= delta;
                    if (p->wep_time <= 0) {
                        wep_list[p->weapon].func(p, 0);
                        p->wep_time = WEP_NONACTIVE;
                    }
                    else if (particleEffects) {
                        wep_list[p->weapon].particleFunc(p, delta);
                    }
                }

                if (p->inv_self > 0) {
                    p->inv_self -= delta;
                    if (p->inv_self < 0) {
                        p->inv_self = 0;
                    }
                    else if (particleEffects) {
                        wep_list[p->weapon].particleFunc(p, delta);
                    }
                }

                if (p->inv_others > 0) {
                    p->inv_others -= delta;
                    if (p->inv_others < 0)
                        p->inv_others = 0;
                }
            }

            if (alivecount <= 1) {
#ifdef __3DS__
                beginRoundEnd();
                return 1;
#else
                clearSurface(gameScreen);
                newRound();
                return 0;
#endif
            } else if (p->alive) {

                unsigned int curx, cury;

                if (weapons && p->wep_time == WEP_NONACTIVE
                        && p->wep_count > 0) {
                    if (p->ai) {
                        if (rand() / (double)RAND_MAX < aiWeaponProb(p->aiLevel) * aiWeaponRate()) {
                            p->wep_time = wep_list[p->weapon].func(p, 1);
                            p->wep_count -= 1;
                            refreshGameScreen();
                        }
                    } else if (buttonDown(p->wkey)) {
                        p->wep_time = wep_list[p->weapon].func(p, 1);
                        p->wep_count -= 1;
                        clearButton(p->wkey);
                        refreshGameScreen();
                    }
                }

                if (p->ai) {

                    /* quick and dirty ai-debugging. This should never
                     * be turned on */
                    /*
                      refreshGameScreen();
                      int checkx = p->posx + 40*cos(p->dir + M_PI_2);
                      int checky = p->posy + 40*sin(p->dir + M_PI_2);
                      SDL_Rect a = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &a, SDL_MapRGB(screen->format,
                      0x55, 0x77, 0x99));
                      checkx = p->posx + 40*cos(p->dir - M_PI_2);
                      checky = p->posy + 40*sin(p->dir - M_PI_2);
                      SDL_Rect b = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &b, SDL_MapRGB(screen->format,
                      0x55, 0x77, 0x99));
                      checkx = p->posx + 85 * cos(p->dir);
                      checky = p->posy + 85 * sin(p->dir);
                      SDL_Rect d = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &d, SDL_MapRGB(screen->format,
                      0x55, 0x77, 0x99));
                      checkx = p->posx + 80 * cos(p->dir);
                      checky = p->posy + 80 * sin(p->dir);
                      SDL_Rect e = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &e, SDL_MapRGB(screen->format,
                      0xFF, 0x30, 0x30));
                      checkx = p->posx + 60*cos(p->dir + M_PI_4);
                      checky = p->posy + 60*sin(p->dir + M_PI_4);
                      SDL_Rect f = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &f, SDL_MapRGB(screen->format,
                      0x66, 0x66, 0x66));
                      checkx = p->posx + 60*cos(p->dir - M_PI_4);
                      checky = p->posy + 60*sin(p->dir - M_PI_4);
                      SDL_Rect g = {checkx, checky, 4, 4};
                      SDL_FillRect(screen, &g, SDL_MapRGB(screen->format,
                      0x66, 0x66, 0x66));
                    */

                    char c = pollAi(p->posx, p->posy, p->dir, p->active,
                                    hitmap, WINDOW_W, WINDOW_H, p->aiLevel, &field);
#ifdef __3DS__
                    /* With nothing in the way, an empty hand goes for an item. */
                    int tx, ty;
                    if (c == '0' && weapons && itemMode && p->wep_count == 0
                        && p->wep_time == WEP_NONACTIVE
                        && itemsNearest(p->posx, p->posy,
                                        aiSeekRange(p->aiLevel), &tx, &ty))
                        c = aiSteerTo(p->posx, p->posy, p->dir, tx, ty);
#endif
                    if (activeTron) {
                        if (c == 'l')
                            p->dir -= M_PI_2;
                        else if (c == 'r')
                            p->dir += M_PI_2;
                    } else {
                        if (c == 'l')
                            p->dir -= 0.0022 * mdelta * p->speed;
                        else if (c == 'r')
                            p->dir += 0.0022 * mdelta * p->speed;
                    }
                } else {
                    if (activeTron) {
                        /* Screw smooth angles, TRON MODE from here on! */
                        if (keyDown[p->lkey]) {
                            keyDown[p->lkey] = 0;
                            p->dir -= M_PI_2;
                        } else if (keyDown[p->rkey]) {
                            keyDown[p->rkey] = 0;
                            p->dir += M_PI_2;
                        }
                    } else {
                        if (buttonDown(p->lkey)) {
                            p->dir -= 0.0022 * mdelta * p->speed;
                        }
                        else if (buttonDown(p->rkey)) {
                            p->dir += 0.0022 * mdelta * p->speed;
                        }
                    }
                }

                p->posx += ZATA_SPEED * cos(p->dir) * mdelta * p->speed;
                p->posy += ZATA_SPEED * sin(p->dir) * mdelta * p->speed;

                if (wallMode == WALLS_WRAP) {
                    /* Without walls one comes out on the other side. */
                    if (p->posx < 0)
                        p->posx += WINDOW_W;
                    else if (p->posx >= WINDOW_W)
                        p->posx -= WINDOW_W;
                    if (p->posy < 0)
                        p->posy += WINDOW_H;
                    else if (p->posy >= WINDOW_H)
                        p->posy -= WINDOW_H;
                } else if (wallMode == WALLS_BOUNCE) {
                    /* The walls throw one back, like light off a mirror. */
                    const double edge = BOUNCE_EDGE;
                    if (p->posx < edge) {
                        p->posx = edge;
                        p->dir = M_PI - p->dir;
                    } else if (p->posx > WINDOW_W - 1 - edge) {
                        p->posx = WINDOW_W - 1 - edge;
                        p->dir = M_PI - p->dir;
                    }
                    if (p->posy < edge) {
                        p->posy = edge;
                        p->dir = -p->dir;
                    } else if (p->posy > WINDOW_H - 1 - edge) {
                        p->posy = WINDOW_H - 1 - edge;
                        p->dir = -p->dir;
                    }
                }

                curx = (unsigned int)p->posx;
                cury = (unsigned int)p->posy;

                /* Out of the part of the field that is left: dead. */
                if (closeX > 0 && ((int)curx < closeX
                                   || (int)curx >= (int)WINDOW_W - closeX
                                   || (int)cury < closeY
                                   || (int)cury >= (int)WINDOW_H - closeY)) {
                    killPlayer(p->active, 0);
                    continue;
                }

                if (holes) {
                    p->holecount += delta;
                    if (p->holecount > HOLE_FREQ)
                        p->holecount = 0;
                }

                if (p->prevx != curx || p->prevy != cury) {
                    if (olvl >= O_DEBUG)
                        fprintf(stderr, "Adding to hitmap: %d, %d...\n",
                                p->prevx, p->prevy);
                    if (holes && p->holecount > HOLE_SIZE)
                        addToHitMap(p->prevx, p->prevy, p->active,
                                    MAX_PLAYERS);
                    else
                        addToHitMap(p->prevx, p->prevy, p->active, 0);
                    p->prevx = curx;
                    p->prevy = cury;
                }
            }
        } else {
            break;
        }
    }

    updateHitMap(delta);

    if (weapons) {
        updateShots(delta, hitmap);
    }

    if (particleEffects) {
        updateParticles(delta);
    }

    SDL_UnlockSurface(gameScreen);
    return 1;
}

#ifdef __3DS__
/*
 * The front of a line is always there, also when a hole is coming: the trail
 * of a hole is wiped a moment after it was drawn, so the head would be bare.
 * This paints the head's own place over the field, for the picture only.
 */
static void drawLineFronts(SDL_Surface *s, SDL_Rect at)
{
    const int reach = ZATA_SIZE / 2;

    for (int k = 0; k < nPlayers; ++k) {
        const struct player *p = &players[k];
        if (!p->alive || !holes || p->holecount <= HOLE_SIZE)
            continue;
        Uint32 c = uiMap(s, colors[p->color]);
        for (int j = -reach; j <= reach; ++j) {
            for (int i = -reach; i <= reach; ++i) {
                if (abs(i) + abs(j) > reach + 1)
                    continue;
                SDL_Rect px = {(int)p->posx + i + at.x, (int)p->posy + j + at.y,
                               1, 1};
                SDL_FillRect(s, &px, c);
            }
        }
    }
}
#endif

/**
 * Display a game round.
 */
void displayGame(void)
{
    useTopView();
    clearPlayfield(screen);
#ifdef __3DS__
    drawClosedIn(screen);
#endif

#ifdef __3DS__
    SDL_Rect shaken = {shakeX, shakeY, 0, 0};
    SDL_Rect *at = &shaken;
#else
    SDL_Rect *at = NULL;
#endif

    if (particleEffects && hasParticles()) {
        renderParticles(particleScreen);
        SDL_BlitSurface(particleScreen, NULL, screen, at);
    }

    SDL_BlitSurface(gameScreen, NULL, screen, at);
#ifdef __3DS__
    if (weapons && itemMode)
        itemsDraw(screen, shaken, smallWepIcons);
    drawLineFronts(screen, shaken);
    shopDrawHeads(screen, shaken);
#endif

    if (presentFrame() == -1) {
        exit(1);
    }
}

/**
 * Logic of the startup phase of the game.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicGameStart(void)
{
#ifdef __3DS__
    if (netGameRole() != NET_ROLE_NONE) {
        int t = netGameTick();
        if (t != NET_TICK_CONTINUE)
            return t == NET_TICK_REDRAW;
    }
#endif
    Uint32 timenow = SDL_GetTicks();
    Uint32 delta = timenow - prevtime;
    prevtime = timenow;

    if (olvl >= O_DEBUG)
        fprintf(stderr, "delta: %d    prevtime: %d\n", delta, prevtime);

#ifdef __3DS__
    /* 13 ms suits 100 frames per second; at 60 the countdown would run slow. */
    if (delta > 25)
        delta = 25;
#else
    if (delta > 13)
        delta = 13;
#endif

    if (countdown > 0)
        countdown -= delta;
    else
        curScene = &game;

    return 1;
}

#ifdef __3DS__
static void drawFieldExtras(void);

/* Whether the score page still has to be drawn for this round start. */
static bool startPageStale = true;
#endif

/**
 * Display blinking arrows at the beginning of a game.
 */
void displayGameStart(void)
{
    useTopView();
    clearPlayfield(screen);
    clearSurface(gameScreen);

#ifdef __3DS__
    /* The score page on the bottom screen is slow to draw: once is enough. */
    if (startPageStale) {
        drawExtras();
        startPageStale = false;
    } else {
        drawFieldExtras();
    }
#else
    drawExtras();
#endif
    SDL_BlitSurface(gameScreen, NULL, screen, NULL);

    if ((countdown % (START_ROUND_WAIT / 4)) > (START_ROUND_WAIT / 8)) {
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (players[i].active) {
                struct player *p = &players[i];
                SDL_Rect offset = {(int)p->posx - 8, (int)p->posy - 8,
                                   0, 0};
                int diri = (int)((p->dir) * (32.0 / (2.0 * M_PI)));
                SDL_BlitSurface(p->arrow, &arrowClip[diri], screen,
                                &offset);
            } else {
                break;
            }
        }
    }

#ifdef __3DS__
    /* The seconds as big digits, with a tick for each */
    static int lastDigit = 0;
    int digit = (countdown + 999) / 1000;
    if (digit >= 1 && digit <= 3) {
        SDL_Color shadow = {0x12, 0x16, 0x2E, 0};
        uiDigit(screen, digit, screen->w / 2 + 4, screen->h / 2 + 4, 8, shadow);
        uiDigit(screen, digit, screen->w / 2, screen->h / 2, 8, uiColAccent);
    }
    if (digit != lastDigit) {
        lastDigit = digit;
        if (digit >= 1 && digit <= 3)
            playSound(SOUND_BEEP, sound);
    }
#endif

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Redraws the whole game screen.
 * Should only be called as a last resort when the whole screen needs to
 * be redrawn.
 */
void refreshGameScreen(void)
{
    SDL_UnlockSurface(gameScreen);
    clearSurface(gameScreen);

    drawExtras();

    SDL_LockSurface(gameScreen);

    Uint32 *target = gameScreen->pixels;

    /* The pixel value of each player's color in the format of gameScreen. */
    Uint32 pcolors[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        SDL_Color *c = &colors[players[i].color];
        pcolors[i] = SDL_MapRGB(gameScreen->format, c->r, c->g, c->b);
    }

    for (unsigned int yy = 0; yy < WINDOW_H; ++yy) {
        for (unsigned int xx = 0; xx < WINDOW_W; ++xx, ++target) {
            char charat = hitmap[sizeof(bool)
                                 * ((WINDOW_W * yy) + xx)];
            if (charat != 0) {
                int pi;
                if (charat > MAX_PLAYERS) {
                    if (charat > MAX_PLAYERS * 2) {
                        pi = charat - (MAX_PLAYERS * 2) - 1;
                    } else {
                        pi = charat - MAX_PLAYERS - 1;
                    }
                } else {
                    pi = charat - 1;
                }
                *target = pcolors[pi];
            }
        }
    }

    SDL_UnlockSurface(gameScreen);
}

#ifdef __3DS__
struct menu menuPause = {
    4,
    0,
};

/* The scene to go back to (game or gameStart), and whether "main menu" has
 * been selected once and now waits for a confirmation. */
static struct scene *pausedScene = NULL;
static bool pauseConfirm = false;

/**
 * Pauses the game: darkens the playfield on the top screen and shows the
 * pause menu on the bottom screen.
 */
void pauseGame(void)
{
    pausedScene = curScene;
    curScene = &pauseMenu;
    menuPause.choice = 0;
    pauseConfirm = false;

    /* Darken the playfield, once. It is not redrawn while the game is paused. */
    useTopView();
    SDL_Surface *dim = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
                                            screen->format->BitsPerPixel,
                                            screen->format->Rmask,
                                            screen->format->Gmask,
                                            screen->format->Bmask, 0);
    SDL_FillRect(dim, NULL, SDL_MapRGB(dim->format, 0, 0, 0));
    SDL_SetAlpha(dim, SDL_SRCALPHA, 150);
    SDL_BlitSurface(dim, NULL, screen, NULL);
    SDL_FreeSurface(dim);

    SDL_Rect sign = {screen->w / 2 - 90, screen->h / 2 - 24, 180, 48};
    sign = uiCard(screen, sign, uiColPanel, uiColAccent, false);
    uiText(screen, FONT_MENU, "PAUSED", uiColAccent, sign.x + sign.w / 2,
           sign.y + 16, UI_CENTER);
    useBottomView();

    displayPause();
}

#define RESUME_STEP 700     /* How long each number of the count down shows (ms) */

static int resumeLeft = 0;
static Uint32 resumePrev = 0;

/**
 * Goes back to the game after a count down of 3, 2, 1, so one can find one's
 * line again. The scores on the bottom screen are drawn again.
 */
void resumeGame(void)
{
    refreshGameScreen();
    useTopView();

    if (pausedScene == &game) {
        curScene = &resumeScene;
        resumeLeft = 3 * RESUME_STEP;
        resumePrev = SDL_GetTicks();
        displayResume();
    } else {
        /* The count down before a round has its own digits. */
        curScene = pausedScene;
    }
}

int logicResume(void)
{
    static int lastDigit = 0;
    Uint32 now = SDL_GetTicks();
    Uint32 delta = now - resumePrev;
    resumePrev = now;
    resumeLeft -= delta > 100 ? 100 : delta;

    if (resumeLeft <= 0) {
        curScene = &game;
        lastDigit = 0;
        return 1;
    }

    int digit = (resumeLeft + RESUME_STEP - 1) / RESUME_STEP;
    if (digit != lastDigit) {
        lastDigit = digit;
        playSound(SOUND_BEEP, sound);
    }
    return 1;
}

void displayResume(void)
{
    /* The field as it was, with the number over it. */
    useTopView();
    clearPlayfield(screen);
    drawClosedIn(screen);
    if (particleEffects && hasParticles())
        SDL_BlitSurface(particleScreen, NULL, screen, NULL);
    SDL_BlitSurface(gameScreen, NULL, screen, NULL);
#ifdef __3DS__
    if (weapons && itemMode)
        itemsDraw(screen, (SDL_Rect){0, 0, 0, 0}, smallWepIcons);
    drawLineFronts(screen, (SDL_Rect){0, 0, 0, 0});
    shopDrawHeads(screen, (SDL_Rect){0, 0, 0, 0});
#endif

    int digit = (resumeLeft + RESUME_STEP - 1) / RESUME_STEP;
    SDL_Color shadow = {0x12, 0x16, 0x2E, 0};
    uiDigit(screen, digit, screen->w / 2 + 4, screen->h / 2 + 4, 8, shadow);
    uiDigit(screen, digit, screen->w / 2, screen->h / 2, 8, uiColAccent);

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Logic of the pause menu.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
static void saveLineupSettings(void);

int logicPause(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        switch (menuPause.choice) {
        case 0:
            playSound(SOUND_BEEP, sound);
            resumeGame();
            return 1;
        case 1: /* Restart the round, the scores stay */
            playSound(SOUND_BEEP, sound);
            endRound();
            clearSurface(gameScreen);
            newRound();
            useTopView();
            return 1;
        case 2: /* Sound and music together */
            playSound(SOUND_BEEP, sound);
            sound = !sound;
            music = sound;
            if (music)
                playBGM();
            else
                stopBGM();
            saveLineupSettings();   /* A tournament has other rules on loan */
            break;
        case 3:
            if (!pauseConfirm) {
                playSound(SOUND_BEEP, sound);
                pauseConfirm = true;
            } else {
                playSound(SOUND_BEP, sound);
                endRound();
                curScene = &mainMenu;
                curScene->displayFunc();
                return 0;
            }
            break;
        }
        return 1;
    }

    /* The tiles: RESUME on top, RESTART and SOUND beside each other, MENU below. */
    int oldChoice = menuPause.choice;
    if (menuButtonQuery(KEY_DOWN)) {
        menuPause.choice = menuPause.choice == 0 ? 1 : 3;
    } else if (menuButtonQuery(KEY_UP)) {
        menuPause.choice = menuPause.choice == 3 ? 1 : 0;
    } else if (menuButtonQuery(KEY_RIGHT)) {
        if (menuPause.choice == 1)
            menuPause.choice = 2;
    } else if (menuButtonQuery(KEY_LEFT)) {
        if (menuPause.choice == 2)
            menuPause.choice = 1;
    }

    if (menuPause.choice != oldChoice) {
        playSound(SOUND_BEEP, sound);
        pauseConfirm = false;
        return 1;
    }
    return 0;
}

/**
 * Display the pause menu on the bottom screen.
 */
void displayPause(void)
{
    useBottomView();
    clearBackground(screen);

    SDL_Rect resume = {16, 10, 288, 62};
    SDL_Rect restart = {16, 82, 140, 72};
    SDL_Rect soundTile = {164, 82, 140, 72};
    SDL_Rect menu = {16, 166, 288, 44};

    touchReset();
    touchAdd(resume, 0, TOUCH_ROW);
    touchAdd(restart, 1, TOUCH_ROW);
    touchAdd(soundTile, 2, TOUCH_ROW);
    touchAdd(menu, 3, TOUCH_ROW);

    uiTile(screen, resume, "RESUME", NULL, uiIconPlay, uiColGreen,
           menuPause.choice == 0, true);
    uiTile(screen, restart, "RESTART", NULL, uiIconRestart, uiColBlue,
           menuPause.choice == 1, true);
    uiTile(screen, soundTile, "SOUND", sound ? "ON" : "OFF", uiIconSound,
           uiColBlue, menuPause.choice == 2, true);
    uiTile(screen, menu, pauseConfirm ? "SURE?" : "MENU",
           pauseConfirm ? "PRESS A AGAIN" : NULL, uiIconHome, uiColRed,
           menuPause.choice == 3, true);

    struct uiHint hints[] = {{"A", "OK"}, {"B", "RESUME"}};
    uiHintBar(screen, hints, 2);

    if (presentFrame() == -1)
        exit(1);
}
#endif

/**
 * Draws what belongs on the playfield: the border and the icon of an active
 * weapon.
 */
static void drawFieldExtras(void)
{
    Uint32 frame = SDL_MapRGB(gameScreen->format, 0xFF, 0xFF, 0xFF);
    bool framed = border;
#ifdef __3DS__
    /* A frame bought in the shop is drawn even if the border is off. */
    SDL_Color chosen;
    if (shopFrameColor(&chosen)) {
        frame = SDL_MapRGB(gameScreen->format, chosen.r, chosen.g, chosen.b);
        framed = true;
    }
#endif
    if (framed && wallMode != WALLS_WRAP) {
        SDL_Rect leftBorder = {0, 0, 1, WINDOW_H};
        SDL_Rect rightBorder = {WINDOW_W - 1, 0, WINDOW_W, WINDOW_H};
        SDL_Rect topBorder = {0, 0, WINDOW_W, 1};
        SDL_Rect bottomBorder = {0, WINDOW_H - 1, WINDOW_W, WINDOW_H};
        SDL_FillRect(gameScreen, &leftBorder, frame);
        SDL_FillRect(gameScreen, &rightBorder, frame);
        SDL_FillRect(gameScreen, &topBorder, frame);
        SDL_FillRect(gameScreen, &bottomBorder, frame);
    }

    /* Active weapons */
    if (activeFreeze || activeConfusion || activeTron || activeChilirun) {
        SDL_Rect offset = {WINDOW_W - wepIcons[0]->w, 0, 0, 0};
        if (activeFreeze)
            SDL_BlitSurface(wepIcons[WEP_FROSTWAVE + 1], NULL, gameScreen, &offset);
        if (activeConfusion)
            SDL_BlitSurface(wepIcons[WEP_CONFUSION + 1], NULL, gameScreen, &offset);
        if (activeTron)
            SDL_BlitSurface(wepIcons[WEP_TRON + 1], NULL, gameScreen, &offset);
        if (activeChilirun)
            SDL_BlitSurface(wepIcons[WEP_CHILI_RUN + 1], NULL, gameScreen, &offset);
    }
}

/**
 * Draws the game scores onto the screen.
 */
void drawExtras(void)
{
#ifdef __3DS__
    /* The bottom screen shows the same score page as after a round: the scores
     * and the weapons, and the latest messages. */
    SDL_Surface *drawingOn = screen;    /* The score page switches to the bottom */
    drawScoreBars(-1, true);
    drawMessages();
    screen = drawingOn;
#else
    char score_str[SCORE_BUF];
    SDL_Surface *scoreText;
    SDL_Surface *hud = gameScreen;

    for (int i = 0; i < nPlayers; ++i) {
        struct player *p = &players[i];

        SDL_Rect offset = {7, SCORE_SPACING * i, 0, 0};

        snprintf(score_str, SCORE_BUF, "%d", p->score);
        scoreText = TTF_RenderUTF8_Shaded(fonts[FONT_SCORE], score_str,
                                          colors[p->color], cMenuBG);
        SDL_BlitSurface(scoreText, NULL, hud, &offset);
        SDL_FreeSurface(scoreText);

        if (weapons && p->wep_count > 0) {
            offset.y += 2;
            offset.x += WEP_SMALL_INIT_OFFSET;

            for (int j = 0; j < p->wep_count; ++j) {
                offset.x += WEP_SMALL_PADDING;
                SDL_BlitSurface(smallWepIcons[p->weapon], NULL, hud,
                                &offset);
            }
        }
    }

    if (broadcasts) {
        for (int i = 0; i < BROADC_LIMIT; ++i) {
            if (broadcast[i] != NULL) {
                SDL_Rect offset = {hud->w - broadcast[i]->w - 4,
                                   hud->h - (broadcast[i]->h * (i + 1)),
                                   0, 0};
                SDL_BlitSurface(broadcast[i], NULL, hud, &offset);
            }
        }
    }
#endif

    drawFieldExtras();
}

/**
 * Starts a new round.
 */
void newRound(void)
{
    cleanHitMap();

    if (particleEffects) {
        resetParticles();
    }

#ifdef __3DS__
    matchWinner = -1;
    closeTime = 0;
    startPageStale = true;
    shakeLeft = shakeX = shakeY = 0;
#endif
    alivecount = nPlayers;
    countdown = START_ROUND_WAIT;
    memset(hitmap, '\0', sizeof(bool) * WINDOW_W * WINDOW_H);

    curScene = &gameStart;

    if (olvl >= O_VERBOSE)
        printf(" -- New round! --  ( Score: ");

    if (weapons)
        resetWeapons();
#ifdef __3DS__
    itemsReset();
#endif
    winnerDeclared = false;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        struct player *p = &players[i];
        if (p->active) {
            if (olvl >= O_VERBOSE)
                printf("%d ", p->score);
            if (duelmode)
                drespawn(p);
            else
                respawn(p);
        } else {
            if (olvl >= O_VERBOSE)
                printf(")\n");
            return;
        }

        p->inv_self = 0;
        p->inv_others = 0;
    }

    if (olvl >= O_VERBOSE)
        printf(")\n");
}

/**
 * Ends the current round.
 */
void endRound(void)
{
    cleanHitMap();
    if (broadcasts)
        cleanBroadcasts();
    if (weapons)
        resetWeapons();
}

/**
 * Lets the AI pick weapons at random.
 */
void assignAiWeapons(void)
{
    for (int i = 0; i < nPlayers; ++i) {
        struct player *p = &players[i];
        if (p->ai) {
            double rnd = (double)rand() / RAND_MAX;
            p->weapon = (int)(rnd * N_WEAPONS);
        }
    }
}

/**
 * Negates all active weapon effects.
 */
void resetWeapons(void)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        struct player *p = &players[i];
        if (p->active) {

#ifdef __3DS__
            /* With items one starts with empty hands. */
            p->wep_count = itemMode ? 0 : wep_list[p->weapon].charges;
#else
            p->wep_count = wep_list[p->weapon].charges;
#endif

            if (p->wep_time != WEP_NONACTIVE) {
                wep_list[p->weapon].func(p, 0);
                p->wep_time = WEP_NONACTIVE;
            }
        }
    }
}

/**
 * Initializes the main menu.
 */
void initMainMenu(void)
{
    colorBalls();
}

#ifdef __3DS__
/**
 * Draws the top screen of the title and the first menus: the animation, and the
 * logo over it.
 */
void drawTitleScreen(void)
{
    SDL_Surface *logo = images[IMG_LOGO];

    useTopView();
    clearBackground(screen);
    demoDraw(screen);
    SDL_Rect offset = {(screen->w - logo->w) / 2, (screen->h - logo->h) / 2,
                       0, 0};
    SDL_BlitSurface(logo, NULL, screen, &offset);
    useBottomView();
}
#endif

#ifdef __3DS__
/* ---- The title screen ---- */

/* Whether the "PRESS A" on the bottom screen is shown right now. */
static bool titleBlinkOn = true;

static bool titleBlink(void)
{
    return (SDL_GetTicks() / 500) % 2 == 0;
}

/* The bottom screen of the title: a blinking "PRESS A", the name of the game,
 * the version and the author. */
static void drawTitleBottom(bool blink)
{
    useBottomView();
    clearBackground(screen);
    touchReset();

    /* A touch anywhere starts. */
    SDL_Rect everywhere = {0, 0, screen->w, screen->h};
    touchAdd(everywhere, 0, TOUCH_ROW);

    if (blink)
        uiText(screen, FONT_MENU, "PRESS A", uiColAccent, screen->w / 2, 92,
               UI_CENTER);
    uiText(screen, FONT_BROADCAST, "A 3DS PORT OF ZATACKA X", uiColDim,
           screen->w / 2, 160, UI_CENTER);
    char profile[48];
    snprintf(profile, sizeof(profile), "%s  -  %u ROUNDS WON", statsTitle(),
             stats.roundWins);
    uiText(screen, FONT_BROADCAST, profile, uiColAccent, screen->w / 2, 194,
           UI_CENTER);

    /* The version at the left, the author at the right. */
    uiText(screen, FONT_BROADCAST, "v" PORT_VERSION, uiColDim, 8,
           screen->h - 14, UI_LEFT);
    uiText(screen, FONT_BROADCAST, "made by toomium", uiColDim, screen->w - 8,
           screen->h - 14, UI_RIGHT);
}

int logicTitle(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        playSound(SOUND_BEEP, sound);
        curScene = &mainMenu;
        return 1;
    }
    return 0;
}

void displayTitle(void)
{
    titleBlinkOn = titleBlink();
    drawTitleBottom(titleBlinkOn);
    drawTitleScreen();

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Every frame while the title screen, the main menu or the mode page is shown:
 * the balls move on, and the top screen is drawn again. On the title the
 * bottom screen is drawn when "PRESS A" starts or stops blinking.
 */
void animTitle(void)
{
    demoUpdate();
    drawTitleScreen();

    if (curScene == &titleScene && titleBlink() != titleBlinkOn) {
        titleBlinkOn = !titleBlinkOn;
        drawTitleBottom(titleBlinkOn);
    }

    if (presentFrame() == -1)
        exit(1);
}

struct menu menuMode = {
    3,
    0,
};

/* The lineup leads to a tournament instead of a game (the third tile). */
static bool tournamentSelected = false;

/* ---- The lineup: eight slots, each OFF, AI or P1 ---- */

/* The setup of the slots while a game runs, and whether players[] is packed. */
static struct player slotBackup[MAX_PLAYERS];
static bool slotsPacked = false;

/* The rules of the local game, kept while a match with several consoles uses
 * the host's. */
static struct {
    unsigned int scorecap;
    bool holes, duelmode, weapons;
    int walls;
    int gameSpeed;
} savedRules;
static bool netRulesSaved = false;

/*
 * Saves the settings with the rules of the lineup, not those that a match with
 * several consoles or a tournament has put in their place.
 */
static void saveLineupSettings(void)
{
    if (tournamentActive())
        tournamentSwapRules();
    unsigned cap = scorecap;
    bool h = holes, d = duelmode, w = weapons;
    int wr = wallMode;
    int speed = gameSpeed;
    if (netRulesSaved) {
        scorecap = savedRules.scorecap;
        holes = savedRules.holes;
        duelmode = savedRules.duelmode;
        weapons = savedRules.weapons;
        wallMode = savedRules.walls;
        gameSpeed = savedRules.gameSpeed;
    }
    saveSettings();
    if (netRulesSaved) {
        scorecap = cap;
        holes = h;
        duelmode = d;
        weapons = w;
        wallMode = wr;
        gameSpeed = speed;
    }
    if (tournamentActive())
        tournamentSwapRules();
}

/* The slot and the row that the cursor is on, on the lineup page, and the row on
 * the rules and options pages. */
int lobbySlot = 0;
static int lobbyRow = 0;
static int rulesRow = 0;
static int optionsRow = 0;

struct player *slotConfig(int i)
{
    return slotsPacked ? &slotBackup[i] : &players[i];
}

/**
 * Puts the indices of the active slots into `idx` and returns how many there are.
 */
int activeSlots(int idx[MAX_PLAYERS])
{
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (slotMode[i] != SLOT_OFF)
            idx[n++] = i;
    }
    return n;
}

/**
 * The game wants its players without gaps at the front of players[]: pack the
 * active slots there. A slot keeps its color. The setup is kept in slotBackup
 * and comes back with restoreSlots().
 */
static int packedSlot[MAX_PLAYERS];     /* Which slot a packed player was */

static void packSlots(int n, const int idx[MAX_PLAYERS])
{
    memcpy(slotBackup, players, sizeof(players));
    memcpy(packedSlot, idx, sizeof(packedSlot));
    for (int k = 0; k < MAX_PLAYERS; ++k) {
        if (k < n) {
            players[k] = slotBackup[idx[k]];
            players[k].ai = slotMode[idx[k]] == SLOT_AI;
            /* The computer wears nothing, a human only what was bought. */
            players[k].headStyle = players[k].ai
                ? 0 : shopOwnedHead(players[k].headStyle);
        } else {
            players[k] = slotBackup[k];
            players[k].active = 0;
        }
    }
    slotsPacked = true;
}

/**
 * Brings the setup of the slots back after a game. Called whenever a menu is
 * shown, so every way out of a game is covered.
 */
void restoreSlots(void)
{
    /* Any menu but the bracket means the tournament is over. */
    if (tournamentActive() && curScene != &tourScene)
        tournamentStop();
    if (slotsPacked) {
        memcpy(players, slotBackup, sizeof(players));
        slotsPacked = false;
    }
    if (netRulesSaved) {
        scorecap = savedRules.scorecap;
        holes = savedRules.holes;
        duelmode = savedRules.duelmode;
        weapons = savedRules.weapons;
        wallMode = savedRules.walls;
        gameSpeed = savedRules.gameSpeed;
        netRulesSaved = false;
    }
}

/* The player whose weapon is chosen on the weapon page: an index into players[]. */
static int wepCursor = 0;

/**
 * The next human player (not AI) from `from` in direction `dir` (1 or -1), or
 * -1 if there is none. `from` itself is not looked at.
 */
static int nextHuman(int from, int dir)
{
    for (int step = 1; step <= MAX_PLAYERS; ++step) {
        int i = (from + dir * step + 2 * MAX_PLAYERS) % MAX_PLAYERS;
        if (players[i].active && !players[i].ai)
            return i;
    }
    return -1;
}

/**
 * Starts a game on this console with the current setup of players and rules.
 */
static void beginGame(int n, const int idx[MAX_PLAYERS])
{
    packSlots(n, idx);
    nPlayers = n;

    initPlayers2();
    newRound();
    if (weapons && !itemMode) {
        assignAiWeapons();
        wepCursor = nextHuman(MAX_PLAYERS - 1, 1);
        if (wepCursor >= 0) {
            curScene = &wepMenu;
        } else {
            /* Only computer players: nothing to choose */
            resetWeapons();
            curScene = &gameStart;
        }
    } else {
        curScene = &gameStart;
    }
}

static void startLocalGame(void)
{
    int idx[MAX_PLAYERS];
    int n = activeSlots(idx);

    if (duelmode && n > 2)
        n = 2;
    beginGame(n, idx);
}

/* A duel of the tournament between two slots of the lineup. */
void startDuel(int a, int b)
{
    int idx[MAX_PLAYERS] = {a, b};

    restoreSlots();
    beginGame(2, idx);
}

/* ---- A match with several consoles ---- */

/**
 * Starts a match with `n` players. The host sends `seats` (see netgame3ds.c);
 * every console builds the same players from them. The local setup is kept and
 * comes back with netEndGame().
 */
void startNetGame(const struct netSeat *seats, int n, int role,
                  unsigned points, bool holesRule, int speed)
{
    if (!slotsPacked) {
        memcpy(slotBackup, players, sizeof(players));
        slotsPacked = true;
    }
    if (!netRulesSaved) {
        savedRules.scorecap = scorecap;
        savedRules.holes = holes;
        savedRules.duelmode = duelmode;
        savedRules.weapons = weapons;
        savedRules.walls = wallMode;
        savedRules.gameSpeed = gameSpeed;
        netRulesSaved = true;
    }
    scorecap = points;
    holes = holesRule;
    duelmode = false;
    gameSpeed = speed;
    weapons = false;
    wallMode = WALLS_SOLID;

    int remote = 0;
    for (int k = 0; k < MAX_PLAYERS; ++k) {
        if (k >= n) {
            players[k] = slotBackup[k];
            players[k].active = 0;
            continue;
        }

        /* The local setup (buttons) is the base of every player. */
        players[k] = slotBackup[0];
        struct player *p = &players[k];
        snprintf(p->name, PLAYER_NAME_LEN, "%s", seats[k].name);
        p->color = seats[k].color;
        p->headStyle = seats[k].head;
        p->ai = seats[k].ai;
        p->aiLevel = AI_NORMAL;
        p->speed = 1.0;
        p->invertedKeys = false;
        p->weapon = 0;
        if (!seats[k].local && !seats[k].ai && role == NET_ROLE_HOST) {
            /* A console far away: its buttons are set from the network. */
            p->lkey = SDLK_F1 + 2 * remote;
            p->rkey = SDLK_F1 + 2 * remote + 1;
            p->wkey = SDLK_F1 + 2 * remote + 1;
            ++remote;
        }
    }
    nPlayers = n;
    memset(netKiller, 0, sizeof(netKiller));

    initPlayers2();
    newRound();
}

int netSceneState(void)
{
    if (curScene == &gameStart)
        return GS_COUNTDOWN;
    if (curScene == &game)
        return GS_PLAY;
    if (curScene == &roundEndScene)
        return GS_ROUNDEND;
    return GS_MATCHEND;
}

int netGetFrame(struct netPlayerFrame *frames, int *cd, int *winner)
{
    for (int i = 0; i < nPlayers; ++i) {
        struct player *p = &players[i];
        frames[i].x = p->posx;
        frames[i].y = p->posy;
        frames[i].dir = p->dir;
        frames[i].alive = p->alive;
        frames[i].hole = holes && p->holecount > HOLE_SIZE;
        frames[i].score = p->score;
        frames[i].killer = netKiller[i];
    }
    *cd = countdown;
    *winner = matchWinner;
    return nPlayers;
}

/* A piece of trail, without the collisions: only the picture of the hitmap. */
static void netTrail(int player, int x, int y)
{
    struct player *p = &players[player];

    SDL_LockSurface(gameScreen);
    for (int i = -TOLERANCE; i <= TOLERANCE; ++i) {
        for (int j = -TOLERANCE; j <= TOLERANCE; ++j) {
            if (abs(i) + abs(j) > TOLERANCE + 1)
                continue;
            int xpx = x + i;
            int ypx = y + j;
            if (xpx < 0 || xpx >= (int)WINDOW_W || ypx < 0
                    || ypx >= (int)WINDOW_H)
                continue;
            putPixel(xpx, ypx, colors[p->color], gameScreen);
            hitmap[sizeof(bool) * ((WINDOW_W * ypx) + xpx)] = player + 1;
        }
    }
    SDL_UnlockSurface(gameScreen);
}

/* The sound, message and sparks of a crash that happened on the host. */
static void netCrashEffect(int victim, int killer)
{
    struct player *p = &players[victim];

    playSound(SOUND_CRASH, sound);
    if (broadcasts) {
        char msg[BROADC_BUF];
        if (killer == 0)
            snprintf(msg, BROADC_BUF, "%d; hit the wall", victim + 1);
        else if (killer == victim + 1)
            snprintf(msg, BROADC_BUF, "%d; committed suicide", victim + 1);
        else
            snprintf(msg, BROADC_BUF, "%d; crashed into ;%d", victim + 1,
                     killer);
        newBroadcast(msg);
    }
    if (shake)
        shakeLeft = SHAKE_TIME;
    if (particleEffects) {
        SDL_Color color = colors[p->color];
        shopCrashBurst(p->posx, p->posy, p->dir, color);
    }
}

static int netPrevGs = -1;

void netApplyFrame(int gstate, int cd, int winner,
                   const struct netPlayerFrame *f, int n)
{
    if (n > nPlayers)
        n = nPlayers;
    /*
     * The host is another console: keep what it sends inside the field. Its
     * players may be a little outside, a head that has hit the wall is, and
     * that is no reason to drop the whole picture. (A NaN gives 0.)
     */
    struct netPlayerFrame sane[MAX_PLAYERS];
    for (int i = 0; i < n; ++i) {
        sane[i] = f[i];
        sane[i].x = f[i].x >= 0 ? (f[i].x < WINDOW_W ? f[i].x : WINDOW_W - 1) : 0;
        sane[i].y = f[i].y >= 0 ? (f[i].y < WINDOW_H ? f[i].y : WINDOW_H - 1) : 0;
    }
    f = sane;
    if (winner < -1 || winner >= nPlayers)
        winner = -1;

    if (gstate == GS_COUNTDOWN && netPrevGs != GS_COUNTDOWN) {
        clearSurface(gameScreen);
        newRound();
    }

    bool refresh = false;
    for (int i = 0; i < n; ++i) {
        struct player *p = &players[i];
        int x = (int)f[i].x, y = (int)f[i].y;

        if (gstate != GS_COUNTDOWN && p->alive && f[i].alive && !f[i].hole) {
            /* The trail from the last place to this one. */
            int x0 = p->prevx, y0 = p->prevy;
            int steps = abs(x - x0) > abs(y - y0) ? abs(x - x0) : abs(y - y0);
            for (int s = 1; s <= steps; ++s)
                netTrail(i, x0 + (x - x0) * s / steps,
                         y0 + (y - y0) * s / steps);
        }
        if (p->alive && !f[i].alive) {
            p->posx = f[i].x;
            p->posy = f[i].y;
            p->dir = f[i].dir;
            netCrashEffect(i, f[i].killer);
            refresh = true;
        }
        if (p->score != f[i].score)
            refresh = true;

        p->posx = f[i].x;
        p->posy = f[i].y;
        p->dir = f[i].dir;
        p->score = f[i].score;
        p->alive = f[i].alive;
        /* The same test as on the host, see drawLineFronts(). */
        p->holecount = f[i].hole ? HOLE_SIZE + 1 : 0;
        p->prevx = x;
        p->prevy = y;
    }

    alivecount = 0;
    for (int i = 0; i < n; ++i)
        alivecount += players[i].alive;
    if (refresh)
        refreshGameScreen();

    switch (gstate) {
    case GS_COUNTDOWN:
        countdown = cd;
        curScene = &gameStart;
        break;
    case GS_PLAY:
        if (curScene != &game)
            curScene = &game;
        break;
    case GS_ROUNDEND:
        if (curScene != &roundEndScene)
            beginRoundEnd();
        break;
    case GS_MATCHEND:
        if (curScene != &matchEndScene) {
            matchWinner = winner;
            beginMatchEnd();
        }
        break;
    }
    netPrevGs = gstate;
}

void netEndGame(void)
{
    restoreSlots();
    endRound();
    netPrevGs = -1;
    matchWinner = -1;
}

/**
 * Logic of the main menu: three tiles, PLAY above OPTIONS and EXIT.
 */
int logicMainMenu(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        switch (menuMain.choice) {
        case 0:
            playSound(SOUND_BEEP, sound);
            menuMode.choice = 0;
            curScene = &modeMenu;
            break;
        case 1:
            playSound(SOUND_BEEP, sound);
            menuSettings.choice = 0;
            optionsRow = 0;
            curScene = &settingsMenu;
            break;
        case 2:
            exitGame(0);
            break;
        case 3:
            playSound(SOUND_BEEP, sound);
            shopEnter(&mainMenu);
            break;
        case 4:
            playSound(SOUND_BEEP, sound);
            profileEnter(&mainMenu);
            break;
        }
        return 1;
    }

    /* PLAY on top; then SHOP and PROFILE; then OPTIONS and EXIT. */
    static const signed char up[5] = {0, 3, 4, 0, 0};
    static const signed char down[5] = {3, 1, 2, 1, 2};
    static const signed char left[5] = {0, 1, 1, 3, 3};
    static const signed char right[5] = {0, 2, 2, 4, 4};
    int old = menuMain.choice;
    if (menuButtonQuery(KEY_DOWN))
        menuMain.choice = down[old];
    else if (menuButtonQuery(KEY_UP))
        menuMain.choice = up[old];
    else if (menuButtonQuery(KEY_RIGHT))
        menuMain.choice = right[old];
    else if (menuButtonQuery(KEY_LEFT))
        menuMain.choice = left[old];

    if (menuMain.choice != old) {
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

void displayMainMenu(void)
{
    restoreSlots();
    useBottomView();
    clearBackground(screen);

    SDL_Rect play = {16, 10, 288, 68};
    SDL_Rect shop = {16, 86, 140, 60};
    SDL_Rect profile = {164, 86, 140, 60};
    SDL_Rect options = {16, 154, 140, 60};
    SDL_Rect quit = {164, 154, 140, 60};

    touchReset();
    touchAdd(play, 0, TOUCH_ROW);
    touchAdd(shop, 3, TOUCH_ROW);
    touchAdd(profile, 4, TOUCH_ROW);
    touchAdd(options, 1, TOUCH_ROW);
    touchAdd(quit, 2, TOUCH_ROW);

    uiTile(screen, play, "PLAY", NULL, uiIconPlay, uiColGreen,
           menuMain.choice == 0, true);
    uiTile(screen, shop, "SHOP", NULL, uiIconShop, uiColAccent,
           menuMain.choice == 3, true);
    uiTile(screen, profile, "PROFILE", NULL, uiIconStar, uiColGreen,
           menuMain.choice == 4, true);
    uiTile(screen, options, "OPTIONS", NULL, uiIconOptions, uiColBlue,
           menuMain.choice == 1, true);
    uiTile(screen, quit, "EXIT", NULL, uiIconExit, uiColRed,
           menuMain.choice == 2, true);

    struct uiHint hints[] = {{"A", "OK"}};
    uiHintBar(screen, hints, 1);

    drawTitleScreen();

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Logic of the page after PLAY: one console, or several (not available yet).
 */
int logicModeMenu(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        if (menuMode.choice != 1) {
            playSound(SOUND_BEEP, sound);
            tournamentSelected = menuMode.choice == 2;
            lobbySlot = 0;
            lobbyRow = 0;
            curScene = &lobbyMenu;
        } else {
            playSound(SOUND_BEEP, sound);
            netMenuEnter(&modeMenu);
        }
        return 1;
    }
    return handleMenu(&menuMode);
}

void displayModeMenu(void)
{
    restoreSlots();
    useBottomView();
    clearBackground(screen);

    SDL_Rect one = {16, 12, 288, 62};
    SDL_Rect many = {16, 82, 288, 62};
    SDL_Rect tour = {16, 152, 288, 62};

    touchReset();
    touchAdd(one, 0, TOUCH_ROW);
    touchAdd(many, 1, TOUCH_ROW);
    touchAdd(tour, 2, TOUCH_ROW);

    uiTile(screen, one, "ONE CONSOLE", NULL, uiIconOneConsole, uiColGreen,
           menuMode.choice == 0, true);
    uiTile(screen, many, "MULTIPLAYER", NULL, uiIconManyConsoles,
           uiColBlue, menuMode.choice == 1, true);
    uiTile(screen, tour, "TOURNAMENT", NULL, uiIconTrophy, uiColAccent,
           menuMode.choice == 2, true);

    struct uiHint hints[] = {{"A", "OK"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 2);

    drawTitleScreen();

    if (presentFrame() == -1)
        exit(1);
}

/* The rows below the slots. Which are shown depends on the slot. */
enum { ROW_STATE, ROW_NAME, ROW_TRAIL, ROW_HEAD, ROW_CONTROLS, ROW_LEVEL,
       ROW_GO };

static int lobbyRows(int slot, int rows[6])
{
    int n = 0;
    rows[n++] = ROW_STATE;
    rows[n++] = ROW_NAME;
    if (slotMode[slot] != SLOT_OFF)
        rows[n++] = ROW_TRAIL;
    if (slotMode[slot] == SLOT_AI)
        rows[n++] = ROW_LEVEL;
    if (slotMode[slot] == SLOT_HUMAN) {
        rows[n++] = ROW_HEAD;
        rows[n++] = ROW_CONTROLS;
    }
    rows[n++] = ROW_GO;
    return n;
}

/* The most humans on one console: the button groups below. */
#define MAX_HUMANS 3

/**
 * How many human slots there are, not counting `except`.
 */
static int humanCount(int except)
{
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (i != except && slotMode[i] == SLOT_HUMAN)
            ++n;
    }
    return n;
}

/**
 * The number of a human slot, P1, P2...: it follows the order of the slots. For a
 * slot that is not human, the number it would get.
 */
static int humanNumber(int slot)
{
    int n = 1;
    for (int i = 0; i < slot; ++i) {
        if (slotMode[i] == SLOT_HUMAN)
            ++n;
    }
    return n;
}

/**
 * Gives a slot that becomes human a group of buttons that no other human uses.
 * P1 steers with L and R, P2 with the D-pad, P3 with Y and A.
 */
static void assignHumanButtons(int slot)
{
    const button groups[MAX_HUMANS][3] = {
        {JOY_BUTTON(0, N3DS_BUTTON_L), JOY_BUTTON(0, N3DS_BUTTON_R),
         JOY_BUTTON(0, N3DS_BUTTON_X)},
        {JOY_BUTTON(0, JOY_DIR_LEFT << 4), JOY_BUTTON(0, JOY_DIR_RIGHT << 4),
         JOY_BUTTON(0, JOY_DIR_DOWN << 4)},
        {JOY_BUTTON(0, N3DS_BUTTON_Y), JOY_BUTTON(0, N3DS_BUTTON_A),
         JOY_BUTTON(0, N3DS_BUTTON_B)}
    };
    struct player *p = &players[slot];

    /* Keep the current buttons if they are buttons and nobody else has them. */
    bool taken = !(isJoyButton(p->lkey) || isJoyAxis(p->lkey));
    for (int i = 0; i < MAX_PLAYERS && !taken; ++i) {
        if (i != slot && slotMode[i] == SLOT_HUMAN && players[i].lkey == p->lkey)
            taken = true;
    }
    if (!taken)
        return;

    for (int g = 0; g < MAX_HUMANS; ++g) {
        bool used = false;
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (i != slot && slotMode[i] == SLOT_HUMAN
                    && players[i].lkey == groups[g][0])
                used = true;
        }
        if (!used) {
            p->lkey = groups[g][0];
            p->rkey = groups[g][1];
            p->wkey = groups[g][2];
            return;
        }
    }
}

bool slotColorTaken(int slot, int color)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (i != slot && slotMode[i] != SLOT_OFF && players[i].color == color)
            return true;
    }
    return false;
}

void setSlotColor(int slot, int color)
{
    struct player *p = &players[slot];
    bool followName = nameIsDefault(p->name);

    p->color = color;
    if (followName)
        snprintf(p->name, sizeof(p->name), "%s", colorName(color));
}

/* A slot that joins takes a free color if its own is in use. */
static void claimColor(int slot)
{
    bool usable = !slotColorTaken(slot, players[slot].color)
        && shopColorOwned(players[slot].color);
    for (int c = 0; !usable && c < N_COLORS; ++c) {
        if (!slotColorTaken(slot, c) && shopColorOwned(c)) {
            setSlotColor(slot, c);
            usable = true;
        }
    }
}

/**
 * Next state of a slot: OFF, AI, human, OFF... At most MAX_HUMANS humans, after
 * that a slot goes from AI to OFF.
 */
static void cycleSlot(int slot)
{
    if (slotMode[slot] == SLOT_OFF) {
        slotMode[slot] = SLOT_AI;
        claimColor(slot);
    } else if (slotMode[slot] == SLOT_AI && humanCount(slot) < MAX_HUMANS) {
        slotMode[slot] = SLOT_HUMAN;
        assignHumanButtons(slot);
    } else {
        slotMode[slot] = SLOT_OFF;
    }
}

/**
 * Sets the state of a slot directly (by touch). A human is only possible if
 * there are fewer than MAX_HUMANS others, else the slot stays as it is.
 */
static void setSlotState(int slot, int mode)
{
    for (int i = 0; i < 3 && slotMode[slot] != mode; ++i)
        cycleSlot(slot);
}

/**
 * Starts the game from the lineup, if at least two slots are in.
 */
void lobbyStart(void)
{
    int idx[MAX_PLAYERS];
    if (activeSlots(idx) < (tournamentSelected ? 3 : MIN_PLAYERS)) {
        playSound(SOUND_BEP, sound);
        return;
    }
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (slotMode[i] != SLOT_OFF)
            claimColor(i);
    }
    playSound(SOUND_BEEP, sound);
    saveSettings();
    rulesRow = 0;
    curScene = &rulesMenu;
}

static void lobbyActivate(int row)
{
    int slot = lobbySlot;

    switch (row) {
    case ROW_STATE:
        playSound(SOUND_BEEP, sound);
        cycleSlot(slot);
        break;
    case ROW_NAME:
        playSound(SOUND_BEEP, sound);
        setNextName(slot);
        break;
    case ROW_TRAIL:
        playSound(SOUND_BEEP, sound);
        slotMenuOpen(SLOTMENU_TRAIL, slot);
        break;
    case ROW_HEAD:
        playSound(SOUND_BEEP, sound);
        slotMenuOpen(SLOTMENU_HEAD, slot);
        break;
    case ROW_LEVEL: {
        /* Easy, normal, hard, and round again. */
        static const unsigned char order[N_AI_LEVELS] = {AI_EASY, AI_NORMAL, AI_HARD};
        int at = 0;
        for (int i = 0; i < N_AI_LEVELS; ++i) {
            if (order[i] == players[slot].aiLevel)
                at = i;
        }
        players[slot].aiLevel = order[(at + 1) % N_AI_LEVELS];
        playSound(SOUND_BEEP, sound);
        break;
    }
    case ROW_CONTROLS:
        playSound(SOUND_BEEP, sound);
        slotMenuOpen(SLOTMENU_CONTROLS, slot);
        break;
    case ROW_GO:
        lobbyStart();
        break;
    }
}

int logicLobby(void)
{
    int rows[6];
    int n = lobbyRows(lobbySlot, rows);
    if (lobbyRow >= n)
        lobbyRow = n - 1;

    if (enterButtonDown()) {
        clearEnterButtons();
        lobbyActivate(rows[lobbyRow]);
        return 1;
    }

    int oldSlot = lobbySlot;
    int oldRow = lobbyRow;
    if (menuButtonQuery(KEY_RIGHT))
        lobbySlot = (lobbySlot + 1) % MAX_PLAYERS;
    else if (menuButtonQuery(KEY_LEFT))
        lobbySlot = (lobbySlot + MAX_PLAYERS - 1) % MAX_PLAYERS;
    else if (menuButtonQuery(KEY_DOWN) && lobbyRow < n - 1)
        ++lobbyRow;
    else if (menuButtonQuery(KEY_UP) && lobbyRow > 0)
        --lobbyRow;

    if (lobbySlot != oldSlot || lobbyRow != oldRow) {
        playSound(SOUND_BEEP, sound);
        /* The new slot may show other rows. */
        n = lobbyRows(lobbySlot, rows);
        if (lobbyRow >= n)
            lobbyRow = n - 1;
        return 1;
    }
    return 0;
}

/* The color and the short name of a slot's state. */
static SDL_Color slotStateColor(int mode)
{
    return mode == SLOT_HUMAN ? uiColGreen : mode == SLOT_AI ? uiColBlue
                                                              : uiColOff;
}

/* The short name of a state: OFF, AI, or P1, P2... for a human slot. For the
 * state "human" of a slot that is not human, the number it would get. */
static void slotStateName(char *buf, int slot, int mode)
{
    if (mode == SLOT_HUMAN)
        snprintf(buf, 4, "P%d", humanNumber(slot));
    else
        snprintf(buf, 4, "%s", mode == SLOT_AI ? "AI" : "OFF");
}

/* The eight slots on the top screen, two rows of four. */
void drawLobbyTop(void)
{
    const int w = 88, h = 100, gap = 8, x0 = 12, y0 = 14;

    useTopView();
    clearBackground(screen);

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        struct player *p = &players[i];
        bool selected = i == lobbySlot;
        bool on = slotMode[i] != SLOT_OFF;
        SDL_Color slotColor = colors[p->color];
        SDL_Rect cell = {x0 + (i % 4) * (w + gap), y0 + (i / 4) * (h + gap),
                         w, h};

        SDL_Color border = selected ? uiColAccent
            : on ? uiMix(uiColPanel, slotColor, 55) : uiMix(uiColPanel, uiColOff, 40);
        cell = uiCard(screen, cell, uiColPanel, border, selected);

        int cx = cell.x + w / 2;
        uiBall(screen, cx, cell.y + 34, 8, 2,
               on ? slotColor : uiMix(uiColPanel, uiColOff, 60));

        char name[11];
        snprintf(name, sizeof(name), "%s", p->name);
        uiText(screen, FONT_BROADCAST, name, on ? slotColor : uiColOff, cx,
               cell.y + 60, UI_CENTER);
        char state[4];
        slotStateName(state, i, slotMode[i]);
        uiText(screen, FONT_MENU, state, slotStateColor(slotMode[i]), cx,
               cell.y + 76, UI_CENTER);
    }
}

/* One row of the details: a panel with a label, the value is drawn by the caller. */
static int lobbyRowH = 26;      /* Smaller on the rules page, which has more rows */

SDL_Rect lobbyRowPanel(int y, bool focused, const char *label)
{
    SDL_Rect r = {16, y, 288, lobbyRowH};
    uiPanel(screen, r, uiColPanel,
            focused ? uiColAccent : uiMix(uiColPanel, uiColDim, 50));
    uiText(screen, FONT_BROADCAST, label, uiColDim, r.x + 10,
           r.y + (r.h - 8) / 2, UI_LEFT);
    return r;
}

/* The three state buttons of a slot, in a row of the details. */
static void drawStateButtons(SDL_Rect row, int slot, int mode)
{
    const int modes[3] = {SLOT_OFF, SLOT_AI, SLOT_HUMAN};
    for (int i = 0; i < 3; ++i) {
        /* Right aligned, 10 pixels from the edge: 3 buttons of 48, gaps of 4. */
        SDL_Rect b = {row.x + row.w - 10 - (3 * 48 + 2 * 4) + i * 52,
                      row.y + 3, 48, row.h - 6};
        touchAdd(b, ROW_STATE, modes[i]);
        SDL_Color c = slotStateColor(modes[i]);
        bool active = modes[i] == mode;
        uiPanel(screen, b, active ? uiMix(uiColPanel, c, 35) : uiColPanel,
                active ? c : uiMix(uiColPanel, c, 30));
        char name[4];
        slotStateName(name, slot, modes[i]);
        uiText(screen, FONT_BROADCAST, name,
               active ? c : uiMix(uiColPanel, c, 55), b.x + b.w / 2,
               b.y + (b.h - 8) / 2,
               UI_CENTER);
    }
}

/* The details of the selected slot on the bottom screen. */
static void drawLobbyBottom(void)
{
    useBottomView();
    clearBackground(screen);
    touchReset();

    struct player *p = &players[lobbySlot];
    int mode = slotMode[lobbySlot];
    int rows[6];
    int n = lobbyRows(lobbySlot, rows);
    if (lobbyRow >= n)
        lobbyRow = n - 1;

    char title[16];
    snprintf(title, sizeof(title), "SLOT %d", lobbySlot + 1);
    uiText(screen, FONT_MENU, title, colors[p->color], 16, 8, UI_LEFT);
    if (tournamentSelected)
        uiText(screen, FONT_BROADCAST, "TOURNAMENT (3+)", uiColAccent, 148, 14,
               UI_LEFT);

    /* The top screen can't be touched: arrows choose the slot. */
    SDL_Rect prev = {212, 2, 48, 28};
    SDL_Rect next = {262, 2, 48, 28};
    touchAdd(prev, TOUCH_PREV, 0);
    touchAdd(next, TOUCH_NEXT, 0);
    uiText(screen, FONT_MENU, "<", uiColAccent, prev.x + prev.w / 2, 8, UI_CENTER);
    uiText(screen, FONT_MENU, ">", uiColAccent, next.x + next.w / 2, 8, UI_CENTER);

    int idx[MAX_PLAYERS];
    bool canStart = activeSlots(idx) >= MIN_PLAYERS;
    const char *action = "CHANGE";

    int y = 34;
    for (int i = 0; i < n; ++i) {
        bool focused = i == lobbyRow;

        switch (rows[i]) {
        case ROW_STATE: {
            SDL_Rect r = lobbyRowPanel(y, focused, "STATE");
            drawStateButtons(r, lobbySlot, mode);
            touchAdd(r, ROW_STATE, TOUCH_ROW);
            if (focused)
                action = "CHANGE";
            break;
        }
        case ROW_NAME: {
            SDL_Rect r = lobbyRowPanel(y, focused, "NAME");
            touchAdd(r, ROW_NAME, TOUCH_ROW);
            uiText(screen, FONT_MENU, p->name, colors[p->color], r.x + r.w - 10,
                   r.y + (r.h - 16) / 2, UI_RIGHT);
            if (focused)
                action = "EDIT";
            break;
        }
        case ROW_TRAIL: {
            SDL_Rect r = lobbyRowPanel(y, focused, "TRAIL");
            touchAdd(r, ROW_TRAIL, TOUCH_ROW);
            uiText(screen, FONT_MENU, colorName(p->color), colors[p->color],
                   r.x + r.w - 10, r.y + (r.h - 16) / 2, UI_RIGHT);
            if (focused)
                action = "OPEN";
            break;
        }
        case ROW_HEAD: {
            SDL_Rect r = lobbyRowPanel(y, focused, "HEAD");
            touchAdd(r, ROW_HEAD, TOUCH_ROW);
            unsigned char style = shopOwnedHead(p->headStyle);
            uiText(screen, FONT_MENU, shopHeadName(style),
                   style ? uiColAccent : uiColText, r.x + r.w - 10,
                   r.y + (r.h - 16) / 2, UI_RIGHT);
            if (focused)
                action = "OPEN";
            break;
        }
        case ROW_LEVEL: {
            static const char *const name[N_AI_LEVELS] = {"NORMAL", "EASY", "HARD"};
            SDL_Rect r = lobbyRowPanel(y, focused, "SKILL");
            touchAdd(r, ROW_LEVEL, TOUCH_ROW);
            uiText(screen, FONT_MENU, name[p->aiLevel % N_AI_LEVELS],
                   p->aiLevel == AI_HARD ? uiColRed
                   : p->aiLevel == AI_EASY ? uiColGreen : uiColText,
                   r.x + r.w - 10, r.y + (r.h - 16) / 2, UI_RIGHT);
            if (focused)
                action = "CHANGE";
            break;
        }
        case ROW_CONTROLS: {
            SDL_Rect r = lobbyRowPanel(y, focused, "CONTROLS");
            touchAdd(r, ROW_CONTROLS, TOUCH_ROW);
            uiText(screen, FONT_MENU, ">", uiColAccent, r.x + r.w - 10,
                   r.y + (r.h - 16) / 2, UI_RIGHT);
            if (focused)
                action = "OPEN";
            break;
        }
        case ROW_GO: {
            SDL_Rect go = {16, 186, 288, 30};
            touchAdd(go, ROW_GO, TOUCH_ROW);
            uiTile(screen, go, "GO", NULL, NULL, uiColGreen, focused, canStart);
            if (focused)
                action = "GO";
            continue;
        }
        }
        y += 30;
    }

    struct uiHint hints[] = {{"A", action, 6}, {"<>", "SLOT"}, {"B", "BACK"},
                             {"START", "GO"}};
    uiHintBar(screen, hints, 4);
}

void displayLobby(void)
{
    restoreSlots();
    drawLobbyTop();
    drawLobbyBottom();

    if (presentFrame() == -1)
        exit(1);
}

/* ---- The rules page: what a match is played with ---- */

enum { RULE_POINTS, RULE_WEAPONS, RULE_HOLES, RULE_WALLS, RULE_SPEED, RULE_GO,
       N_RULE_ROWS };

static const char *const ruleTitle[N_RULE_ROWS] = {
    "MAX POINTS", "WEAPONS", "HOLES", "WALLS", "SPEED", "GO"
};

/* A short explanation of each rule, shown on the top screen. At most 44
 * characters per line. */
static const char *const ruleHelp[N_RULE_ROWS][4] = {
    {"THE FIRST PLAYER TO REACH THIS SCORE", "WINS THE MATCH. INF HAS NO LIMIT, YOU",
     "PLAY UNTIL YOU QUIT.", NULL},
    {"OFF: PLAY WITHOUT WEAPONS.", "FIXED: EVERYONE PICKS ONE BEFORE THE GAME.",
     "ITEMS: COLLECT THEM ON THE FIELD.", NULL},
    {"GAPS IN THE TRAILS THAT YOU CAN DRIVE", "THROUGH.", NULL, NULL},
    {"SOLID: THE WALL STOPS YOU. WRAP: YOU COME", "OUT ON THE OTHER SIDE. BOUNCE: THE WALL",
     "THROWS YOU BACK. CLOSE: AFTER 10 SECONDS", "THE WALLS COME IN."},
    {"HOW FAST EVERYONE MOVES AND TURNS.", NULL, NULL, NULL},
    {"START THE MATCH.", NULL, NULL, NULL}
};

static const unsigned int pointChoices[] = {10, 20, 30, 40, 50, 100, 0};
#define N_POINT_CHOICES ((int)(sizeof(pointChoices) / sizeof(pointChoices[0])))
static const int speedChoices[] = {75, 100, 125, 150};
#define N_SPEED_CHOICES ((int)(sizeof(speedChoices) / sizeof(speedChoices[0])))

static int pointsIndex(void)
{
    for (int i = 0; i < N_POINT_CHOICES; ++i) {
        if (pointChoices[i] == scorecap)
            return i;
    }
    return 3;
}

static int speedIndex(void)
{
    for (int i = 0; i < N_SPEED_CHOICES; ++i) {
        if (speedChoices[i] == gameSpeed)
            return i;
    }
    return 1;
}

/**
 * Changes the value of a rule one step. `dir` is 1 for forward, -1 for back;
 * switches just flip.
 */
static void ruleChange(int row, int dir)
{
    switch (row) {
    case RULE_POINTS:
        scorecap = pointChoices[(pointsIndex() + dir + N_POINT_CHOICES)
                                % N_POINT_CHOICES];
        break;
    case RULE_WEAPONS: {
        /* Off, fixed (chosen before the game), items (found on the field). */
        int mode = (weapons ? (itemMode ? 2 : 1) : 0) + dir + 3;
        mode %= 3;
        weapons = mode > 0;
        itemMode = mode == 2;
        break;
    }
    case RULE_HOLES:
        holes = !holes;
        break;
    case RULE_WALLS:
        wallMode = (wallMode + dir + N_WALL_MODES) % N_WALL_MODES;
        break;
    case RULE_SPEED:
        gameSpeed = speedChoices[(speedIndex() + dir + N_SPEED_CHOICES)
                                 % N_SPEED_CHOICES];
        break;
    }
}

/**
 * Starts the match with the chosen rules.
 */
void rulesStart(void)
{
    playSound(SOUND_ROUND_BEGIN, sound);
    saveSettings();
    if (tournamentSelected)
        tournamentBegin();
    else
        startLocalGame();
}

int logicRules(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        if (rulesRow == RULE_GO) {
            rulesStart();
        } else {
            playSound(SOUND_BEEP, sound);
            ruleChange(rulesRow, 1);
        }
        return 1;
    }

    int oldRow = rulesRow;
    if (menuButtonQuery(KEY_DOWN) && rulesRow < N_RULE_ROWS - 1)
        ++rulesRow;
    else if (menuButtonQuery(KEY_UP) && rulesRow > 0)
        --rulesRow;
    else if (menuButtonQuery(KEY_RIGHT) && rulesRow != RULE_GO)
        ruleChange(rulesRow, 1);
    else if (menuButtonQuery(KEY_LEFT) && rulesRow != RULE_GO)
        ruleChange(rulesRow, -1);
    else
        return 0;

    playSound(SOUND_BEEP, sound);
    (void)oldRow;
    return 1;
}

/* The players and the explanation of the selected rule, on the top screen. */
static void drawRulesTop(void)
{
    useTopView();
    clearBackground(screen);

    int idx[MAX_PLAYERS];
    int n = activeSlots(idx);
    if (duelmode && n > 2)
        n = 2;

    const int step = 48;
    int x0 = (screen->w - n * step) / 2 + step / 2;
    for (int k = 0; k < n; ++k) {
        int slot = idx[k];
        int cx = x0 + k * step;
        char state[4];
        slotStateName(state, slot, slotMode[slot]);
        uiBall(screen, cx, 40, 8, 2, colors[players[slot].color]);
        uiText(screen, FONT_BROADCAST, state, slotStateColor(slotMode[slot]), cx,
               66, UI_CENTER);
    }

    SDL_Rect card = {12, 100, 376, 124};
    card = uiCard(screen, card, uiColPanel, uiMix(uiColPanel, uiColBlue, 55),
                  false);
    uiText(screen, FONT_MENU, ruleTitle[rulesRow],
           rulesRow == RULE_GO ? uiColGreen : uiColBlue, card.x + 14,
           card.y + 12, UI_LEFT);
    for (int i = 0; i < 4 && ruleHelp[rulesRow][i] != NULL; ++i)
        uiText(screen, FONT_BROADCAST, ruleHelp[rulesRow][i], uiColText,
               card.x + 14, card.y + 44 + i * 16, UI_LEFT);
}

/* A row of buttons, one of them active, right aligned in the row (10 pixels
 * from the edge). `enabled` has a bit per button. */
void drawChoice(SDL_Rect row, int id, int w, int n, const char *labels[],
                       int active, unsigned enabled)
{
    int x = row.x + row.w - 10 - (n * w + (n - 1) * 4);

    for (int i = 0; i < n; ++i) {
        SDL_Rect b = {x + i * (w + 4), row.y + 3, w, row.h - 6};
        touchAdd(b, id, i);
        bool on = (enabled >> i) & 1;
        SDL_Color c = on ? uiColBlue : uiColOff;
        bool isActive = i == active;
        uiPanel(screen, b, isActive ? uiMix(uiColPanel, c, 35) : uiColPanel,
                isActive ? c : uiMix(uiColPanel, c, 30));
        uiText(screen, FONT_BROADCAST, labels[i],
               isActive ? c : uiMix(uiColPanel, c, 55), b.x + b.w / 2,
               b.y + (b.h - 8) / 2, UI_CENTER);
    }
}

/* An on/off switch at the right end of a row. */
void drawSwitch(SDL_Rect row, int id, bool on)
{
    touchAdd(row, id, 0);
    SDL_Rect track = {row.x + row.w - 58, row.y + (row.h - 16) / 2, 44, 16};
    SDL_Color c = on ? uiColGreen : uiColOff;
    uiPanel(screen, track, uiMix(uiColPanel, c, on ? 35 : 15), c);
    SDL_Rect knob = {track.x + (on ? 28 : 2), track.y + 2, 14, 12};
    SDL_FillRect(screen, &knob, uiMap(screen, on ? uiColGreen : uiColDim));
    uiText(screen, FONT_BROADCAST, on ? "ON" : "OFF", c, track.x - 8,
           track.y + 4, UI_RIGHT);
}

static void drawRulesBottom(void)
{
    useBottomView();
    clearBackground(screen);
    touchReset();

    uiText(screen, FONT_MENU, "RULES", uiColBlue, 16, 8, UI_LEFT);

    /* Five rows: a bit smaller than elsewhere, to leave room for GO. */
    lobbyRowH = 22;
    int y = 30;
    for (int row = 0; row < RULE_GO; ++row, y += 26) {
        bool focused = row == rulesRow;
        SDL_Rect r = lobbyRowPanel(y, focused, ruleTitle[row]);

        switch (row) {
        case RULE_POINTS: {
            char value[8];
            if (scorecap == 0)
                snprintf(value, sizeof(value), "INF");
            else
                snprintf(value, sizeof(value), "%u", scorecap);
            int cx = r.x + r.w - 70;
            SDL_Rect less = {cx - 54 - 20, r.y, 40, r.h};
            SDL_Rect more = {cx + 54 - 20, r.y, 40, r.h};
            touchAdd(less, RULE_POINTS, -1);
            touchAdd(more, RULE_POINTS, 1);
            int ty = r.y + (r.h - 16) / 2;
            uiText(screen, FONT_MENU, "<", uiColAccent, cx - 54, ty, UI_CENTER);
            uiText(screen, FONT_MENU, value, uiColText, cx, ty, UI_CENTER);
            uiText(screen, FONT_MENU, ">", uiColAccent, cx + 54, ty, UI_CENTER);
            break;
        }
        case RULE_WEAPONS: {
            const char *labels[3] = {"OFF", "FIXED", "ITEMS"};
            drawChoice(r, RULE_WEAPONS, 56, 3, labels, weapons ? (itemMode ? 2 : 1) : 0,
                       0x7);
            break;
        }
        case RULE_HOLES:
            drawSwitch(r, RULE_HOLES, holes);
            break;
        case RULE_WALLS: {
            const char *labels[N_WALL_MODES] = {"SOLID", "WRAP", "BOUNCE", "CLOSE"};
            drawChoice(r, RULE_WALLS, 52, N_WALL_MODES, labels, wallMode, 0xF);
            break;
        }
        case RULE_SPEED: {
            const char *labels[4] = {"75%", "100%", "125%", "150%"};
            drawChoice(r, RULE_SPEED, 40, 4, labels, speedIndex(), 0xF);
            break;
        }
        }
        touchAdd(r, row, TOUCH_ROW);
    }

    lobbyRowH = 26;

    SDL_Rect go = {16, 190, 288, 26};
    touchAdd(go, RULE_GO, TOUCH_ROW);
    uiTile(screen, go, "GO", NULL, NULL, uiColGreen, rulesRow == RULE_GO, true);

    struct uiHint hints[] = {{"A", rulesRow == RULE_GO ? "GO" : "CHANGE", 6},
                             {"<>", "VALUE"}, {"B", "BACK"}, {"START", "GO"}};
    uiHintBar(screen, hints, 4);
}

void displayRules(void)
{
    restoreSlots();
    drawRulesTop();
    drawRulesBottom();

    if (presentFrame() == -1)
        exit(1);
}

/* ---- The options: what always applies, not per match ---- */

enum { OPT_SOUND, OPT_MUSIC, OPT_PARTICLES, OPT_MESSAGES, OPT_BORDER, OPT_SHAKE,
       OPT_BACK, N_OPT_ROWS };


static const char *const optionTitle[N_OPT_ROWS] = {
    "SOUND FX", "MUSIC", "PARTICLES", "MESSAGES", "BORDER", "SHAKE", "BACK"
};

static const char *const optionHelp[N_OPT_ROWS][4] = {
    {"SOUNDS OF THE MENUS, CRASHES AND WEAPONS.", NULL, NULL, NULL},
    {"THE BACKGROUND MUSIC.", NULL, NULL, NULL},
    {"SPARKS WHEN A PLAYER CRASHES AND TRAILS OF", "WEAPONS. TURN THEM OFF IF THE GAME SLOWS",
     "DOWN.", NULL},
    {"SHOWS WHO CRASHED INTO WHOM, ON THE BOTTOM", "SCREEN DURING THE GAME.", NULL, NULL},
    {"DRAWS A WHITE FRAME AROUND THE FIELD. THE", "WALL STILL STOPS YOU WITHOUT IT.", NULL,
     NULL},
    {"THE FIELD SHAKES A LITTLE WHEN A PLAYER", "CRASHES.", NULL, NULL},
    {"BACK TO THE MENU. YOUR CHANGES ARE SAVED.", NULL, NULL, NULL}
};

static bool optionValue(int row)
{
    switch (row) {
    case OPT_SOUND:     return sound;
    case OPT_MUSIC:     return music;
    case OPT_PARTICLES: return particleEffects;
    case OPT_MESSAGES:  return broadcasts;
    case OPT_BORDER:    return border;
    default:            return shake;
    }
}

static void optionToggle(int row)
{
    switch (row) {
    case OPT_SOUND:
        sound = !sound;
        playSound(SOUND_BEEP, sound);
        break;
    case OPT_MUSIC:
        music = !music;
        if (music)
            playBGM();
        else
            stopBGM();
        break;
    case OPT_PARTICLES:
        particleEffects = !particleEffects;
        playSound(SOUND_BEEP, sound);
        break;
    case OPT_MESSAGES:
        broadcasts = !broadcasts;
        playSound(SOUND_BEEP, sound);
        break;
    case OPT_BORDER:
        border = !border;
        playSound(SOUND_BEEP, sound);
        break;
    case OPT_SHAKE:
        shake = !shake;
        playSound(SOUND_BEEP, sound);
        break;
    }
}

/**
 * Leaves the options for the main menu and saves them (writing is fast now, and
 * a 3DS game is often closed from the HOME menu).
 */
static void leaveOptions(void)
{
    playSound(SOUND_BEP, sound);
    initMainMenu();
    saveSettings();
    curScene = &mainMenu;
}

int logicSettingsMenu(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        if (optionsRow == OPT_BACK)
            leaveOptions();
        else
            optionToggle(optionsRow);
        return 1;
    }

    if (menuButtonQuery(KEY_DOWN) && optionsRow < N_OPT_ROWS - 1) {
        ++optionsRow;
    } else if (menuButtonQuery(KEY_UP) && optionsRow > 0) {
        --optionsRow;
    } else if ((menuButtonQuery(KEY_LEFT) || menuButtonQuery(KEY_RIGHT))
               && optionsRow != OPT_BACK) {
        optionToggle(optionsRow);
        return 1;
    } else {
        return 0;
    }

    playSound(SOUND_BEEP, sound);
    return 1;
}

void displaySettingsMenu(void)
{
    /* The top screen: the options' explanation. */
    useTopView();
    clearBackground(screen);

    uiText(screen, FONT_MENU, "OPTIONS", uiColBlue, screen->w / 2, 28, UI_CENTER);
    SDL_Rect card = {12, 70, 376, 130};
    card = uiCard(screen, card, uiColPanel, uiMix(uiColPanel, uiColBlue, 55),
                  false);
    uiText(screen, FONT_MENU, optionTitle[optionsRow], uiColBlue, card.x + 14,
           card.y + 14, UI_LEFT);
    for (int i = 0; i < 4 && optionHelp[optionsRow][i] != NULL; ++i)
        uiText(screen, FONT_BROADCAST, optionHelp[optionsRow][i], uiColText,
               card.x + 14, card.y + 46 + i * 16, UI_LEFT);

    /* The bottom screen: the switches and a button back. */
    useBottomView();
    clearBackground(screen);
    touchReset();

    int y = 8;
    for (int row = 0; row < OPT_BACK; ++row, y += 30) {
        SDL_Rect r = lobbyRowPanel(y, row == optionsRow, optionTitle[row]);
        drawSwitch(r, row, optionValue(row));
    }

    SDL_Rect back = {16, 190, 288, 26};
    touchAdd(back, OPT_BACK, TOUCH_ROW);
    uiTile(screen, back, "BACK", NULL, NULL, uiColBlue, optionsRow == OPT_BACK,
           true);

    struct uiHint hints[] = {{"A", optionsRow == OPT_BACK ? "BACK" : "CHANGE", 6},
                             {"<>", "CHANGE"}, {"B", "BACK"}};
    uiHintBar(screen, hints, 3);

    if (presentFrame() == -1)
        exit(1);
}

/* ---- The weapon page: every human player picks a weapon ---- */

/**
 * Starts the round with the chosen weapons.
 */
void wepStart(void)
{
    resetWeapons();
    playSound(SOUND_ROUND_BEGIN, sound);
    curScene = &gameStart;
}

int logicWepMenu(void)
{
    if (enterButtonDown()) {
        clearEnterButtons();
        wepStart();
        return 1;
    }

    struct player *p = &players[wepCursor];
    if (menuButtonQuery(KEY_RIGHT)) {
        p->weapon = (p->weapon + 1) % N_WEAPONS;
    } else if (menuButtonQuery(KEY_LEFT)) {
        p->weapon = (p->weapon + N_WEAPONS - 1) % N_WEAPONS;
    } else if (menuButtonQuery(KEY_DOWN)) {
        wepCursor = nextHuman(wepCursor, 1);
    } else if (menuButtonQuery(KEY_UP)) {
        wepCursor = nextHuman(wepCursor, -1);
    } else {
        return 0;
    }

    playSound(SOUND_BEEP, sound);
    return 1;
}

/* The weapons are a grid on the bottom screen, where they can be touched. */
#define WEP_COLS    4
#define WEP_GAP     6

static SDL_Rect wepCell(int i)
{
    int w = wepIcons[0]->w, h = wepIcons[0]->h;
    int x0 = (320 - (WEP_COLS * w + (WEP_COLS - 1) * WEP_GAP)) / 2;
    return (SDL_Rect){x0 + (i % WEP_COLS) * (w + WEP_GAP),
                      4 + (i / WEP_COLS) * (h + WEP_GAP), w, h};
}

/* Who has chosen what, and the weapon of the player in turn, described. */
static void drawWepTop(void)
{
    useTopView();
    clearBackground(screen);

    for (int k = 0; k < nPlayers; ++k) {
        struct player *p = &players[k];
        SDL_Rect cell = {8 + (k % 2) * 194, 8 + (k / 2) * 34, 190, 30};
        bool turn = k == wepCursor;
        SDL_Color c = p->ai ? uiMix(uiColPanel, colors[p->color], 55)
                            : colors[p->color];

        uiPanel(screen, cell, uiColPanel,
                turn ? uiColAccent : uiMix(uiColPanel, c, 40));
        uiBall(screen, cell.x + 12, cell.y + 15, 4, 1, c);
        char name[9];
        snprintf(name, sizeof(name), "%s", p->name);
        uiText(screen, FONT_BROADCAST, name, c, cell.x + 24, cell.y + 11,
               UI_LEFT);
        SDL_Rect icon = {cell.x + cell.w - 24, cell.y + 7, 0, 0};
        SDL_BlitSurface(smallWepIcons[p->weapon], NULL, screen, &icon);
    }

    struct player *p = &players[wepCursor];
    struct weapon w = wep_list[p->weapon];
    SDL_Rect card = {8, 150, 384, 82};
    card = uiCard(screen, card, uiColPanel, uiMix(uiColPanel, colors[p->color], 60),
                  false);
    uiText(screen, FONT_MENU, w.name, colors[p->color], card.x + 12, card.y + 10,
           UI_LEFT);
    uiText(screen, FONT_BROADCAST, w.desc1, uiColText, card.x + 12, card.y + 34,
           UI_LEFT);
    uiText(screen, FONT_BROADCAST, w.desc2, uiColText, card.x + 12, card.y + 48,
           UI_LEFT);
    char charges[20];
    snprintf(charges, sizeof(charges), "CHARGES: %d", w.charges);
    uiText(screen, FONT_BROADCAST, charges, uiColAccent, card.x + 12,
           card.y + 62, UI_LEFT);
}

/* The grid of weapons: tap one for the player in turn. */
static void drawWepBottom(void)
{
    useBottomView();
    clearBackground(screen);
    touchReset();

    for (int i = 0; i < N_WEAPONS; ++i) {
        SDL_Rect offset = wepCell(i);
        touchAdd(offset, i, TOUCH_ROW);

        /* A yellow frame around the weapon of the player that is choosing */
        if (players[wepCursor].weapon == i) {
            SDL_Rect frame = {offset.x - 3, offset.y - 3, offset.w + 6,
                              offset.h + 6};
            uiPanel(screen, frame, uiColAccent, uiColAccent);
            SDL_Rect inner = {offset.x - 1, offset.y - 1, offset.w + 2,
                              offset.h + 2};
            SDL_FillRect(screen, &inner, uiMap(screen, uiColPanel));
        }

        SDL_BlitSurface(wepIcons[0], NULL, screen, &offset);
        SDL_BlitSurface(wepIcons[i + 1], NULL, screen, &offset);

        /* A dot in the color of every player who has chosen it */
        for (int j = 0; j < nPlayers; ++j) {
            if (players[j].weapon == i)
                uiBall(screen, offset.x + 6 + j * 7, offset.y + 6, 2, 1,
                       colors[players[j].color]);
        }
    }

    SDL_Rect go = {80, 192, 160, 24};
    touchAdd(go, TOUCH_GO, TOUCH_ROW);
    uiTile(screen, go, "GO", NULL, NULL, uiColGreen, false, true);

    struct uiHint hints[] = {{"A", "GO"}, {"<>", "PICK"}, {"^v", "PLAYER"},
                             {"START", "GO"}};
    uiHintBar(screen, hints, 4);
}

void displayWepMenu(void)
{
    drawWepTop();
    drawWepBottom();

    if (presentFrame() == -1)
        exit(1);
}

/* ---- After a round and after a match ---- */

static int roundWinner = -1;    /* The last player alive, or -1 for a draw */
static Uint32 roundEndAt = 0;   /* When the round ended */
#define AUTO_NEXT_MS 2500       /* A game of computer players goes on after this */

/**
 * The score page on the bottom screen, in the game and after a round: one row
 * per player with the name, the weapon and its charges, the score as a bar and
 * as a number. `highlight` gets a yellow frame; with `dimDead` the players that
 * are out are dimmed.
 */
void drawScoreBars(int highlight, bool dimDead)
{
    useBottomView();
    clearBackground(screen);

    uiText(screen, FONT_MENU, "SCORE", uiColBlue, 16, 8, UI_LEFT);

    /* The bars end at the score cap, or at the best score if there is none. */
    int most = scorecap > 0 ? (int)scorecap : 10;
    for (int i = 0; i < nPlayers; ++i) {
        if (players[i].score > most)
            most = players[i].score;
    }

    for (int k = 0; k < nPlayers; ++k) {
        struct player *p = &players[k];
        SDL_Rect row = {16, 32 + k * 22, 288, 20};
        bool out = dimDead && !p->alive;
        SDL_Color color = out ? uiMix(uiColPanel, colors[p->color], 45)
                              : colors[p->color];

        uiPanel(screen, row, uiColPanel,
                k == highlight ? uiColAccent : uiMix(uiColPanel, color, 40));
        uiBall(screen, row.x + 12, row.y + 10, 4, 1, color);

        char name[9];
        snprintf(name, sizeof(name), "%s", p->name);
        uiText(screen, FONT_BROADCAST, name, color, row.x + 24, row.y + 6,
               UI_LEFT);

        /* The weapon: its icon (framed while it is active) and the charges left */
        if (weapons && !(itemMode && p->wep_count == 0 && p->wep_time == WEP_NONACTIVE)) {
            SDL_Rect icon = {row.x + 92, row.y + 2, 0, 0};
            if (p->wep_time != WEP_NONACTIVE) {
                SDL_Rect frame = {icon.x - 1, icon.y - 1, 18, 18};
                SDL_FillRect(screen, &frame, uiMap(screen, uiColAccent));
            }
            SDL_BlitSurface(smallWepIcons[p->weapon], NULL, screen, &icon);

            char charges[6];
            snprintf(charges, sizeof(charges), "x%d", p->wep_count);
            uiText(screen, FONT_BROADCAST, charges,
                   p->wep_count > 0 ? uiColText : uiColOff, row.x + 112,
                   row.y + 6, UI_LEFT);
        }

        SDL_Rect track = {row.x + 142, row.y + 5, 94, 10};
        SDL_FillRect(screen, &track, uiMap(screen, uiMix(uiColPanel, color, 15)));
        SDL_Rect fill = {track.x, track.y, track.w * p->score / most, track.h};
        SDL_FillRect(screen, &fill, uiMap(screen, color));

        char score[8];
        snprintf(score, sizeof(score), "%d", p->score);
        uiText(screen, FONT_MENU, score, out ? uiColOff : uiColText,
               row.x + row.w - 6, row.y + 2, UI_RIGHT);
    }
}

/**
 * The two latest messages ("Red crashed into Blue") under the score page.
 */
void drawMessages(void)
{
    if (!broadcasts)
        return;

    for (int i = 0; i < 2; ++i) {
        if (broadcast[i] != NULL) {
            SDL_Rect offset = {16, bottomView->h - 6
                                   - (broadcast[i]->h + 2) * (i + 1), 0, 0};
            SDL_BlitSurface(broadcast[i], NULL, bottomView, &offset);
        }
    }
}

/**
 * The round is over: a banner on the frozen field, the scores below. The next
 * round follows with A.
 */
void beginRoundEnd(void)
{
    roundWinner = -1;
    for (int i = 0; i < nPlayers; ++i) {
        if (players[i].alive)
            roundWinner = i;
    }
    curScene = &roundEndScene;
    roundEndAt = SDL_GetTicks();
    statsRoundEnd(roundWinner);
    saveLineupSettings();
    if (netGameRole() == NET_ROLE_HOST)
        netGameRoundEndBegin();
    displayRoundEnd();
}

int logicRoundEnd(void)
{
    if (netGameRole() != NET_ROLE_NONE) {
        int t = netGameTick();
        if (t != NET_TICK_CONTINUE)
            return t == NET_TICK_REDRAW;

        /* The host: READY, and the next round when all are ready. */
        bool changed = false;
        if (enterButtonDown()) {
            clearEnterButtons();
            netGameReadyPress();
            changed = true;
        }
        if (netGameReadyComplete()) {
            clearSurface(gameScreen);
            newRound();
            return 1;
        }
        return netGameStatusChanged() || changed;
    }

    /* Only A goes on: the score is looked at as long as one likes. Computer
     * players alone have nobody to press it, so the next round starts by itself. */
    bool alone = true;
    for (int i = 0; i < nPlayers; ++i)
        alone = alone && players[i].ai;
    if (enterButtonDown() || (alone && SDL_GetTicks() - roundEndAt > AUTO_NEXT_MS)) {
        clearEnterButtons();
        clearSurface(gameScreen);
        newRound();
        return 1;
    }
    return 0;
}

void displayRoundEnd(void)
{
    /* The field stays as it was, with a banner in the middle. */
    useTopView();
    /* With several consoles the READY situation is part of the banner: it is
     * drawn over a card, so the old text is gone. */
    int extra = netGameRole() != NET_ROLE_NONE ? 26 : 0;
    SDL_Rect banner = {screen->w / 2 - 120, screen->h / 2 - 30 - extra / 2,
                       240, 60 + extra};
    SDL_Color color = roundWinner >= 0 ? colors[players[roundWinner].color]
                                       : uiColAccent;
    banner = uiCard(screen, banner, uiColPanel, color, false);
    char name[11];
    snprintf(name, sizeof(name), "%s",
             roundWinner >= 0 ? players[roundWinner].name : "DRAW");
    uiText(screen, FONT_MENU, name, color, banner.x + banner.w / 2,
           banner.y + 12, UI_CENTER);
    if (roundWinner >= 0)
        uiText(screen, FONT_BROADCAST, "WINS THE ROUND", uiColText,
               banner.x + banner.w / 2, banner.y + 38, UI_CENTER);

    if (netGameRole() != NET_ROLE_NONE) {
        char status[32];
        netGameStatus(status, sizeof(status));
        uiText(screen, FONT_BROADCAST, status, uiColAccent, screen->w / 2,
               banner.y + 66, UI_CENTER);
    }
    if (statsNewTitle() != NULL) {
        char line[40];
        snprintf(line, sizeof(line), "NEW TITLE: %s", statsNewTitle());
        uiText(screen, FONT_MENU, line, uiColAccent, screen->w / 2,
               banner.y - 28, UI_CENTER);
    }

    drawScoreBars(roundWinner, false);
    touchReset();
    SDL_Rect everywhere = {0, 0, screen->w, screen->h - 20};
    touchAdd(everywhere, 0, TOUCH_ROW);
    struct uiHint hints[] = {{"A", netGameRole() != NET_ROLE_NONE ? "READY" : "NEXT"}};
    uiHintBar(screen, hints, 1);

    if (presentFrame() == -1)
        exit(1);
}

static struct menu menuMatch = {
    2,
    0,
};

/**
 * Somebody has reached the score cap: the winner page with AGAIN and MENU.
 */
void beginMatchEnd(void)
{
    curScene = &matchEndScene;
    menuMatch.choice = 0;
    statsMatchEnd(matchWinner);
    saveLineupSettings();
    if (tournamentActive() && netGameRole() == NET_ROLE_NONE
        && matchWinner >= 0) {
        /* A duel of the tournament: the bracket goes on instead. */
        endRound();
        tournamentDuelOver(packedSlot[matchWinner]);
        curScene->displayFunc();
        return;
    }
    if (netGameRole() == NET_ROLE_HOST)
        netGameRoundEndBegin();
    displayMatchEnd();
}

int logicMatchEnd(void)
{
    if (netGameRole() != NET_ROLE_NONE) {
        int t = netGameTick();
        if (t != NET_TICK_CONTINUE)
            return t == NET_TICK_REDRAW;

        /* The host: READY, and the same match again when all are ready. */
        bool changed = false;
        if (enterButtonDown()) {
            clearEnterButtons();
            netGameReadyPress();
            changed = true;
        }
        if (netGameReadyComplete()) {
            playSound(SOUND_ROUND_BEGIN, sound);
            initPlayers2();
            clearSurface(gameScreen);
            newRound();
            return 1;
        }
        return netGameStatusChanged() || changed;
    }

    if (enterButtonDown()) {
        clearEnterButtons();
        if (menuMatch.choice == 0) {
            /* The same players, rules and weapons: only the scores start at 0. */
            playSound(SOUND_ROUND_BEGIN, sound);
            initPlayers2();
            clearSurface(gameScreen);
            newRound();
        } else {
            playSound(SOUND_BEP, sound);
            endRound();
            curScene = &mainMenu;
        }
        return 1;
    }

    if (menuButtonQuery(KEY_DOWN) || menuButtonQuery(KEY_UP)) {
        menuMatch.choice = !menuMatch.choice;
        playSound(SOUND_BEEP, sound);
        return 1;
    }
    return 0;
}

void displayMatchEnd(void)
{
    /* Top: the winner, big, and everybody's final score. */
    useTopView();
    clearBackground(screen);

    struct player *w = &players[matchWinner];
    SDL_Color color = colors[w->color];
    uiBall(screen, screen->w / 2, 62, 12, 3, color);

    char text[32];
    snprintf(text, sizeof(text), "%s", w->name);
    uiText(screen, FONT_MENU, text, color, screen->w / 2, 112, UI_CENTER);
    uiText(screen, FONT_MENU, "WINS!", uiColAccent, screen->w / 2, 134,
           UI_CENTER);

    const int step = 44;
    int x0 = (screen->w - nPlayers * step) / 2 + step / 2;
    for (int k = 0; k < nPlayers; ++k) {
        char score[8];
        snprintf(score, sizeof(score), "%d", players[k].score);
        uiBall(screen, x0 + k * step, 186, 4, 2, colors[players[k].color]);
        uiText(screen, FONT_MENU, score, uiColText, x0 + k * step, 204,
               UI_CENTER);
    }

    /* Bottom: play again or leave. */
    useBottomView();
    clearBackground(screen);

    SDL_Rect again = {16, 22, 288, 70};
    SDL_Rect menu = {16, 108, 288, 60};

    touchReset();
    touchAdd(again, 0, TOUCH_ROW);
    if (netGameRole() != NET_ROLE_NONE) {
        /* Several consoles: everybody says READY for the next match. */
        char status[32];
        netGameStatus(status, sizeof(status));
        uiTile(screen, again, "AGAIN", NULL, uiIconRestart, uiColGreen, true,
               true);
        uiText(screen, FONT_MENU, status, uiColAccent, 160, 120, UI_CENTER);
        struct uiHint netHints[] = {{"A", "READY"}, {"B", "LOBBY"}};
        uiHintBar(screen, netHints, netGameRole() == NET_ROLE_HOST ? 2 : 1);
        if (presentFrame() == -1)
            exit(1);
        return;
    }
    touchAdd(menu, 1, TOUCH_ROW);
    uiTile(screen, again, "AGAIN", NULL, uiIconRestart, uiColGreen,
           menuMatch.choice == 0, true);
    uiTile(screen, menu, "MENU", NULL, uiIconHome, uiColRed,
           menuMatch.choice == 1, true);

    struct uiHint hints[] = {{"A", "OK"}};
    uiHintBar(screen, hints, 1);

    if (presentFrame() == -1)
        exit(1);
}

/* ---- Touch ---- */

static void touchTitle(int id, int value, bool released)
{
    (void)id;
    (void)value;
    if (released)
        touchPressA();
}

static void touchMainMenu(int id, int value, bool released)
{
    (void)value;
    if (!released)
        menuMain.choice = id;
    else
        touchPressA();
}

static void touchModeMenu(int id, int value, bool released)
{
    (void)value;
    if (!released)
        menuMode.choice = id;
    else
        touchPressA();
}

static void touchPause(int id, int value, bool released)
{
    (void)value;
    if (!released) {
        if (id != 3)
            pauseConfirm = false;
        menuPause.choice = id;
    } else {
        touchPressA();
    }
}

static void touchOptions(int id, int value, bool released)
{
    (void)value;
    if (!released)
        optionsRow = id;
    else if (id == OPT_BACK)
        leaveOptions();
    else
        optionToggle(id);
}

static void touchRoundEnd(int id, int value, bool released)
{
    (void)id;
    (void)value;
    if (released)
        touchPressA();
}

static void touchMatchEnd(int id, int value, bool released)
{
    (void)value;
    if (!released)
        menuMatch.choice = id;
    else
        touchPressA();
}

static void touchLobby(int id, int value, bool released)
{
    if (id == TOUCH_PREV || id == TOUCH_NEXT) {
        if (released) {
            lobbySlot = (lobbySlot + (id == TOUCH_NEXT ? 1 : MAX_PLAYERS - 1))
                % MAX_PLAYERS;
            playSound(SOUND_BEEP, sound);
        }
        return;
    }

    if (!released) {
        int rows[6];
        int n = lobbyRows(lobbySlot, rows);
        for (int i = 0; i < n; ++i) {
            if (rows[i] == id)
                lobbyRow = i;
        }
    } else if (id == ROW_STATE) {
        if (value >= 0) {
            playSound(SOUND_BEEP, sound);
            setSlotState(lobbySlot, value);
        }
    } else {
        touchPressA();
    }
}

static void touchRules(int id, int value, bool released)
{
    if (!released) {
        rulesRow = id;
        return;
    }

    switch (id) {
    case RULE_POINTS:
        if (value == -1 || value == 1) {
            playSound(SOUND_BEEP, sound);
            ruleChange(RULE_POINTS, value);
        }
        break;
    case RULE_WEAPONS:
        if (value >= 0 && value <= 2) {
            playSound(SOUND_BEEP, sound);
            weapons = value > 0;
            itemMode = value == 2;
        }
        break;
    case RULE_HOLES:
        playSound(SOUND_BEEP, sound);
        ruleChange(id, 1);
        break;
    case RULE_WALLS:
        if (value >= 0 && value < N_WALL_MODES) {
            playSound(SOUND_BEEP, sound);
            wallMode = value;
        }
        break;
    case RULE_SPEED:
        if (value >= 0 && value < N_SPEED_CHOICES) {
            playSound(SOUND_BEEP, sound);
            gameSpeed = speedChoices[value];
        }
        break;
    case RULE_GO:
        touchPressA();
        break;
    }
}

static void touchWep(int id, int value, bool released)
{
    (void)value;

    if (id == TOUCH_GO) {
        if (released)
            touchPressA();
        return;
    }
    /* A weapon: it goes to the player in turn, and the next human follows. */
    if (released && id >= 0 && id < N_WEAPONS) {
        players[wepCursor].weapon = id;
        int next = nextHuman(wepCursor, 1);
        if (next >= 0)
            wepCursor = next;
        playSound(SOUND_BEEP, sound);
    }
}

/* Going back from the lineup keeps what was chosen there; overwriting the file
 * in place is quick. */
static void saveLineup(void)
{
    saveSettings();
}

/* The pages that can be touched. */
static void initTouch(void)
{
    titleScene.touchFunc = touchTitle;
    titleScene.animFunc = animTitle;
    mainMenu.animFunc = animTitle;
    modeMenu.animFunc = animTitle;
    mainMenu.touchFunc = touchMainMenu;
    modeMenu.touchFunc = touchModeMenu;
    lobbyMenu.touchFunc = touchLobby;
    lobbyMenu.leaveFunc = saveLineup;
    tournamentInit(&modeMenu);
    slotMenusInit(&lobbyMenu);
    rulesMenu.touchFunc = touchRules;
    wepMenu.touchFunc = touchWep;
    pauseMenu.touchFunc = touchPause;
    settingsMenu.touchFunc = touchOptions;
    roundEndScene.touchFunc = touchRoundEnd;
    matchEndScene.touchFunc = touchMatchEnd;
}

/* A touch that began on a target: it counts when the finger is lifted on the
 * same one. */
static struct touchTarget touchHeld;
static bool touching = false;

static void touchPressButton(int button)
{
    if (button == N3DS_BUTTON_A) {
        touchPressA();
        return;
    }

    /* B and START are handled as events: send one. */
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = SDL_JOYBUTTONDOWN;
    ev.jbutton.which = 0;
    ev.jbutton.button = button;
    ev.jbutton.state = SDL_PRESSED;
    SDL_PushEvent(&ev);
}

/**
 * Handles a touch (the touchscreen arrives as mouse events). The coordinates
 * are those of the display: the bottom screen starts at (BOTTOM_VIEW_X, WINDOW_H).
 */
static void handleTouch(const SDL_Event *e)
{
    if (curScene->touchFunc == NULL) {
        touching = false;
        return;
    }

    int x = e->button.x - BOTTOM_VIEW_X;
    int y = e->button.y - WINDOW_H;

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        const struct touchTarget *t = touchFind(x, y);
        touching = t != NULL;
        if (t == NULL)
            return;
        touchHeld = *t;
        if (t->id != TOUCH_HINT) {
            curScene->touchFunc(t->id, t->value, false);
            curScene->displayFunc();
        }
        return;
    }

    if (!touching)
        return;
    touching = false;

    const struct touchTarget *t = touchFind(x, y);
    if (t == NULL || t->id != touchHeld.id || t->value != touchHeld.value)
        return;

    if (t->id == TOUCH_HINT) {
        touchPressButton(t->value);
    } else {
        curScene->touchFunc(t->id, t->value, true);
        curScene->displayFunc();
    }
}
#endif

#ifndef __3DS__
/**
 * Logic for the main menu.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicMainMenu(void)
{
    if (enterButtonDown()) {
        switch (menuMain.choice) {
        case 0:
            playSound(SOUND_BEEP, sound);
            initPlayers2();
            newRound();
            if (weapons) {
                assignAiWeapons();
                curScene = &wepMenu;
            } else {
                curScene = &gameStart;
            }
            break;
        case 1:
            playSound(SOUND_BEEP, sound);
            menuSettings.choice = 0;
            curScene = &settingsMenu;
            break;
        case 2:
            exitGame(0);
            break;
        default:
            break;
        }
        clearEnterButtons();
        return 1;
    } else if (menuButtonQuery(KEY_LEFT)) {
        if (menuMain.choice == 0 && nPlayers > MIN_PLAYERS) {
            --nPlayers;
            playSound(nPlayers - 1, sound);
            deselectWeapons();
        }
        return 1;
    } else if (menuButtonQuery(KEY_RIGHT)) {
        if (!duelmode && menuMain.choice == 0 && nPlayers < MAX_PLAYERS) {
            ++nPlayers;
            playSound(nPlayers - 1, sound);
        }
        return 1;
    }
    return handleMenu(&menuMain);
}
#endif

/**
 * Displays the main menu screen.
 */

#ifndef __3DS__
void displayMainMenu(void)
{
    char *c[menuMain.choices];
    c[0] = "START GAME";
    c[1] = "SETTINGS";
    c[2] = "EXIT";

    SDL_Surface *logo = images[IMG_LOGO];
    int ymod = logo->h / 2;
    displayMenu(c, &menuMain, ymod);
    SDL_Rect offset = {WINDOW_W / 2 - logo->w / 2, WINDOW_H / 2 - logo->h,
                       0, 0};
    SDL_BlitSurface(logo, NULL, screen, &offset);

    /* This could/should be made smoother... */
    for (int i = 0; i < nPlayers; ++i) {
        offset.x = (WINDOW_W / 2)            /* window offset */
            - 60                             /* temp. offset */
            + (i - nPlayers) * BALL_SPACING; /* player modifier */

        offset.y = (WINDOW_H / 2)            /* window offset */
            - BALL_Y_MOD + ymod;             /* temp. offset */

        SDL_BlitSurface(pballs[i], NULL, screen, &offset);
    }

    for (int i = nPlayers; i < MAX_PLAYERS; ++i) {
        offset.x = (WINDOW_W / 2)            /* window offset */
            + 68                             /* temp. offset */
            + (i - nPlayers) * BALL_SPACING; /* player modifier */

        offset.y = (WINDOW_H / 2)            /* window offset */
            - BALL_Y_MOD + ymod;             /* temp. offset */

        SDL_BlitSurface(pballs[MAX_PLAYERS], NULL, screen, &offset);
    }

    if (SDL_Flip(screen) == -1)
        exit(1);
}
#endif

#ifndef __3DS__
/**
 * Logic of the weapon selection menu.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicWepMenu(void)
{
    int i;
    struct player *p;

    if (enterButtonDown()) {
        resetWeapons();
        playSound(SOUND_ROUND_BEGIN, sound);
        curScene = &gameStart;
    }

    for (p = &players[0], i = 0; i < nPlayers; ++i, ++p) {
        if (buttonDown(p->lkey)) {
            if (p->weapon == 0) {
                p->weapon = N_WEAPONS - 1;
            }
            else {
                p->weapon--;
            }
            clearButton(p->lkey);
            return 1;
        }
        else if (buttonDown(p->rkey)) {
            if (p->weapon == N_WEAPONS - 1) {
                p->weapon = 0;
            }
            else {
                p->weapon++;
            }
            clearButton(p->rkey);
            return 1;
        }
    }
    return 0;
}
#endif


#ifndef __3DS__
/**
 * Displays the weapon selection menu.
 */
void displayWepMenu(void)
{
    useBottomView();
    struct player *p;
    int mid = -N_WEAPONS/2;
    clearBackground(screen);

    char wep_str[MENU_BUF];
    SDL_Surface *wep_text;

    /* Draw weapon description strings */
    for (int i = 0; i < nPlayers; ++i) {
        p = &players[i];
        struct weapon w = wep_list[p->weapon];

        SDL_Rect offset = {(i % 4) * (screen->w / 4) + 10,
                           i > 3 ? screen->h - 118 : 10, 0, 0};

        snprintf(wep_str, PLAYER_NAME_LEN, "%s", p->name);
        wep_text = TTF_RenderUTF8_Shaded(fonts[FONT_SCORE], wep_str,
                                         colors[p->color], cMenuBG);
        SDL_BlitSurface(wep_text, NULL, screen, &offset);
        SDL_FreeSurface(wep_text);

        offset.y += 30;
        snprintf(wep_str, MENU_BUF, "%s", w.name);
        wep_text = TTF_RenderUTF8_Shaded(fonts[FONT_SCORE], wep_str,
                                         colors[p->color], cMenuBG);
        SDL_BlitSurface(wep_text, NULL, screen, &offset);
        SDL_FreeSurface(wep_text);

        offset.y += 30;
        snprintf(wep_str, MENU_BUF, "%s", w.desc1);
        wep_text = TTF_RenderUTF8_Shaded(fonts[FONT_BROADCAST_BOLD], wep_str,
                                         colors[p->color], cMenuBG);
        SDL_BlitSurface(wep_text, NULL, screen, &offset);
        SDL_FreeSurface(wep_text);

        if (w.desc2[0] != '\0') {
            offset.y += FONT_SIZE_BROADCAST + 2;
            snprintf(wep_str, MENU_BUF, "%s", w.desc2);
            wep_text = TTF_RenderUTF8_Shaded(fonts[FONT_BROADCAST_BOLD],
                                             wep_str, colors[p->color],
                                             cMenuBG);
            SDL_BlitSurface(wep_text, NULL, screen, &offset);
            SDL_FreeSurface(wep_text);
        }

        if (i > 3)
            offset.y = screen->h - 10 - FONT_SIZE_BROADCAST;
        else
            offset.y = 105;
        snprintf(wep_str, MENU_BUF, "Charges: %d", w.charges);
        wep_text = TTF_RenderUTF8_Shaded(fonts[FONT_BROADCAST_BOLD], wep_str,
                                         colors[p->color], cMenuBG);
        SDL_BlitSurface(wep_text, NULL, screen, &offset);
        SDL_FreeSurface(wep_text);
    }

    /* Draw weapon icons */
    for (int i = 0; i < N_WEAPONS; ++i, ++mid) {
        SDL_Rect offset = {
            (screen->w / 2)          /* offset */
            - (wepIcons[0]->w / 2)  /* centralize */
            + (mid * ((wepIcons[0]->w / 2) + WEP_SPACEMOD))
            - ((N_WEAPONS % 2) - 1)
            * (WEP_SPACEMOD / 1.2)
            ,
            (screen->h / 2)          /* offset */
            - (wepIcons[0]->h / 2)  /* centralize */
            , 0, 0
        };

        SDL_BlitSurface(wepIcons[0], NULL, screen, &offset);
        SDL_BlitSurface(wepIcons[i + 1], NULL, screen, &offset);

        /* Draw weapon selection arrows */
        offset.y -= PARROWSELECT_MOD_Y;
        for (int j = 0; j < nPlayers; ++j) {
            if (j == MAX_PLAYERS / 2) {
                offset.y += wepIcons[0]->h + PARROWSELECT_MOD_Y * 1.2;
                offset.x -= PARROWSELECT_MOD_X * (MAX_PLAYERS / 2);
            }
            p = &players[j];
            if (j != 0) offset.x += PARROWSELECT_MOD_X;
            if (p->weapon == i) {
                int diri;
                if (j >= MAX_PLAYERS / 2) {
                    diri = (int)(3*M_PI_2 * (32 / (2*M_PI)));
                } else {
                    diri = (int)(M_PI_2 * (32 / (2*M_PI)));
                }
                SDL_BlitSurface(p->arrow, &arrowClip[diri], screen,
                                &offset);
            }
        }
    }

    if (presentFrame() == -1)
        exit(1);
}
#endif

#ifndef __3DS__
/**
 * Logic of the settings menu.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicSettingsMenu(void)
{
    if (enterButtonDown()) {
        switch (menuSettings.choice) {
        case 0: /* Toggle fullscreen */
            playSound(SOUND_BEEP, sound);
            fullscreen ^= 1;
            initScreen();
            initHitMap(WINDOW_W, WINDOW_H);
            break;
        case 1: /* Toggle sound */
            sound ^= 1;
            playSound(SOUND_BEEP, sound);
            break;
        case 2: /* Toggle music */
            music ^= 1;
            if (music)
                playBGM();
            else
                stopBGM();
            break;
        case 3: /* Toggle weapons */
            playSound(SOUND_BEEP, sound);
            weapons ^= 1;
            break;
        case 4: /* Toggle holes */
            playSound(SOUND_BEEP, sound);
            holes ^= 1;
            break;
        case 5: /* Toggle broadcasts */
            playSound(SOUND_BEEP, sound);
            broadcasts ^= 1;
            break;
        case 6: /* Toggle particle effects */
            playSound(SOUND_BEEP, sound);
            particleEffects ^= 1;
            break;
        case 7: /* Toggle border visibility */
            playSound(SOUND_BEEP, sound);
            border ^= 1;
            break;
        case 8: /* Toggle duel mode */
            playSound(SOUND_BEEP, sound);
            duelmode ^= 1;
            if (duelmode)
                nPlayers = 2;
            break;
        case 9: /* Score cap */
            playSound(SOUND_BEEP, sound);
            scorecap += 10;
            if (scorecap > SCORE_CAP_MAX) {
                scorecap = SCORE_CAP_MAX;
            }
            break;
        case 10:
            playSound(SOUND_BEEP, sound);
            menuPlayer.choice = 0;
            curScene = &playerMenu;
            break;
        case 11: /* Back */
            playSound(SOUND_BEP, sound);
            clearEnterButtons();
            initMainMenu();
            curScene = curScene->parentScene;
            break;
        default:
            break;
        }
        clearEnterButtons();
        return 1;
    }
    else if (menuButtonQuery(KEY_LEFT)) {
        if (menuSettings.choice == 9 && scorecap > 0) {
            playSound(SOUND_BEP, sound);
            --scorecap;
        }
        return 1;
    }
    else if (menuButtonQuery(KEY_RIGHT)) {
        if (menuSettings.choice == 9 && scorecap < SCORE_CAP_MAX) {
            playSound(SOUND_BEEP, sound);
            ++scorecap;
        }
        return 1;
    }
    else if (keyDown[SDLK_BACKSPACE]) {
        keyDown[SDLK_BACKSPACE] = 0;
        if (menuSettings.choice == 9) {
            playSound(SOUND_BEP, sound);
            scorecap = 0;
        }
        return 1;
    }

    /* Special case for the score setting */
    if (menuSettings.choice == 9) {
        int num = -1;
        if      (keyDown[SDLK_0]) num = 0;
        else if (keyDown[SDLK_1]) num = 1;
        else if (keyDown[SDLK_2]) num = 2;
        else if (keyDown[SDLK_3]) num = 3;
        else if (keyDown[SDLK_4]) num = 4;
        else if (keyDown[SDLK_5]) num = 5;
        else if (keyDown[SDLK_6]) num = 6;
        else if (keyDown[SDLK_7]) num = 7;
        else if (keyDown[SDLK_8]) num = 8;
        else if (keyDown[SDLK_9]) num = 9;

        if (num != -1) {
            scorecap = scorecap * 10 + num;
            if (scorecap > SCORE_CAP_MAX) {
                scorecap = SCORE_CAP_MAX;
            }
            playSound(SOUND_BEEP, sound);

            keyDown[SDLK_0] = keyDown[SDLK_1] = keyDown[SDLK_2] = keyDown[SDLK_3] =
                keyDown[SDLK_4] = keyDown[SDLK_5] = keyDown[SDLK_6] =
                keyDown[SDLK_7] = keyDown[SDLK_8] = keyDown[SDLK_9] = 0;

            return 1;
        }
    }

    return handleMenu(&menuSettings);
}
#endif

#ifndef __3DS__
/**
 * Display the settings menu screen.
 */
void displaySettingsMenu(void)
{
    useBottomView();
    char *c[menuSettings.choices];
    char tmpCap[SCORE_BUF];

    char s1[MENU_BUF] = "FULLSCREEN ";
#ifdef __3DS__
    /* Press Start 2P is wide: shorter texts fit the 320 pixels. */
    char s2[MENU_BUF] = "SOUND FX ";
#else
    char s2[MENU_BUF] = "SOUND EFFECTS ";
#endif
    char s3[MENU_BUF] = "MUSIC ";
    char s4[MENU_BUF] = "WEAPONS ";
    char s5[MENU_BUF] = "HOLES ";
    char s6[MENU_BUF] = "BROADCASTS ";
#ifdef __3DS__
    char s7[MENU_BUF] = "PARTICLES ";
#else
    char s7[MENU_BUF] = "PARTICLE EFFECTS ";
#endif
    char s8[MENU_BUF] = "SHOW BORDER ";
    char s9[MENU_BUF] = "DUEL MODE ";
    char s10[MENU_BUF] = "SCORE CAP: ";
    strncat(s1, fullscreen ON_OFF, MENU_BUF - strlen(s1) - 1);
    strncat(s2, sound ON_OFF, MENU_BUF - strlen(s2) - 1);
    strncat(s3, music ON_OFF, MENU_BUF - strlen(s3) - 1);
    strncat(s4, weapons ON_OFF, MENU_BUF - strlen(s4) - 1);
    strncat(s5, holes ON_OFF, MENU_BUF - strlen(s5) - 1);
    strncat(s6, broadcasts ON_OFF, MENU_BUF - strlen(s6) - 1);
    strncat(s7, particleEffects ON_OFF, MENU_BUF - strlen(s7) - 1);
    strncat(s8, border ON_OFF, MENU_BUF - strlen(s8) - 1);
    strncat(s9, duelmode ON_OFF, MENU_BUF - strlen(s9) - 1);
    if (scorecap == 0) {
        strncat(s10, "∞", MENU_BUF - strlen(s10) - 1);
    } else {
        snprintf(tmpCap, SCORE_BUF, "%d", scorecap);
        strncat(s10, tmpCap, MENU_BUF - strlen(s10) - 1);
    }
    c[0] = s1;
    c[1] = s2;
    c[2] = s3;
    c[3] = s4;
    c[4] = s5;
    c[5] = s6;
    c[6] = s7;
    c[7] = s8;
    c[8] = s9;
    c[9] = s10;
    c[10] = "PLAYER CONFIG";
    c[11] = "BACK";

    displayMenu(c, &menuSettings, 0);

    if (presentFrame() == -1)
        exit(1);
}
#endif

/**
 * Logic for the player menu.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicPlayerMenu(void)
{
    if (enterButtonDown()) {
        if (menuPlayer.choice == menuPlayer.choices - 1) {
            playSound(SOUND_BEP, sound);
            curScene = curScene->parentScene;
        } else {
            playSound(SOUND_BEEP, sound);
            editPlayer = menuPlayer.choice;
            menuPConf.choice = 0;
            curScene = &pConfMenu;
        }
        clearEnterButtons();
        return 1;
    }
    return handleMenu(&menuPlayer);
}

/**
 * Display the player menu screen.
 */
void displayPlayerMenu(void)
{
    useBottomView();
    char *c[menuPlayer.choices];
    c[0] = "PLAYER 1 CONFIG";
    c[1] = "PLAYER 2 CONFIG";
    c[2] = "PLAYER 3 CONFIG";
    c[3] = "PLAYER 4 CONFIG";
    c[4] = "PLAYER 5 CONFIG";
    c[5] = "PLAYER 6 CONFIG";
    c[6] = "PLAYER 7 CONFIG";
    c[7] = "PLAYER 8 CONFIG";
    c[8] = "BACK";

    displayMenu(c, &menuPlayer, 0);

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Logic for the player configuration.
 *
 * @return 1 if something has changed, and needs to be redrawn, 0 else.
 */
int logicPConfMenu(void)
{
    if (enterButtonDown()) {
        switch (menuPConf.choice) {
        case 0:
#ifdef __3DS__
            /* The on-screen keyboard is operated with the joypad. */
            playSound(SOUND_BEEP, sound);
            setNextName(editPlayer);
#else
            // Disable name input with joystick for the time being.
            if (keyDown[SDLK_SPACE] || keyDown[SDLK_RETURN]) {
                playSound(SOUND_BEEP, sound);
                setNextName(editPlayer);
            }
#endif
            break;
        case 1: /* Toggle AI */
            playSound(SOUND_BEEP, sound);
            (&players[editPlayer])->ai ^= 1;
            break;
        case 2:
            playSound(SOUND_BEEP, sound);
            (&players[editPlayer])->lkey = SDLK_CLEAR;
            displayPConfMenu(); /* Update menu before catching key */
            setNextKey(editPlayer, 'l');
            break;
        case 3:
            playSound(SOUND_BEEP, sound);
            (&players[editPlayer])->wkey = SDLK_CLEAR;
            displayPConfMenu(); /* Update menu before catching key */
            setNextKey(editPlayer, 'w');
            break;
        case 4:
            playSound(SOUND_BEEP, sound);
            (&players[editPlayer])->rkey = SDLK_CLEAR;
            displayPConfMenu(); /* Update menu before catching key */
            setNextKey(editPlayer, 'r');
            break;
        case 5: /* Reset player settings */
            playSound(SOUND_BEP, sound);
            resetPlayer(editPlayer);
            break;
        case 6:
            playSound(SOUND_BEP, sound);
            curScene = curScene->parentScene;
            break;
        default:
            break;
        }
        clearEnterButtons();
        return 1;
    } else if (menuPConf.choice == 0) {
        if (menuButtonQuery(KEY_LEFT)) {
            playSound(SOUND_BEEP, sound);
            setColor(editPlayer, 0);
            return 1;
        } else if (menuButtonQuery(KEY_RIGHT)) {
            playSound(SOUND_BEEP, sound);
            setColor(editPlayer, 1);
            return 1;
        } else if (keyDown[SDLK_BACKSPACE]) {
            keyDown[SDLK_BACKSPACE] = 0;
            playSound(SOUND_BEEP, sound);
            setNextName(editPlayer);
            return 1;
        }
    }
    return handleMenu(&menuPConf);
}

/**
 * Display the player configuration screen.
 */
void displayPConfMenu(void)
{
    useBottomView();
    char *c[menuPConf.choices];

    char s1[MENU_BUF] = "";
    char s2[MENU_BUF] = "AI ";
    char s3[MENU_BUF] = "LEFT KEY: ";
    char s4[MENU_BUF] = "WEAPON KEY: ";
    char s5[MENU_BUF] = "RIGHT KEY: ";

    char tmpKey[BUTTON_NAME_MAX_LEN];
    struct player *p = (&players[editPlayer]);
    char *lkey = buttonName(p->lkey);
    char *rkey = buttonName(p->rkey);
    char *wkey = buttonName(p->wkey);
    snprintf(s1, MENU_BUF, "%s",
             p->name[0] == '\0' ? "< enter name >" : p->name);

    strncat(s2, p->ai ON_OFF, MENU_BUF - strlen(s2) - 1);

    snprintf(tmpKey, BUTTON_NAME_MAX_LEN*sizeof(char), "%s", lkey);
    free(lkey);
    strncat(s3, tmpKey, MENU_BUF - strlen(s3) - 1);

    snprintf(tmpKey, BUTTON_NAME_MAX_LEN*sizeof(char), "%s", wkey);
    free(wkey);
    strncat(s4, tmpKey, MENU_BUF - strlen(s4) - 1);

    snprintf(tmpKey, BUTTON_NAME_MAX_LEN*sizeof(char), "%s", rkey);
    free(rkey);
    strncat(s5, tmpKey, MENU_BUF - strlen(s5) - 1);

    c[0] = s1;
    c[1] = s2;
    c[2] = s3;
    c[3] = s4;
    c[4] = s5;
    c[5] = "RESET";
    c[6] = "BACK";

    displayMenu(c, &menuPConf, 0);

    if (presentFrame() == -1)
        exit(1);
}

/**
 * Handles a general menu.
 *
 * @param m The menu to do handling of.
 */
int handleMenu(struct menu *m)
{
    if (menuButtonQuery(KEY_DOWN)) {
        playSound(SOUND_BEEP, sound);
        ++(m->choice);
        return 1;
    }
    else if (menuButtonQuery(KEY_UP)) {
        playSound(SOUND_BEEP, sound);
        --(m->choice);
        return 1;
    }

    if (m->choice >= m->choices) {
        m->choice = 0;
        return 1;
    }
    else if (m->choice < 0) {
        m->choice = m->choices - 1;
        return 1;
    }

    return 0;
}

#ifdef __3DS__
/**
 * Draws small arrows at the top and bottom edge of the screen to show that a
 * scrolling menu has more entries in that direction.
 */
static void drawScrollArrows(bool more_above, bool more_below)
{
    Uint32 color = SDL_MapRGB(screen->format, cMenuText.r, cMenuText.g,
                              cMenuText.b);
    int cx = screen->w / 2;

    for (int row = 0; row < 5; ++row) {
        SDL_Rect up = {cx - row, 4 + row, 2 * row + 1, 1};
        SDL_Rect down = {cx - row, screen->h - 5 - row, 2 * row + 1, 1};

        if (more_above)
            SDL_FillRect(screen, &up, color);
        if (more_below)
            SDL_FillRect(screen, &down, color);
    }
}
#endif

/**
 * Displays a general menu.
 * The menu structure is separated from the strings, so that arbitrary
 * strings can be used.
 *
 * @param c All the strings to be shown in the menu.
 * @param m The menu to be shown.
 * @param ymod Amount of y-coordinate shifting.
 */
void displayMenu(char *c[], struct menu *m, int ymod)
{
    /* Menus with more than MENU_MAX_VISIBLE entries scroll: show a window of
     * entries that follows the selection. */
    int visible = m->choices < MENU_MAX_VISIBLE ? m->choices : MENU_MAX_VISIBLE;
    int first = m->choice - visible / 2;
    if (first > m->choices - visible)
        first = m->choices - visible;
    if (first < 0)
        first = 0;

    int mid = -(visible / 2);
    clearBackground(screen);

    for (int i = first; i < first + visible; ++i, ++mid) {

        if (i == m->choice) {
            msg = TTF_RenderUTF8_Shaded(fonts[FONT_MENU_BOLD], c[i],
                                        i == 0 && curScene == &pConfMenu
                                        ? colors[(&players[editPlayer])->color]
                                        : cMenuTextH, cMenuBG);
        } else {
            msg = TTF_RenderUTF8_Solid(fonts[FONT_MENU], c[i],
                                       i == 0 && curScene == &pConfMenu
                                       ? colors[(&players[editPlayer])->color]
                                       : cMenuText);
        }

        SDL_Rect offset = {
            (screen->w / 2)                     /* offset */
            - (msg->w / 2)                      /* centralize */
            ,
            (screen->h / 2)                     /* offset */
            + (mid * ((msg->h / 2) + SPACEMOD)) /* spacing */
            - ((visible % 2) - 1)               /* halfspace */
            * (SPACEMOD / 2)
            - (msg->h / 2)                      /* centralize */
            + ymod                              /* eventual offset */
            , 0, 0
        };

        SDL_BlitSurface(msg, NULL, screen, &offset);
        SDL_FreeSurface(msg);
    }

#ifdef __3DS__
    drawScrollArrows(first > 0, first + visible < m->choices);
#endif
}

/**
 * Puts a pixel on a surface.
 *
 * @param x x coordinate of the pixel destination.
 * @param y y coordinate of the pixel destination.
 * @param c Desired color of the pixel.
 * @param target Points to which SDL_Surface the pixel should be put.
 */
void putPixel(int x, int y, SDL_Color c, SDL_Surface *target)
{
    /* Let SDL arrange the color channels, they differ between platforms. */
    Uint32 *pixel = (Uint32 *)target->pixels + (WINDOW_W * y) + x;
    *pixel = SDL_MapRGB(target->format, c.r, c.g, c.b);
}

/**
 * Fills a surface with a color.
 * All white pixels in the surface will be replaced with the specified
 * color.
 *
 * @param c Color to fill the surface with.
 * @param sprite Surface to be filled.
 */
void colorFill(SDL_Color c, SDL_Surface *sprite)
{
    Uint32 *target = sprite->pixels;

    for (int yy = 0; yy < sprite->h; ++yy) {
        for (int xx = 0; xx < sprite->w; ++xx, ++target) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(*target, sprite->format, &r, &g, &b, &a);
            *target = SDL_MapRGBA(sprite->format, r * (c.r / 255.0),
                                  g * (c.g / 255.0), b * (c.b / 255.0), a);
        }
    }
}

/**
 * Colors the balls for the main menu.
 */
void colorBalls(void)
{
    SDL_Color inactive = {127, 127, 127, 0};
    SDL_Surface **s = pballs;
    struct player *p = &players[0];
    for (int i = 0; i < MAX_PLAYERS; ++i, ++p, ++s) {
        copySprite(images[IMG_BALL], *s);
        SDL_LockSurface(*s);
        colorFill(colors[p->color], *s);
        SDL_UnlockSurface(*s);
    }

    copySprite(images[IMG_BALL], *s);
    SDL_LockSurface(*s);
    colorFill(inactive, *s);
    SDL_UnlockSurface(*s);
}

/**
 * Initializes SDL.
 *
 * @return 1 if the initialization was successful, 0 if not.
 */
int init(void)
{
#ifdef __3DS__
    /* Audio is opened (and allowed to fail) by initSound(). */
    const Uint32 sdlFlags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER;
#else
    const Uint32 sdlFlags = SDL_INIT_EVERYTHING;
#endif

    if (SDL_Init(sdlFlags) == -1) {
        if (olvl >= O_NORMAL)
            fprintf(stderr, "ERROR: Could not initialize SDL.\n");
        return 0;
    }

    if (atexit(SDL_Quit) != 0)
        return 0;

    openJoysticks();
    atexit(closeJoysticks);

    if (initSound() == -1) {
        if (olvl >= O_NORMAL)
            fprintf(stderr, "ERROR: Could not initialize sound.\n");
        return 0;
    }

    if (TTF_Init() == -1) {
        if (olvl >= O_NORMAL)
            fprintf(stderr, "ERROR: Could not initialize fonts.\n");
        return 0;
    }
    atexit(TTF_Quit);

    cleanBroadcasts();
    srand(time(NULL) ^ SDL_GetTicks());

    SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_SetCaption("Zatacka X", "Zatacka X");
    SDL_Surface *icon = loadIcon("icon.bmp");
    SDL_WM_SetIcon(icon, NULL);
    SDL_FreeSurface(icon);

    return 1;
}

/**
 * Make miscellaneous graphics preparations, such as sprite clipping
 * and duplication.
 */
void initGraphics(void)
{
    SDL_Surface **p;

    /* Clip arrow sprite sheet */
    SDL_Surface *arrows = images[IMG_ARROWS];
    for (int y = 0, i = 0; y < arrows->h; y += 16) {
        for (int x = 0; x < arrows->w; x += 16, ++i) {
            arrowClip[i].x = x;
            arrowClip[i].y = y;
            arrowClip[i].w = 16;
            arrowClip[i].h = 16;
        }
    }

    /* Make arrow copies */
    parrows = malloc(MAX_PLAYERS * sizeof(SDL_Surface *));
    p = parrows;
    for (int i = 0; i < MAX_PLAYERS; ++i, ++p) {
        *p = SDL_CreateRGBSurface(arrows->flags, arrows->w,
                                  arrows->h, arrows->format->BitsPerPixel,
                                  arrows->format->Rmask,
                                  arrows->format->Gmask,
                                  arrows->format->Bmask, SPRITE_AMASK(arrows));
    }

    /* Make ball copies */
    pballs = malloc((MAX_PLAYERS + 1) * sizeof(SDL_Surface *));
    p = pballs;
    SDL_Surface *ball = images[IMG_BALL];
    for (int i = 0; i < MAX_PLAYERS + 1; ++i, ++p) {
        *p = SDL_CreateRGBSurface(ball->flags, ball->w,
                                  ball->h, ball->format->BitsPerPixel,
                                  ball->format->Rmask, ball->format->Gmask,
                                  ball->format->Bmask, SPRITE_AMASK(ball));
    }

    /* Initialize weapon pointer array */
    wepIcons[0] = images[IMG_WI_BG];
    wepIcons[WEP_LIGHTNINGSPEED + 1] = images[IMG_WI_SPEED];
    wepIcons[WEP_FROSTWAVE + 1] = images[IMG_WI_FROST];
    wepIcons[WEP_CONFUSION + 1] = images[IMG_WI_CONFUSION];
    wepIcons[WEP_SHARPTURN + 1] = images[IMG_WI_SHARPTURN];
    wepIcons[WEP_TIMESTEP + 1] = images[IMG_WI_TIMESTEP];
    wepIcons[WEP_MOLE + 1] = images[IMG_WI_MOLE];
    wepIcons[WEP_WARP + 1] = images[IMG_WI_WARP];
    wepIcons[WEP_GHOST + 1] = images[IMG_WI_GHOST];
    wepIcons[WEP_TRON + 1] = images[IMG_WI_TRON];
    wepIcons[WEP_CHILI_RUN + 1] = images[IMG_WI_CHILIRUN];
    wepIcons[WEP_ROCKETLAUNCHER + 1] = images[IMG_WI_ROCKETLAUNCHER];

    smallWepIcons[WEP_LIGHTNINGSPEED] = images[IMG_WIS_SPEED];
    smallWepIcons[WEP_FROSTWAVE] = images[IMG_WIS_FROST];
    smallWepIcons[WEP_CONFUSION] = images[IMG_WIS_CONFUSION];
    smallWepIcons[WEP_SHARPTURN] = images[IMG_WIS_SHARPTURN];
    smallWepIcons[WEP_TIMESTEP] = images[IMG_WIS_TIMESTEP];
    smallWepIcons[WEP_MOLE] = images[IMG_WIS_MOLE];
    smallWepIcons[WEP_WARP] = images[IMG_WIS_WARP];
    smallWepIcons[WEP_GHOST] = images[IMG_WIS_GHOST];
    smallWepIcons[WEP_TRON] = images[IMG_WIS_TRON];
    smallWepIcons[WEP_CHILI_RUN] = images[IMG_WIS_CHILIRUN];
    smallWepIcons[WEP_ROCKETLAUNCHER] = images[IMG_WIS_ROCKETLAUNCHER];
}

/**
 * Exits the game.
 *
 * @param status Exit status.
 */
void exitGame(int status)
{
    free(hitmap);
    free(parrows);
    free(pballs);

#ifdef __3DS__
    /* Files in romfs must be closed before platformExit() unmounts it, or the
     * clean-up of the C library crashes at exit. */
    fprintf(stderr, "exit: start        t=%u\n", platformMillis());
    closeSounds();
    fprintf(stderr, "exit: sounds closed t=%u\n", platformMillis());
    closeFonts();
    fprintf(stderr, "exit: fonts closed  t=%u\n", platformMillis());
#endif

#ifndef __3DS__
    screen = SDL_SetVideoMode(WINDOW_W, WINDOW_H, SCREEN_BPP,
                              SDL_SWSURFACE);
#endif
#ifdef __3DS__
    platformTrace("exit: closed fonts");
#endif
#ifdef __3DS__
    /* Settings are saved when the settings menu is left (writing takes long). */
    fprintf(stderr, "exit: done          t=%u\n", platformMillis());
#else
    saveSettings();
#endif

    exit(status);
}

int main(void)
{
    static Uint32 prevtime = 0;

#ifdef __3DS__
    if (!platformInit())
        return 1;
    atexit(platformExit);
#endif

#ifdef __3DS__
    unsigned int t0 = platformMillis(), tlast = t0;
#define LOG_LOAD_TIME(what) do { \
        unsigned int now = platformMillis(); \
        fprintf(stderr, "load: %-8s %5u ms\n", what, now - tlast); \
        tlast = now; \
    } while (0)
#else
#define LOG_LOAD_TIME(what) ((void)0)
#endif

    initPlayers1();
    restoreSettings();
#ifdef __3DS__
    if (debugLog)
        platformOpenLog();
#endif

    if (!init())
        return 1;
    LOG_LOAD_TIME("init");

    if (!initScreen())
        return 1;
    LOG_LOAD_TIME("screen");

    initHitMap(WINDOW_W, WINDOW_H);

    if (!loadImages()) {
        if (olvl >= O_NORMAL) {
            fprintf(stderr, "ERROR: Failed to load image files.\n");
        }
        return 1;
    }

    LOG_LOAD_TIME("images");

    if (!loadSounds()) {
        if (olvl >= O_NORMAL) {
            fprintf(stderr, "ERROR: Failed to load sound files.\n");
        }
        return 1;
    }
    LOG_LOAD_TIME("sounds");

    if (!loadFonts()) {
        if (olvl >= O_NORMAL) {
            fprintf(stderr, "ERROR: Failed to load font files.\n");
        }
        return 1;
    }

    LOG_LOAD_TIME("fonts");

    initGraphics();
    initColors();
    initMainMenu();
    LOG_LOAD_TIME("rest");
#ifdef __3DS__
    platformTraceDump();
#endif

#ifdef __3DS__
    initTouch();
    demoReset();
    curScene = &titleScene;
#else
    curScene = &mainMenu;
#endif
    curScene->displayFunc();

    if (music)
        playBGM();

    for (;;) {
#ifdef __3DS__
        /* The lid was closed, or the game went to the HOME menu: pause. */
        if (platformWasSuspended()) {
            if (netGameRole() != NET_ROLE_NONE)
                ;       /* No pause with several consoles */
            else if (curScene == &game || curScene == &gameStart)
                pauseGame();
            else if (curScene == &resumeScene)
                resumeLeft = 3 * RESUME_STEP;
        }
#endif
        while (SDL_PollEvent(&event)) {
#ifdef __3DS__
            /* The touchscreen: handled on its own, not as a mouse. */
            if (event.type == SDL_MOUSEBUTTONDOWN
                    || event.type == SDL_MOUSEBUTTONUP) {
                handleTouch(&event);
                continue;
            }
            if (!translateEvent3ds(&event))
                continue;
#endif
            if (event.type == SDL_KEYDOWN
                    || event.type == SDL_MOUSEBUTTONDOWN
                    || event.type == SDL_JOYBUTTONDOWN
                    || event.type == SDL_JOYAXISMOTION) {

                int k = -1;
                if (event.type == SDL_KEYDOWN) {
                    k = event.key.keysym.sym;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    k = event.button.button;
                }
                else if (event.type == SDL_JOYBUTTONDOWN) {
                    k = event.button.button;
                }
                else if (event.type == SDL_JOYAXISMOTION) {
                    k = axisNumber(event.jaxis) << 4;
                }

#ifdef __3DS__
                /* START pauses. B (escape) doesn't end a round any more; in
                 * the pause menu it means "resume". */
                bool inGame = curScene == &game || curScene == &gameStart;
                if (event.type == SDL_JOYBUTTONDOWN
                        && k == N3DS_BUTTON_START) {
                    if (netGameRole() != NET_ROLE_NONE) {
                        continue;       /* No pause with several consoles */
                    } else if (inGame) {
                        pauseGame();
                        continue;
                    } else if (curScene == &pauseMenu) {
                        resumeGame();
                        continue;
                    } else if (curScene == &lobbyMenu) {
                        lobbyStart();
                        curScene->displayFunc();    /* The page may have changed */
                        continue;
                    } else if (curScene == &rulesMenu) {
                        rulesStart();
                        curScene->displayFunc();
                        continue;
                    } else if (curScene == &wepMenu) {
                        wepStart();
                        continue;
                    } else if (curScene == &titleScene) {
                        curScene = &mainMenu;
                        curScene->displayFunc();
                        continue;
                    }
                }
                if (event.type == SDL_KEYDOWN && k == SDLK_ESCAPE) {
                    if (inGame)
                        continue;
                    if (curScene == &pauseMenu) {
                        resumeGame();
                        continue;
                    }
                    /* After a match with several consoles the host can send
                     * everybody back to the lobby. */
                    if (curScene == &matchEndScene
                            && netGameRole() == NET_ROLE_HOST) {
                        netGameToLobby();
                        curScene->displayFunc();
                        continue;
                    }
                    /* The only way out of the game is the EXIT tile. */
                    if (curScene == &mainMenu) {
                        curScene = &titleScene;
                        curScene->displayFunc();
                        continue;
                    }
                    if (curScene == &titleScene || curScene == &roundEndScene
                            || curScene == &matchEndScene
                            || curScene == &resumeScene)
                        continue;
                }
#endif

                if (screenFreeze && k == SDLK_RETURN) {
                    screenFreeze = false;
                    endRound();
                    curScene = &mainMenu;
                    curScene->displayFunc();
                }

                if ((event.type == SDL_KEYDOWN && k == SDLK_ESCAPE)) {
                    screenFreeze = false;
                    if (curScene == &game || curScene == &gameStart)
                        endRound();
                    else if (curScene == &settingsMenu) {
                        initMainMenu();
#ifdef __3DS__
                        /* A 3DS game is often closed from the HOME menu, so
                         * don't wait for exitGame() to save. */
                        saveSettings();
                        platformTraceDump();
#endif
                    }
                    else if (curScene->parentScene == NULL)
                        exitGame(0);
                    playSound(SOUND_BEP, sound);
                    if (curScene->leaveFunc != NULL)
                        curScene->leaveFunc();
                    curScene = curScene->parentScene;
                    curScene->displayFunc();
                }
                else if (event.type == SDL_JOYBUTTONDOWN) {
                    if (olvl >= O_DEBUG) {
                        fprintf(stderr, "Pressed joy button: %d\n", k);
                    }
                    joyButtonDown[event.button.which][k] = true;
                }
                else if (event.type == SDL_JOYAXISMOTION) {
                    Uint8 jid = event.jaxis.which;

                    if (event.jaxis.value) {
                        if (olvl >= O_DEBUG) {
                            fprintf(stderr, "Joy axis changed: %d\n", k);
                        }
                        joyButtonDown[jid][k] = true;
                    }
                    else {
                        if (event.jaxis.axis == JOY_DIR_UP
                                || event.jaxis.axis == JOY_DIR_DOWN) {
                            joyButtonDown[jid][JOY_DIR_UP << 4] = false;
                            joyButtonDown[jid][JOY_DIR_DOWN << 4] = false;
                        }
                        else {
                            joyButtonDown[jid][JOY_DIR_RIGHT << 4] = false;
                            joyButtonDown[jid][JOY_DIR_LEFT << 4] = false;
                        }
                    }
                }
                else {
                    if (olvl >= O_DEBUG) {
                        fprintf(stderr, "Pressed: %c (%d)\n", k, k);
                    }
                    keyDown[k] = true;
                }
            }
            else if (event.type == SDL_KEYUP) {
                keyDown[event.key.keysym.sym] = false;
            }
            else if (event.type == SDL_MOUSEBUTTONUP) {
                keyDown[event.button.button] = false;
            }
            else if (event.type == SDL_JOYBUTTONUP) {
                joyButtonDown[event.button.which][event.button.button] = false;
            }
            else if (event.type == SDL_QUIT) {
                exitGame(0);
            }
            else if (event.type == SDL_VIDEORESIZE) {
                WINDOW_W = event.resize.w;
                WINDOW_H = event.resize.h;

                if (!initScreen()) {
                    return 1;
                }

                initHitMap(WINDOW_W, WINDOW_H);
                refreshGameScreen();
            }
        }
        if (!screenFreeze) {
            Uint32 timenow = SDL_GetTicks();
            Uint32 delta = timenow - prevtime;
            prevtime = timenow;

            if (delta < 1000/FPS_CAP) {
                SDL_Delay(1000/FPS_CAP - delta);
            }

#ifdef __3DS__
            /* Once per two seconds of play: how long logic and display take. */
            static unsigned int statStart, statFrames, statLogic, statDisplay;
            unsigned int s0 = platformMillis();
            int changed = curScene->logicFunc();
            unsigned int s1 = platformMillis();
            if (changed)
                curScene->displayFunc();
            else if (curScene->animFunc != NULL)
                curScene->animFunc();
            unsigned int s2 = platformMillis();

            if (curScene == &game) {
                statLogic += s1 - s0;
                statDisplay += s2 - s1;
                ++statFrames;
                if (s2 - statStart >= 2000) {
                    if (statFrames > 0)
                        fprintf(stderr, "game: %u fps, logic %u ms, "
                                "display %u ms per frame\n",
                                statFrames * 1000 / (s2 - statStart),
                                statLogic / statFrames,
                                statDisplay / statFrames);
                    statStart = s2;
                    statFrames = statLogic = statDisplay = 0;
                }
            }
#else
            if (curScene->logicFunc()) {
                curScene->displayFunc();
            }
#endif
        }
    }

    return 0;
}
