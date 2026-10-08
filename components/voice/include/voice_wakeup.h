#pragma once

/*
 * Future ESP-SR/WakeNet integration boundary.
 * This is an API contract only; an exact-phrase "Hello Robot" WakeNet
 * model is not bundled or initialized by the current repository.
 *
 * Do not claim that setting a string can program WakeNet to recognize it.
 */

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOICE_WAKE_PHRASE "Hello Robot"

typedef enum {
    VOICE_WAKE_UNAVAILABLE = 0, /* Missing model or uninitialized audio RX */
    VOICE_WAKE_IDLE,
    VOICE_WAKE_DETECTED,
    VOICE_WAKE_ERROR,
} voice_wakeup_state_t;

typedef void (*voice_wakeup_event_callback_t)(voice_wakeup_state_t state,
                                               void *user_ctx);

/* Return ESP_ERR_NOT_SUPPORTED until the real RX, AFE and model exist. */
esp_err_t voice_wakeup_start(voice_wakeup_event_callback_t callback,
                            void *user_ctx);
voice_wakeup_state_t voice_wakeup_get_state(void);
void voice_wakeup_stop(void);

#ifdef __cplusplus
}
#endif
