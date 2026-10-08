#include "audio.h"

#include <stdio.h>

#include <SDL.h>

#define TONE_FREQUENCY_HZ 440
#define SAMPLE_RATE 44100
#define TONE_VOLUME 0x4000 /* half scale: a square wave at full scale is harsh */

/*
 * Runs on the SDL audio thread. Writes a 440 Hz square wave while the
 * beeping flag is set, silence otherwise.
 */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    Audio *audio = (Audio *)userdata;
    Sint16 *out = (Sint16 *)stream;
    int sample_count = len / (int)sizeof(Sint16);
    bool beeping = SDL_AtomicGet(&audio->beeping) != 0;

    if (!beeping) {
        SDL_memset(stream, 0, (size_t)len);
        audio->phase = 0.0; /* every beep starts on the same edge */
        return;
    }

    for (int i = 0; i < sample_count; i++) {
        out[i] = (audio->phase < 0.5) ? TONE_VOLUME : (Sint16)-TONE_VOLUME;
        audio->phase += (double)TONE_FREQUENCY_HZ / (double)SAMPLE_RATE;
        if (audio->phase >= 1.0) {
            audio->phase -= 1.0;
        }
    }
}

bool audio_init(Audio *audio)
{
    audio->initialized = false;
    audio->device = 0;
    audio->phase = 0.0;
    SDL_AtomicSet(&audio->beeping, 0);

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = audio;

    /* NULL with allowed_changes 0: the spec must be met exactly. */
    audio->device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (audio->device == 0) {
        fprintf(stderr, "Could not open audio device: %s (running silently)\n",
                SDL_GetError());
        return true;
    }

    SDL_PauseAudioDevice(audio->device, 0); /* start the callback */
    audio->initialized = true;
    return true;
}

void audio_set_beeping(Audio *audio, bool beeping)
{
    SDL_AtomicSet(&audio->beeping, beeping ? 1 : 0);
}

void audio_close(Audio *audio)
{
    SDL_AtomicSet(&audio->beeping, 0);
    if (audio->device != 0) {
        SDL_CloseAudioDevice(audio->device);
        audio->device = 0;
    }
    audio->initialized = false;
}
