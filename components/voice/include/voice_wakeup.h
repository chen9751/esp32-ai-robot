#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Temporary factory model, NOT the future Hello Robot model. */
#define VOICE_WAKE_PHRASE "Hi ESP"

typedef enum {
    VOICE_WAKE_UNAVAILABLE = 0,
    VOICE_WAKE_IDLE,
    VOICE_WAKE_DETECTED,
    VOICE_WAKE_ERROR,
} voice_wakeup_state_t;

/* Called on the detector task: enqueue work and return promptly. Blocking
 * network/playback work here would stall AFE draining. */
typedef void (*voice_wakeup_event_callback_t)(voice_wakeup_state_t state, void *user_ctx);
esp_err_t voice_wakeup_start(voice_wakeup_event_callback_t cb, void *ctx);
voice_wakeup_state_t voice_wakeup_get_state(void);
/* Application lifecycle calls should be serialized. Stop waits for workers
 * for up to 3.5 s; on timeout it retains the instance (ERROR), so retry stop.
 * A stop from the wake callback only requests exit; the next external
 * stop/start reclaims the instance, avoiding a self-join. */
void voice_wakeup_stop(void);

#ifdef __cplusplus
}
#endif
