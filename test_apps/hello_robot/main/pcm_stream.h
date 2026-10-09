#pragma once
#include <stddef.h>
#include <stdint.h>

/* ES7210 / project RX: 24 kHz, interleaved 2-channel S16.
 * 240 stereo frames (10 ms) -> 160 mono frames (10 ms at 16 kHz).
 * Source left slot is provisional; verify actual mic channel on device.
 * Stateless interpolation matches the existing WakeNet feed conversion.
 * Feature extraction (micro_speech frontend) happens AFTER this stage. */
#define HELLO_AUDIO_SOURCE_FRAMES 240
#define HELLO_AUDIO_TARGET_FRAMES 160
#define HELLO_AUDIO_CHANNELS 2

static inline void hello_pcm_24k_to_16k_left(
    const int16_t input[HELLO_AUDIO_SOURCE_FRAMES * HELLO_AUDIO_CHANNELS],
    int16_t output[HELLO_AUDIO_TARGET_FRAMES])
{
    for (size_t i = 0; i < HELLO_AUDIO_TARGET_FRAMES / 2; ++i) {
        const size_t k = i * 6;
        output[2 * i] = input[k];
        output[2 * i + 1] =
            (int16_t)(((int32_t)input[k + 2] + input[k + 4]) / 2);
    }
}
