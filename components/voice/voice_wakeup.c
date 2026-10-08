#include "voice_wakeup.h"

#include "esp_log.h"

static const char *TAG = "voice_wakeup";

/*
 * Explicit disabled adapter, not an accidental always-success mock.
 * Wiring this into app_main prematurely must not suggest that the
 * "Hello Robot" model has been trained or that microphone RX works.
 */
esp_err_t voice_wakeup_start(voice_wakeup_event_callback_t callback,
                            void *user_ctx)
{
    (void)callback;
    (void)user_ctx;
    ESP_LOGW(TAG, "WakeNet inactive: missing ES7210 RX/AFE and '%s' model",
             VOICE_WAKE_PHRASE);
    return ESP_ERR_NOT_SUPPORTED;
}

voice_wakeup_state_t voice_wakeup_get_state(void)
{
    return VOICE_WAKE_UNAVAILABLE;
}

void voice_wakeup_stop(void)
{
}
