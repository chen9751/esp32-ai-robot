#pragma once
#include "esp_err.h"
#include "voice_wakeup.h"

/* Create one resident task. WakeNet callback only sends a notification. */
esp_err_t voice_bridge_init(voice_wakeup_event_callback_t callback, void *context);
void voice_bridge_notify(void);
