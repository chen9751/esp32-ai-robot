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
typedef enum {
    AUDIO_ALERT_NONE = 0,
    AUDIO_ALERT_TIMER,
    AUDIO_ALERT_ALARM,
} audio_alert_kind_t;

esp_err_t audio_service_play_timer(void);
esp_err_t audio_service_play_alarm(void);
bool audio_service_alert_active(void);
audio_alert_kind_t audio_service_get_alert_kind(void);
void audio_service_stop(void);

#ifdef __cplusplus
}
#endif
