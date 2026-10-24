/* sound -- sound handling.
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

#include "sound.h"

Mix_Music *bgm = NULL;
Mix_Chunk *sounds[N_SOUNDS];

/* False if the mixer couldn't be opened and the game runs silently. */
static bool soundAvailable = true;

/**
 * Initialize the mixer. Return -1 on failure, 0 else.
 */
int initSound(void)
{
#ifdef __3DS__
    /* A small buffer runs dry when the game is busy: sound crackles. */
    const int buffer = 2048;
#else
    const int buffer = 512;
#endif
#ifdef __3DS__
    /* The sounds are converted to 22.05 kHz (3ds/make-romfs.sh), so there is no
     * need to mix at 44.1 kHz. */
    const int frequency = MIX_DEFAULT_FREQUENCY;
#else
    const int frequency = MIX_DEFAULT_FREQUENCY * 2;
#endif
    int status = Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, buffer);
#ifdef __3DS__
    /* No audio without the DSP firmware (sdmc:/3ds/dspfirm.cdc): go silent
     * instead of refusing to start. */
    if (status == -1) {
        fprintf(stderr, "No sound: %s\n", Mix_GetError());
        soundAvailable = false;
        return 0;
    }
#endif
    return status;
}

/**
 * Load a sound file `filename` and identify it as `s`. Return true on
 * success, otherwise false.
 */
bool loadSound(Sound s, const char *filename)
{
    char *file = dataFile("sound", filename);
    Mix_Chunk *sound = Mix_LoadWAV(file);

    if (sound == NULL) {
        fileNotFound(file);
        free(file);
        return false;
    }

    sounds[s] = sound;
    free(file);
    return true;
}

/**
 * Load the background music from `filename`. Return true on success,
 * otherwise false.
 */
bool loadBGM(const char *filename)
{
    char *file = dataFile("sound", filename);
    bgm = Mix_LoadMUS(file);

    if (bgm == NULL) {
        fileNotFound(file);
        free(file);
        return false;
    }

    free(file);
    return true;
}

/**
 * Load every needed sound file. Return true if every file loaded
 * successfully, otherwise false.
 */
bool loadSounds(void)
{
    if (!soundAvailable)
        return true;

    return loadBGM("theme1.ogg")
        && loadSound(SOUND_BEP, "bep.ogg")
        && loadSound(SOUND_BEEP, "beep.ogg")
        && loadSound(SOUND_BEEEP, "beeep.ogg")
        && loadSound(SOUND_BEEEEP, "beeeep.ogg")
        && loadSound(SOUND_BEEEEEP, "beeeeep.ogg")
        && loadSound(SOUND_BEEEEEEP, "beeeeeep.ogg")
        && loadSound(SOUND_BEEEEEEEP, "beeeeeeep.ogg")
        && loadSound(SOUND_BEEEEEEEEP, "beeeeeeeep.ogg")
        && loadSound(SOUND_ROUND_BEGIN, "round_begin.ogg")
        && loadSound(SOUND_CRASH, "crash.ogg")
        && loadSound(SOUND_EXPLOSION, "explosion.ogg")
        && loadSound(SOUND_SPEED, "speed.ogg")
        && loadSound(SOUND_FREEZE, "freeze.ogg")
        && loadSound(SOUND_CONFUSION, "confusion.ogg")
        && loadSound(SOUND_SHARPTURN, "sharpturn.ogg")
        && loadSound(SOUND_TIMESTEP, "timestep.ogg")
        && loadSound(SOUND_MOLE, "mole.ogg")
        && loadSound(SOUND_WARP, "warp.ogg")
        && loadSound(SOUND_GHOST, "ghost.ogg")
        && loadSound(SOUND_TRON, "tronmode.ogg")
        && loadSound(SOUND_CHILIRUN, "chilirun.ogg")
        && loadSound(SOUND_ROCKETLAUNCHER, "rocketlauncher.ogg");
}

/**
 * Free every sound and close the mixer. Needed on the 3DS before the romfs is
 * unmounted: the music stays open as a file.
 */
void closeSounds(void)
{
    if (!soundAvailable)
        return;

    Mix_HaltMusic();
    Mix_FreeMusic(bgm);
    bgm = NULL;

    for (int i = 0; i < N_SOUNDS; ++i) {
        Mix_FreeChunk(sounds[i]);
        sounds[i] = NULL;
    }

    Mix_CloseAudio();
    soundAvailable = false;
}

/**
 * Plays a sound.
 *
 * @param sound One of the sounds stated in enum sounds (see sound.h).
 * @param play 1 if the sound is to be played, 0 else.
 */
void playSound(unsigned int sound, int play)
{
    if (play && soundAvailable)
        Mix_PlayChannel(-1, sounds[sound], 0);
}

/**
 * Plays the background music.
 */
void playBGM(void)
{
    if (!soundAvailable)
        return;

    if (!Mix_PlayingMusic()) {
        if ((Mix_PlayMusic(bgm, -1) == -1)) {
            if (olvl >= O_NORMAL)
                fprintf(stderr, "Couldn't play music: %s\n", Mix_GetError());
        }
    }
}

/**
 * Stops the background music.
 */
void stopBGM(void)
{
    if (!soundAvailable)
        return;

    Mix_HaltMusic();
}
