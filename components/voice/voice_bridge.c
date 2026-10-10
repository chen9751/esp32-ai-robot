#include "voice_bridge.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "audio_service.h"
#include "network_service.h"
#include "ui_manager.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define INPUT_RATE 24000u
#define INPUT_CHANNELS 2u
#define RECORD_SECONDS 3u
#define RECORD_BYTES (INPUT_RATE * INPUT_CHANNELS * 2u * RECORD_SECONDS)
#define MAX_REPLY_BYTES (1024u * 1024u)
#define CAPTURE_STEP 1024u

static const char *TAG = "voice_bridge";
static TaskHandle_t s_worker;
static voice_wakeup_event_callback_t s_wake_callback;
static void *s_wake_context;

static void put16(uint8_t *p, uint16_t n)
{
    p[0] = n & 255u; p[1] = n >> 8;
}

static void put32(uint8_t *p, uint32_t n)
{
    p[0] = n & 255u; p[1] = (n >> 8) & 255u;
    p[2] = (n >> 16) & 255u; p[3] = n >> 24;
}

static void wav_header(uint8_t *p, uint32_t bytes)
{
    memcpy(p, "RIFF", 4); put32(p + 4, bytes + 36);
    memcpy(p + 8, "WAVEfmt ", 8); put32(p + 16, 16);
    put16(p + 20, 1); put16(p + 22, INPUT_CHANNELS);
    put32(p + 24, INPUT_RATE); put32(p + 28, INPUT_RATE * 4);
    put16(p + 32, 4); put16(p + 34, 16);
    memcpy(p + 36, "data", 4); put32(p + 40, bytes);
}

/* Request/response buffers live in PSRAM. Do not allocate large DMA buffers. */
static esp_err_t perform_roundtrip(const network_backend_config_t *cfg)
{
    const size_t wav_bytes = 44u + RECORD_BYTES;
    uint8_t *request = heap_caps_malloc(wav_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!request) return ESP_ERR_NO_MEM;
    wav_header(request, RECORD_BYTES);
    esp_err_t err = ESP_OK;
    for (size_t at = 44; at < wav_bytes; at += CAPTURE_STEP) {
        if (audio_service_capture_read(request + at, CAPTURE_STEP) != ESP_OK) {
            err = ESP_FAIL;
            goto done;
        }
    }

    ESP_LOGI(TAG, "Captured %u bytes; uploading to local AI", (unsigned)wav_bytes);
    char authorization[NETWORK_AI_TOKEN_MAX + 8];
    snprintf(authorization, sizeof(authorization), "Bearer %s", cfg->ai_token);
    esp_http_client_config_t http_config = {
        .url = cfg->ai_url,
        .timeout_ms = 30000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) { err = ESP_ERR_NO_MEM; goto done; }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", authorization);
    esp_http_client_set_header(client, "Content-Type", "audio/wav");
    err = esp_http_client_open(client, (int)wav_bytes);
    if (err != ESP_OK) { esp_http_client_cleanup(client); goto done; }
    size_t sent = 0;
    while (sent < wav_bytes) {
        const int chunk = (wav_bytes - sent) > 4096 ? 4096 : (int)(wav_bytes - sent);
        int n = esp_http_client_write(client, (const char *)request + sent, chunk);
        if (n <= 0) { err = ESP_FAIL; break; }
        sent += (size_t)n;
    }
    heap_caps_free(request);
    request = NULL;
    if (err != ESP_OK) { esp_http_client_close(client); esp_http_client_cleanup(client); return err; }

    /* The server can take tens of seconds to run Whisper/Gemma/Piper. */
    int64_t content_size = esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200 || content_size > MAX_REPLY_BYTES) {
        ESP_LOGW(TAG, "AI HTTP response %d length=%lld", status, (long long)content_size);
        err = ESP_FAIL;
        goto close_client;
    }

    uint8_t *reply = heap_caps_malloc(MAX_REPLY_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!reply) { err = ESP_ERR_NO_MEM; goto close_client; }
    size_t used = 0;
    int idle = 0;
    while (used < MAX_REPLY_BYTES) {
        int n = esp_http_client_read(client, (char *)reply + used,
                                     (MAX_REPLY_BYTES - used) > 4096 ? 4096 : (int)(MAX_REPLY_BYTES - used));
        if (n < 0) { err = ESP_FAIL; break; }
        if (n == 0) {
            if (esp_http_client_is_complete_data_received(client)) break;
            if (++idle >= 3) { err = ESP_ERR_TIMEOUT; break; }
            continue;
        }
        idle = 0;
        used += (size_t)n;
    }
    if (used == MAX_REPLY_BYTES && !esp_http_client_is_complete_data_received(client))
        err = ESP_ERR_INVALID_SIZE;
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Received %u bytes WAV; playing", (unsigned)used);
        err = audio_service_play_wav(reply, used);
    }
    heap_caps_free(reply);
close_client:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
done:
    heap_caps_free(request);
    return err;
}

static void bridge_worker(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        network_backend_config_t cfg = {0};
        network_wifi_status_t wifi = {0};
        network_service_get_wifi_status(&wifi);
        network_service_get_backend_config(&cfg);
        if (!wifi.connected || !cfg.ai_url[0] || !cfg.ai_token[0]) {
            ESP_LOGW(TAG, "AI bridge disabled: require Wi-Fi, ai.url and ai.token");
            ui_notify_voice_finished();
            continue;
        }

        /* Give the already-queued wake greeting time to play; the WakeNet
         * detector must not be stopped or joined from its own callback. */
        vTaskDelay(pdMS_TO_TICKS(1200));
        voice_wakeup_stop();
        if (voice_wakeup_get_state() != VOICE_WAKE_UNAVAILABLE) {
            ESP_LOGW(TAG, "WakeNet stop unsuccessful; skip recording");
            ui_notify_voice_finished();
            continue;
        }
        esp_err_t err = perform_roundtrip(&cfg);
        ESP_LOGI(TAG, "Voice roundtrip: %s", esp_err_to_name(err));
        /* Includes synchronous ES8311 PCM playback; close after it returns. */
        ui_notify_voice_finished();
        err = voice_wakeup_start(s_wake_callback, s_wake_context);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "WakeNet resume failed: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t voice_bridge_init(voice_wakeup_event_callback_t callback, void *context)
{
    if (s_worker) return ESP_OK;
    if (!callback) return ESP_ERR_INVALID_ARG;
    s_wake_callback = callback;
    s_wake_context = context;
    return xTaskCreate(bridge_worker, "ai_voice", 7168, NULL, 4, &s_worker) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM;
}

void voice_bridge_notify(void)
{
    if (s_worker) xTaskNotifyGive(s_worker);
    else ui_notify_voice_finished(); /* Worker unavailable: don't leave overlay stuck. */
}
