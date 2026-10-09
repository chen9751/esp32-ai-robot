#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_service_init(void);
esp_err_t audio_service_set_volume(uint8_t percent);
uint8_t audio_service_get_volume(void);

/* 24 kHz, interleaved stereo signed 16-bit PCM from ES7210.
 * Future WakeNet integration must resample or change shared duplex clock
 * coherently to 16 kHz. */
bool audio_service_capture_ready(void);
esp_err_t audio_service_capture_read(void *pcm, size_t bytes);
esp_err_t audio_service_capture_diagnostic_start(void);
void audio_service_stop(void);
/* Nonblocking greeting trigger for WakeNet callback. */
void audio_service_play_hello(void);

#ifdef __cplusplus
}
#endif
