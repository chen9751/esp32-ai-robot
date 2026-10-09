#include "voice_mock.h"
/* Test production lifecycle code with deterministic allocation/task failures. */
#define free tracked_free
#include "../../components/voice/voice_wakeup.c"
#undef free
static atomic_bool callback_called, release_callback;
static void slow_callback(voice_wakeup_state_t state,void *ctx) {
    (void)state;(void)ctx;callback_called=true;
    while(!release_callback)usleep(1000);
}
static void stop_in_callback(voice_wakeup_state_t state,void *ctx) {
    (void)state;(void)ctx;voice_wakeup_stop();callback_called=true;
}
int main(void) {
    for (int fail=1;fail<=5;fail++) {
        alloc_calls=0;alloc_fail=fail;
        assert(voice_wakeup_start(NULL,NULL)!=ESP_OK);
        voice_wakeup_stop();reap_tasks();assert(live_allocs==0);
    }
    alloc_fail=0;
    for (int fail=1;fail<=2;fail++) {
        task_calls=0;task_fail=fail;
        assert(voice_wakeup_start(NULL,NULL)==ESP_ERR_NO_MEM);
        voice_wakeup_stop();reap_tasks();assert(live_allocs==0);
    }
    task_fail=0;
    for(int i=0;i<30;i++) {
        assert(voice_wakeup_start(NULL,NULL)==ESP_OK);
        assert(voice_wakeup_start(NULL,NULL)==ESP_ERR_INVALID_STATE);
        usleep(3000);voice_wakeup_stop();reap_tasks();assert(live_allocs==0);
        assert(voice_wakeup_get_state()==VOICE_WAKE_UNAVAILABLE);
    }
    microphone_error=true;
    assert(voice_wakeup_start(NULL,NULL)==ESP_OK);
    usleep(220000);assert(voice_wakeup_get_state()==VOICE_WAKE_IDLE);
    voice_wakeup_stop();reap_tasks();assert(live_allocs==0);microphone_error=false;
    generate_wake=true;
    assert(voice_wakeup_start(stop_in_callback,NULL)==ESP_OK);
    TickType_t start=xTaskGetTickCount();
    while(!callback_called&&xTaskGetTickCount()-start<1000)usleep(1000);
    assert(callback_called);
    /* Restart also reaps a deferred callback stop. */
    assert(voice_wakeup_start(NULL,NULL)==ESP_OK);
    voice_wakeup_stop();reap_tasks();assert(live_allocs==0);
    callback_called=false;generate_wake=true;
    assert(voice_wakeup_start(slow_callback,NULL)==ESP_OK);
    start=xTaskGetTickCount();
    while(!callback_called&&xTaskGetTickCount()-start<1000)usleep(1000);
    assert(callback_called);
    voice_wakeup_stop();
    assert(voice_wakeup_get_state()==VOICE_WAKE_ERROR && live_allocs>0);
    release_callback=true;
    voice_wakeup_stop();reap_tasks();assert(live_allocs==0);
    puts("PASS: 5 allocation failures, 2 task failures, 30 restart cycles, fetch timeout, callback stop/restart, stop timeout retains active resources");
}
