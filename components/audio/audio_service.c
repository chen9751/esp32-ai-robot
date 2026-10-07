#include "audio_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/i2c_master.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "audio";

#define AUDIO_SAMPLE_RATE 24000
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16
#define AUDIO_I2S_MCLK GPIO_NUM_7
#define AUDIO_I2S_BCLK GPIO_NUM_15
#define AUDIO_I2S_WS GPIO_NUM_46
#define AUDIO_I2S_DOUT GPIO_NUM_45
#define AUDIO_DEFAULT_VOLUME 30
#define AUDIO_ALERT_MAX_MS (10u * 60u * 1000u)
#define AUDIO_ALERT_VOLUME 80

typedef enum {
    SOUND_TIMER = 0,
    SOUND_ALARM,
} sound_kind_t;

static esp_codec_dev_handle_t s_playback = NULL;
static i2s_chan_handle_t s_tx = NULL;
static const audio_codec_data_if_t *s_data_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;
static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_if_t *s_codec_if = NULL;
static SemaphoreHandle_t s_write_lock = NULL;
static TaskHandle_t s_sound_task = NULL;
static volatile uint32_t s_generation = 1;
static bool s_ready = false;
static uint8_t s_volume = AUDIO_DEFAULT_VOLUME;
static volatile bool s_alert_active = false;
static volatile audio_alert_kind_t s_alert_kind = AUDIO_ALERT_NONE;

static bool generation_alive(uint32_t generation)
{
    return generation == s_generation;
}

static void fill_square_stereo(int16_t *pcm, size_t frames,
                               uint32_t frequency_hz, int16_t amplitude,
                               uint32_t *phase)
{
    if (frequency_hz == 0) {
        for (size_t i = 0; i < frames * 2; ++i) pcm[i] = 0;
        return;
    }

    const uint32_t period = AUDIO_SAMPLE_RATE / frequency_hz;
    const uint32_t half = period > 1 ? period / 2 : 1;
    for (size_t i = 0; i < frames; ++i) {
        int16_t sample = ((*phase % period) < half) ? amplitude : -amplitude;
        pcm[i * 2] = sample;
        pcm[i * 2 + 1] = sample;
        ++(*phase);
    }
}

static bool write_frames(uint32_t generation, uint32_t frequency_hz,
                         uint32_t duration_ms, int16_t amplitude)
{
    const size_t frames_per_chunk = 240;
    int16_t pcm[frames_per_chunk * 2];
    uint32_t phase = 0;
    uint32_t frames_left = (AUDIO_SAMPLE_RATE * duration_ms) / 1000U;

    while (frames_left > 0 && generation_alive(generation)) {
        size_t frames = frames_left > frames_per_chunk ? frames_per_chunk : frames_left;
        fill_square_stereo(pcm, frames, frequency_hz, amplitude, &phase);

        if (xSemaphoreTake(s_write_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }
        int rc = esp_codec_dev_write(s_playback, pcm,
                                     (int)(frames * AUDIO_CHANNELS * sizeof(int16_t)));
        xSemaphoreGive(s_write_lock);
        if (rc != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "audio write failed: %d", rc);
            return false;
        }
        frames_left -= frames;
    }
    return generation_alive(generation);
}

static bool silence(uint32_t generation, uint32_t duration_ms)
{
    return write_frames(generation, 0, duration_ms, 0);
}

static void sound_task(void *arg)
{
    (void)arg;

    for (;;) {
        uint32_t command = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        sound_kind_t kind;
        if (command == 1) {
            kind = SOUND_TIMER;
        }
        else if (command == 2) {
            kind = SOUND_ALARM;
        }
        else {
            continue;
        }

        const uint32_t generation = s_generation;
        const TickType_t started = xTaskGetTickCount();
        const TickType_t max_ticks = pdMS_TO_TICKS(AUDIO_ALERT_MAX_MS);

        while (generation_alive(generation) &&
               (xTaskGetTickCount() - started) < max_ticks) {
            if (kind == SOUND_TIMER) {
                /* Familiar digital kitchen-timer cadence:
                 * four short beeps, then a short pause, repeated until acknowledged. */
                for (int i = 0; i < 4 && generation_alive(generation); ++i) {
                    if (!write_frames(generation, 1100, 170, 11000)) break;
                    if (i != 3 && !silence(generation, 130)) break;
                }
                if (!generation_alive(generation)) break;
                if (!silence(generation, 1200)) break;
            }
            else {
                /* Classic bedside alarm: alternating two-tone buzzer,
                 * repeated until the user acknowledges it. */
                if (!write_frames(generation, 880, 280, 12000)) break;
                if (!silence(generation, 90)) break;
                if (!write_frames(generation, 660, 280, 12000)) break;
                if (!silence(generation, 500)) break;
            }
        }

        if (generation_alive(generation)) {
            ++s_generation; /* 10-minute safety timeout */
            s_alert_active = false;
            s_alert_kind = AUDIO_ALERT_NONE;
            (void)esp_codec_dev_set_out_vol(s_playback, s_volume);
        }
    }
}

static esp_err_t start_sound(sound_kind_t kind)
{
    if (!s_ready || s_sound_task == NULL) return ESP_ERR_INVALID_STATE;

    ++s_generation;
    s_alert_active = true;
    s_alert_kind = kind == SOUND_TIMER ? AUDIO_ALERT_TIMER : AUDIO_ALERT_ALARM;

    /* Alert playback is fixed at 80%, independently of the saved user volume. */
    if (esp_codec_dev_set_out_vol(s_playback, AUDIO_ALERT_VOLUME) != ESP_CODEC_DEV_OK) {
        s_alert_active = false;
        s_alert_kind = AUDIO_ALERT_NONE;
        return ESP_FAIL;
    }

    const uint32_t command = kind == SOUND_TIMER ? 1u : 2u;
    if (xTaskNotify(s_sound_task, command, eSetValueWithOverwrite) != pdPASS) {
        s_alert_active = false;
        s_alert_kind = AUDIO_ALERT_NONE;
        (void)esp_codec_dev_set_out_vol(s_playback, s_volume);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s alert queued at 80%%",
             kind == SOUND_TIMER ? "timer" : "alarm");
    return ESP_OK;
}

esp_err_t audio_service_init(void)
{
    if (s_ready) return ESP_OK;

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_master_get_bus_handle(I2C_NUM_0, &bus);
    if (err != ESP_OK || bus == NULL) {
        ESP_LOGE(TAG, "system I2C bus unavailable");
        return err == ESP_OK ? ESP_FAIL : err;
    }

    ESP_LOGI(TAG, "before I2S: internal free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    /* Default I2S DMA sizing is aimed at general streaming and is too large
     * for this UI appliance once Wi-Fi + BLE + LVGL are already resident.
     * Alerts only need a small steady PCM pipeline, so 3x128 frames is ample
     * at 24 kHz while cutting internal DMA usage dramatically. */
    chan_cfg.dma_desc_num = 3;
    chan_cfg.dma_frame_num = 128;
    err = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (err != ESP_OK) return err;

    i2s_tdm_slot_mask_t slot_mask =
        I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
            32, I2S_SLOT_MODE_STEREO, slot_mask),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK,
            .bclk = AUDIO_I2S_BCLK,
            .ws = AUDIO_I2S_WS,
            .dout = AUDIO_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    tdm_cfg.slot_cfg.total_slot = 4;

    err = i2s_channel_init_tdm_mode(s_tx, &tdm_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S TDM init failed: %s; internal free=%u largest=%u",
                 esp_err_to_name(err),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        i2s_del_channel(s_tx);
        s_tx = NULL;
        return err;
    }
    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) return err;

    s_gpio_if = audio_codec_new_gpio();

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = s_tx,
        .rx_handle = NULL,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bus,
    };
    s_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = s_ctrl_if,
        .gpio_if = s_gpio_if,
        .pa_pin = -1,
        .use_mclk = true,
        .hw_gain = {
            .pa_gain = 6,
        },
    };
    s_codec_if = es8311_codec_new(&es8311_cfg);

    if (s_gpio_if == NULL || s_data_if == NULL ||
        s_ctrl_if == NULL || s_codec_if == NULL) {
        ESP_LOGE(TAG, "ES8311 interface creation failed");
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = s_codec_if,
        .data_if = s_data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    s_playback = esp_codec_dev_new(&dev_cfg);
    if (s_playback == NULL) return ESP_FAIL;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channel = AUDIO_CHANNELS,
        .bits_per_sample = AUDIO_BITS,
    };
    if (esp_codec_dev_open(s_playback, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "ES8311 playback open failed");
        return ESP_FAIL;
    }

    s_write_lock = xSemaphoreCreateMutex();
    if (s_write_lock == NULL) return ESP_ERR_NO_MEM;

    /* Allocate the alert worker once, during boot while contiguous internal
     * RAM is still available. Creating/deleting a 4 KB task for every ring was
     * unreliable after Wi-Fi/BLE/LVGL had consumed and fragmented internal
     * SRAM; ESP-IDF also notes that memory from a self-deleted task can be
     * reclaimed later by the idle task. */
    if (xTaskCreate(sound_task, "alert_audio", 4096, NULL, 4,
                    &s_sound_task) != pdPASS) {
        ESP_LOGE(TAG, "persistent alert task creation failed");
        s_sound_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_volume = AUDIO_DEFAULT_VOLUME;
    esp_codec_dev_set_out_vol(s_playback, s_volume);
    s_ready = true;
    ESP_LOGI(TAG,
             "ES8311 playback ready, volume=%u%%, internal free=%u largest=%u",
             (unsigned)s_volume,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return ESP_OK;
}

esp_err_t audio_service_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_volume = percent;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    /* Store user preference while an alert is active, but keep the alert at
     * its dedicated 80% level until acknowledgement. */
    if (s_alert_active) return ESP_OK;

    return esp_codec_dev_set_out_vol(s_playback, percent) == ESP_CODEC_DEV_OK
               ? ESP_OK : ESP_FAIL;
}

uint8_t audio_service_get_volume(void)
{
    return s_volume;
}

esp_err_t audio_service_play_timer(void)
{
    return start_sound(SOUND_TIMER);
}

esp_err_t audio_service_play_alarm(void)
{
    return start_sound(SOUND_ALARM);
}

bool audio_service_alert_active(void)
{
    return s_alert_active;
}

audio_alert_kind_t audio_service_get_alert_kind(void)
{
    return s_alert_active ? s_alert_kind : AUDIO_ALERT_NONE;
}

void audio_service_stop(void)
{
    ++s_generation;
    s_alert_active = false;
    s_alert_kind = AUDIO_ALERT_NONE;
    if (s_ready) {
        (void)esp_codec_dev_set_out_vol(s_playback, s_volume);
    }
}
