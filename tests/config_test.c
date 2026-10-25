/* Saves the settings and reads them back, names with spaces included.
 * Run: make -C tests (needs the SDL 1.2 headers only). */
#include <assert.h>
#include <stdlib.h>

#include "../src/config.c"

struct player players[MAX_PLAYERS];
unsigned char nPlayers;

#ifdef __3DS__
unsigned char slotMode[MAX_PLAYERS];
struct player *slotConfig(int i) { return &players[i]; }
void platformTrace(const char *label) { (void)label; }
int netGameRole(void) { return 0; }
int netGameMySeat(void) { return -1; }
#include "../src/stats3ds.c"
#endif

int main(void)
{
    char dir[] = "/tmp/zatackax-test-XXXXXX";
    assert(mkdtemp(dir) != NULL);
    setenv("XDG_CONFIG_HOME", dir, 1);

    snprintf(players[0].name, sizeof(players[0].name), "Big Bob");
    snprintf(players[1].name, sizeof(players[1].name), "Ann");
    players[1].color = 5;
    assert(saveSettings());

    memset(players, 0, sizeof(players));
    restoreSettings();
    assert(strcmp(players[0].name, "Big Bob") == 0);
    assert(strcmp(players[1].name, "Ann") == 0);
    assert(players[1].color == 5);

#ifdef __3DS__
    /* What the shop keeps: 53 items need more than 32 bits. */
    shopOwned = (1ull << 50) | 5;
    shopTheme = 3;
    shopFrame = 22;
    shopBlast = 12;
    players[0].headStyle = 3;
    players[2].color = 17;
    stats = (struct stats){4, 2, 30, 12, 5};
    assert(saveSettings());

    shopOwned = 0;
    shopTheme = shopFrame = shopBlast = -1;
    memset(&stats, 0, sizeof(stats));
    memset(players, 0, sizeof(players));
    restoreSettings();
    assert(shopOwned == ((1ull << 50) | 5));
    assert(shopTheme == 3 && shopFrame == 22 && shopBlast == 12);
    assert(players[0].headStyle == 3);
    assert(players[2].color == 17);
    assert(stats.matches == 4 && stats.roundWins == 12 && stats.bestStreak == 5);
#endif

    /* Bad lines must not write outside: a long word, and slots 0 and 9. */
    char *file = getConfigFile();
    FILE *f = fopen(file, "w");
    fprintf(f, "%0100d = 1\n0n = x\n9n = x\n1n = %0100d\n", 7, 7);
    fclose(f);
    free(file);
    restoreSettings();
    assert(strlen(players[0].name) < PLAYER_NAME_LEN);

    puts("ok");
    return 0;
}
