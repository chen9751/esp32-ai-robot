#include "audio_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/i2c_master.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
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

typedef enum {
    SOUND_TIMER = 0,
    SOUND_ALARM,
} sound_kind_t;

typedef struct {
    sound_kind_t kind;
    uint32_t generation;
} sound_task_arg_t;

static esp_codec_dev_handle_t s_playback = NULL;
static i2s_chan_handle_t s_tx = NULL;
static const audio_codec_data_if_t *s_data_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;
static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_if_t *s_codec_if = NULL;
static SemaphoreHandle_t s_write_lock = NULL;
static volatile uint32_t s_generation = 1;
static bool s_ready = false;
static uint8_t s_volume = AUDIO_DEFAULT_VOLUME;
static volatile bool s_alert_active = false;

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
    sound_task_arg_t req = *(sound_task_arg_t *)arg;
    free(arg);

    if (!s_ready || !generation_alive(req.generation)) {
        vTaskDelete(NULL);
        return;
    }

    s_alert_active = true;
    const TickType_t started = xTaskGetTickCount();
    const TickType_t max_ticks = pdMS_TO_TICKS(AUDIO_ALERT_MAX_MS);

    while (generation_alive(req.generation) &&
           (xTaskGetTickCount() - started) < max_ticks) {
        if (req.kind == SOUND_TIMER) {
            /* Familiar digital kitchen-timer cadence:
             * four short beeps, then a short pause, repeated until acknowledged. */
            for (int i = 0; i < 4 && generation_alive(req.generation); ++i) {
                if (!write_frames(req.generation, 1100, 170, 11000)) break;
                if (i != 3 && !silence(req.generation, 130)) break;
            }
            if (!generation_alive(req.generation)) break;
            if (!silence(req.generation, 1200)) break;
        }
        else {
            /* Classic bedside alarm: alternating two-tone buzzer,
             * repeated until the user touches the screen. */
            if (!write_frames(req.generation, 880, 280, 12000)) break;
            if (!silence(req.generation, 90)) break;
            if (!write_frames(req.generation, 660, 280, 12000)) break;
            if (!silence(req.generation, 500)) break;
        }
    }

    if (generation_alive(req.generation)) {
        ++s_generation; /* 10-minute safety timeout */
    }
    s_alert_active = false;
    vTaskDelete(NULL);
}

static esp_err_t start_sound(sound_kind_t kind)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    ++s_generation;
    s_alert_active = true;
    sound_task_arg_t *arg = malloc(sizeof(*arg));
    if (arg == NULL) return ESP_ERR_NO_MEM;
    arg->kind = kind;
    arg->generation = s_generation;

    if (xTaskCreate(sound_task, kind == SOUND_TIMER ? "timer_sound" : "alarm_sound",
                    4096, arg, 4, NULL) != pdPASS) {
        free(arg);
        return ESP_ERR_NO_MEM;
    }
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

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
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
    if (err != ESP_OK) return err;
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

    s_volume = AUDIO_DEFAULT_VOLUME;
    esp_codec_dev_set_out_vol(s_playback, s_volume);
    s_ready = true;
    ESP_LOGI(TAG, "ES8311 playback ready, volume=%u%%", (unsigned)s_volume);
    return ESP_OK;
}

esp_err_t audio_service_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_volume = percent;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
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

void audio_service_stop(void)
{
    ++s_generation;
    s_alert_active = false;
}
