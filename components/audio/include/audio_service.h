#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_service_init(void);
esp_err_t audio_service_set_volume(uint8_t percent);
uint8_t audio_service_get_volume(void);

void audio_service_stop(void);

#ifdef __cplusplus
}
#endif
