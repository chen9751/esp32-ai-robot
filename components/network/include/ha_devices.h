#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
 bool valid;
 bool ac_on; int ac_temp_x2; int ac_mode; int ac_fan; bool ac_auto, ac_swing; bool ac_features[4];
 bool curtain_valid, rack_valid;
 int curtain_pos; int rack_pos;
 int bath_current_x10, bath_target_x10; int bath_levels[3]; bool bath_dry;
} ha_devices_state_t;
esp_err_t ha_devices_init(void);
bool ha_devices_get(ha_devices_state_t *out);
bool ha_devices_command(const char *domain, const char *service, const char *entity, const char *params);
