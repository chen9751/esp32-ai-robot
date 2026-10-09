#include "voice_wakeup.h"

#include <stdint.h>
#include <stdatomic.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "audio_service.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_process_sdkconfig.h"
#include "model_path.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "voice_wakeup";
static const esp_afe_sr_iface_t *s_afe = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;
static _Atomic voice_wakeup_state_t s_state = VOICE_WAKE_UNAVAILABLE;
static atomic_bool s_running = false;
static voice_wakeup_event_callback_t s_callback = NULL;
static void *s_context = NULL;

/* Lifecycle calls never free an instance while either worker can use it. */
static atomic_flag s_lifecycle = ATOMIC_FLAG_INIT;
static atomic_bool s_feed_done = true, s_detect_done = true;
static _Atomic(TaskHandle_t) s_feed_task, s_detect_task;
static srmodel_list_t *s_models;
static int16_t *s_source, *s_mono;
static int s_samples;
static size_t s_source_bytes;

static bool join_workers(void)
{
    TickType_t start = xTaskGetTickCount();
    while (!s_feed_done || !s_detect_done) {
        if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(3500)) {
            ESP_LOGE(TAG, "workers still active; retaining AFE until stop completes");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_feed_task = NULL;
    s_detect_task = NULL;
    return true;
}

static void release_instance(void)
{
    if (s_afe_data) s_afe->destroy(s_afe_data);
    s_afe_data = NULL;
    s_afe = NULL;
    if (s_models) esp_srmodel_deinit(s_models);
    s_models = NULL;
    free(s_source); s_source = NULL;
    free(s_mono); s_mono = NULL;
    s_callback = NULL;
    s_context = NULL;
}

/* Convert 24 kHz stereo to 16 kHz mono: 3 source frames -> 2 target
 * frames. Deliberately choose left codec channel at first; measured
 * slot ordering must be confirmed on the actual Waveshare V2 board. */
static void convert_24k_to_16k(const int16_t *in, int16_t *out, int n)
{
    for (int i = 0; i < n / 2; ++i) {
        const int k = i * 6;
        out[i * 2] = in[k];
        out[i * 2 + 1] = (int16_t)(((int32_t)in[k + 2] + in[k + 4]) / 2);
    }
}

static void feed_task(void *arg)
{
    (void)arg;
    /* Both tasks must exist before either accesses the AFE. */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const int n = s_samples;
    int16_t *source = s_source, *mono = s_mono;
    const size_t source_bytes = s_source_bytes;
    uint32_t errors = 0;
    ESP_LOGI(TAG, "PCM scratch in PSRAM: source=%u mono=%u bytes",
             (unsigned)source_bytes, (unsigned)((size_t)n * sizeof(int16_t)));
    TickType_t next_report = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
    while (s_running) {
        if (audio_service_capture_read(source, source_bytes) != ESP_OK) {
            if (++errors % 100 == 1) ESP_LOGW(TAG, "microphone read failed (%lu)", (unsigned long)errors);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (!s_running) break;
        convert_24k_to_16k(source, mono, n);
        s_afe->feed(s_afe_data, mono);
        const TickType_t now = xTaskGetTickCount();
        if ((int32_t)(now - next_report) >= 0) {
            ESP_LOGI(TAG, "STACK wn_feed: min_free=%u bytes; read_errors=%lu",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned long)errors);
            next_report = now + pdMS_TO_TICKS(60000);
        }
    }
    s_feed_done = true;
    vTaskDelete(NULL);
}

static void detect_task(void *arg)
{
    (void)arg;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    TickType_t next_report = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
    /* Drain while the producer exits: feed may be waiting for ring space. */
    while (s_running || !s_feed_done) {
        afe_fetch_result_t *res = s_afe->fetch_with_delay(s_afe_data, pdMS_TO_TICKS(100));
        /* A timed fetch with no data is normal (including microphone errors). */
        if (!res || res->ret_value == ESP_FAIL) {
            /* Also yield if a driver reports an immediate error. */
            vTaskDelay(1);
            continue;
        }
        if (!s_running) continue;
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "WAKE WORD DETECTED: Hi ESP");
            s_state = VOICE_WAKE_DETECTED;
            if (s_callback) s_callback(s_state, s_context);
            if (s_running) s_state = VOICE_WAKE_IDLE;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((int32_t)(now - next_report) >= 0) {
            ESP_LOGI(TAG, "STACK wn_detect: min_free=%u bytes",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            next_report = now + pdMS_TO_TICKS(60000);
        }
    }
    s_detect_done = true;
    vTaskDelete(NULL);
}

esp_err_t voice_wakeup_start(voice_wakeup_event_callback_t cb, void *ctx)
{
    if (atomic_flag_test_and_set(&s_lifecycle)) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_running || !audio_service_capture_ready()) goto done;
    if (!join_workers()) { err = ESP_ERR_TIMEOUT; goto done; }
    release_instance();
    s_models = esp_srmodel_init("model");
    if (!s_models) {
        ESP_LOGE(TAG, "model partition unavailable; use idf.py flash (not app-flash)");
        err = ESP_ERR_NOT_FOUND;
        goto fail;
    }
    ESP_LOGI(TAG, "AFE before create: internal=%u largest=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    /* Preserve the tested HIGH_PERF profile and microphone geometry. */
    afe_config_t *cfg = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (!cfg) { err = ESP_ERR_NO_MEM; goto fail; }
    s_afe = esp_afe_handle_from_config(cfg);
    s_afe_data = s_afe ? s_afe->create_from_config(cfg) : NULL;
    afe_config_free(cfg);
    ESP_LOGI(TAG, "AFE after create: internal=%u largest=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (!s_afe_data) { err = ESP_FAIL; goto fail; }
    s_samples = s_afe->get_feed_chunksize(s_afe_data);
    if (s_samples <= 0 || s_samples % 2 || s_samples > INT_MAX / 6 ||
        s_afe->get_feed_channel_num(s_afe_data) != 1) {
        err = ESP_ERR_INVALID_SIZE;
        goto fail;
    }
    s_source_bytes = (size_t)s_samples * 3 * sizeof(int16_t);
    /* CPU destinations, copied from the I2S driver's internal DMA buffers. */
    s_source = heap_caps_malloc(s_source_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_mono = heap_caps_malloc((size_t)s_samples * sizeof(int16_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_source || !s_mono) { err = ESP_ERR_NO_MEM; goto fail; }
    s_callback = cb;
    s_context = ctx;
    TaskHandle_t feed = NULL, detect = NULL;
    s_feed_done = false;
    if (xTaskCreate(feed_task, "wn_feed", 6144, NULL, 5, &feed) != pdPASS) {
        s_feed_done = true;
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    s_feed_task = feed;
    s_detect_done = false;
    if (xTaskCreate(detect_task, "wn_detect", 6144, NULL, 5, &detect) != pdPASS) {
        s_detect_done = true;
        /* Release the gated producer with running=false; it cannot enter AFE. */
        xTaskNotifyGive(feed);
        err = ESP_ERR_NO_MEM;
        if (!join_workers()) { s_state = VOICE_WAKE_ERROR; goto done; }
        goto fail;
    }
    s_detect_task = detect;
    s_state = VOICE_WAKE_IDLE;
    s_running = true;
    xTaskNotifyGive(detect);
    xTaskNotifyGive(feed);
    ESP_LOGI(TAG, "WakeNet started; expected phrase: Hi ESP");
    err = ESP_OK;
    goto done;
fail:
    release_instance();
    s_state = VOICE_WAKE_ERROR;
done:
    atomic_flag_clear(&s_lifecycle);
    return err;
}

voice_wakeup_state_t voice_wakeup_get_state(void) { return s_state; }

void voice_wakeup_stop(void)
{
    /* A callback may request stop; it must never join its own worker. An
     * external stop/start reaps that instance after the callback returns. */
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    if (self == s_detect_task || self == s_feed_task) {
        s_running = false;
        s_state = VOICE_WAKE_UNAVAILABLE;
        return;
    }
    TickType_t start = xTaskGetTickCount();
    while (atomic_flag_test_and_set(&s_lifecycle)) {
        if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(3500)) {
            ESP_LOGE(TAG, "lifecycle busy; stop must be retried by its owner");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_running = false;
    if (join_workers()) {
        release_instance();
        s_state = VOICE_WAKE_UNAVAILABLE;
    } else {
        s_state = VOICE_WAKE_ERROR;
    }
    atomic_flag_clear(&s_lifecycle);
}
