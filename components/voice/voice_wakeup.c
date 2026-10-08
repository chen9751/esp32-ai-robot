#include "voice_wakeup.h"

#include <stdint.h>
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
static volatile voice_wakeup_state_t s_state = VOICE_WAKE_UNAVAILABLE;
static volatile bool s_running = false;
static voice_wakeup_event_callback_t s_callback = NULL;
static void *s_context = NULL;

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
    const int n = s_afe->get_feed_chunksize(s_afe_data);
    const int channels = s_afe->get_feed_channel_num(s_afe_data);
    if (n <= 0 || (n % 2) || channels != 1) {
        ESP_LOGE(TAG, "unsupported AFE input geometry: samples=%d channels=%d", n, channels);
        s_state = VOICE_WAKE_ERROR;
        s_running = false;
        vTaskDelete(NULL);
        return;
    }
    const size_t source_frames = (size_t)n * 3 / 2;
    /* esp_codec_dev_read -> i2s_channel_read copies from the driver's DMA
     * buffers into this application buffer. It is not a DMA destination.
     * Keep driver DMA internal; both CPU-side PCM buffers can use PSRAM. */
    const size_t source_bytes = source_frames * 2 * sizeof(int16_t);
    int16_t *source = heap_caps_malloc(source_bytes,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *mono = heap_caps_malloc((size_t)n * sizeof(int16_t),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!source || !mono) {
        ESP_LOGE(TAG, "audio feed buffer allocation failed");
        free(source);
        free(mono);
        s_state = VOICE_WAKE_ERROR;
        s_running = false;
        vTaskDelete(NULL);
        return;
    }
    uint32_t errors = 0;
    ESP_LOGI(TAG, "PCM scratch in PSRAM: source=%u mono=%u bytes",
             (unsigned)source_bytes, (unsigned)((size_t)n * sizeof(int16_t)));
    TickType_t next_report = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
    while (s_running) {
        if (audio_service_capture_read(source, source_frames * 2 * sizeof(int16_t)) != ESP_OK) {
            if (++errors % 100 == 1) ESP_LOGW(TAG, "microphone read failed (%lu)", (unsigned long)errors);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        convert_24k_to_16k(source, mono, n);
        s_afe->feed(s_afe_data, mono);
        const TickType_t now = xTaskGetTickCount();
        if ((int32_t)(now - next_report) >= 0) {
            ESP_LOGI(TAG, "STACK wn_feed: min_free=%u bytes; read_errors=%lu",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned long)errors);
            next_report = now + pdMS_TO_TICKS(60000);
        }
    }
    free(source);
    free(mono);
    vTaskDelete(NULL);
}

static void detect_task(void *arg)
{
    (void)arg;
    TickType_t next_report = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
    while (s_running) {
        afe_fetch_result_t *res = s_afe->fetch(s_afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "AFE fetch failed");
            s_state = VOICE_WAKE_ERROR;
            break;
        }
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "WAKE WORD DETECTED: Hi ESP");
            s_state = VOICE_WAKE_DETECTED;
            if (s_callback) s_callback(s_state, s_context);
            s_state = VOICE_WAKE_IDLE;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((int32_t)(now - next_report) >= 0) {
            ESP_LOGI(TAG, "STACK wn_detect: min_free=%u bytes",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            next_report = now + pdMS_TO_TICKS(60000);
        }
    }
    s_running = false;
    vTaskDelete(NULL);
}

esp_err_t voice_wakeup_start(voice_wakeup_event_callback_t cb, void *ctx)
{
    if (s_running) return ESP_ERR_INVALID_STATE;
    if (!audio_service_capture_ready()) return ESP_ERR_INVALID_STATE;

    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "model partition unavailable; use idf.py flash (not app-flash)");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "AFE before create: internal=%u largest=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    /* HIGH_PERF can trade PSRAM for internal DRAM on ESP32-S3 SR paths.
     * This is an A/B memory experiment against the proven LOW_COST baseline.
     * Model, mic geometry and UI remain unchanged. */
    afe_config_t *cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (!cfg) return ESP_ERR_NO_MEM;
    s_afe = esp_afe_handle_from_config(cfg);
    s_afe_data = s_afe ? s_afe->create_from_config(cfg) : NULL;
    afe_config_free(cfg);
    ESP_LOGI(TAG, "AFE after create: internal=%u largest=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (!s_afe_data) {
        ESP_LOGE(TAG, "AFE create failed (model selection or memory)");
        return ESP_FAIL;
    }
    s_callback = cb;
    s_context = ctx;
    s_running = true;
    s_state = VOICE_WAKE_IDLE;
    if (xTaskCreate(feed_task, "wn_feed", 6144, NULL, 5, NULL) != pdPASS) {
        s_running = false;
        s_state = VOICE_WAKE_ERROR;
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(detect_task, "wn_detect", 6144, NULL, 5, NULL) != pdPASS) {
        s_running = false;
        s_state = VOICE_WAKE_ERROR;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "WakeNet started; expected phrase: Hi ESP");
    return ESP_OK;
}

voice_wakeup_state_t voice_wakeup_get_state(void) { return s_state; }
void voice_wakeup_stop(void) { s_running = false; }
