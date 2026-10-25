/* platform -- 3DS specific setup, kept apart from the game code.
 *
 * libctru's <3ds.h> defines names (KEY_UP, ...) that clash with the game's
 * own, so only platform.c may include it.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>

/* Initialize 3DS services (romfs). Returns false on failure. */
bool platformInit(void);

/*
 * Start logging stderr to sdmc:/zatackax.log (unbuffered). The first write to a
 * new file on the SD card can take about ten seconds, so this is only done when
 * the setting "debug = 1" in settings.cfg asks for it.
 */
void platformOpenLog(void);

/* Counterpart of platformInit(), suitable for atexit(). */
void platformExit(void);

/*
 * Record a time stamp with a label in memory (cheap, no file access), and write
 * the records to the log with platformTraceDump(). For finding slow spots.
 */
void platformTrace(const char *label);
void platformTraceDump(void);

/*
 * True once after the console was put to sleep (lid closed) or the game was
 * sent to the HOME menu. Call it every frame to pause the game then.
 */
bool platformWasSuspended(void);

/* Milliseconds since an arbitrary start, usable before SDL is initialized. */
unsigned int platformMillis(void);

/*
 * Ask for a text with the system's on-screen keyboard. `text` is the initial
 * text and receives the result, which is at most `size` - 1 bytes long. Returns
 * true if the user confirmed, in which case `text` has been changed.
 */
bool platformTextInput(char *text, size_t size, const char *hint);

/*
 * The Play Coins of the system (gamecoin.dat in the shared extdata of the
 * pedometer service). Reading is harmless. Spending writes two bytes of a
 * system file that the HOME menu needs at startup, so it checks the file first
 * and reads the value back afterwards.
 *
 * platformCoinsRead() returns false if the file cannot be opened or does not
 * look as expected (an emulator without the file, missing rights).
 * platformCoinsSpend() returns true only if the coins were taken and the new
 * value was read back.
 */
bool platformCoinsRead(unsigned int *coins);
bool platformCoinsSpend(unsigned int amount);

#endif
