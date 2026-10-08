#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define HA_LIGHT_COUNT 8
typedef struct {
    bool available;
    bool on;
    uint8_t brightness_pct;
    uint16_t color_temp_k;
    uint16_t hue_deg;
    uint8_t saturation_pct;
} ha_light_state_t;

typedef enum { HA_LIGHT_POWER, HA_LIGHT_BRIGHTNESS, HA_LIGHT_TEMPERATURE, HA_LIGHT_COLOR } ha_light_command_t;

/* Index is the existing UI card order; no network calls from LVGL callbacks. */
esp_err_t ha_lights_init(void);
bool ha_lights_get(int index, ha_light_state_t *out);
bool ha_lights_send(int index, ha_light_command_t command, int value, int extra);
const char *ha_lights_entity_id(int index);
