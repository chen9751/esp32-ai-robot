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
#include "freertos/task.h"
#include "esp_err.h"

static const char *TAG = "audio";

#define AUDIO_SAMPLE_RATE 24000
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16
#define AUDIO_I2S_MCLK GPIO_NUM_7
#define AUDIO_I2S_BCLK GPIO_NUM_15
#define AUDIO_I2S_WS GPIO_NUM_46
#define AUDIO_I2S_DOUT GPIO_NUM_45
#define AUDIO_I2S_DIN GPIO_NUM_6
#define AUDIO_MIC_GAIN_DB 30.0f
#define AUDIO_DEFAULT_VOLUME 30


static esp_codec_dev_handle_t s_playback = NULL;
static i2s_chan_handle_t s_tx = NULL;
static i2s_chan_handle_t s_rx = NULL;
static esp_codec_dev_handle_t s_record = NULL;
static bool s_capture_ready = false;
static bool s_capture_task_running = false;
static const audio_codec_data_if_t *s_data_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;
static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_if_t *s_codec_if = NULL;
static bool s_ready = false;
static uint8_t s_volume = AUDIO_DEFAULT_VOLUME;

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

    /* Reserve a compact audio DMA channel for future media playback. */
    chan_cfg.dma_desc_num = 3;
    chan_cfg.dma_frame_num = 128;
    err = i2s_new_channel(&chan_cfg, &s_tx, &s_rx);
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
            .din = AUDIO_I2S_DIN,
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
        if (s_rx) i2s_del_channel(s_rx);
        s_tx = NULL;
        s_rx = NULL;
        return err;
    }
    err = i2s_channel_init_tdm_mode(s_rx, &tdm_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S microphone RX TDM init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2s_channel_enable(s_rx);
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

    /* ES7210 microphone ADC shares the board I2C bus and I2S0 TDM clock
     * with the ES8311 output. The current playback rate (24k) is retained.
     * WakeNet will later require either a coordinated 16k duplex rate or
     * an explicit conversion to 16k before AFE feed. */
    audio_codec_i2s_cfg_t rx_i2s_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = NULL,
        .rx_handle = s_rx,
    };
    const audio_codec_data_if_t *rx_data = audio_codec_new_i2s_data(&rx_i2s_cfg);
    audio_codec_i2c_cfg_t rx_i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = bus,
    };
    const audio_codec_ctrl_if_t *rx_ctrl = audio_codec_new_i2c_ctrl(&rx_i2c_cfg);
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = rx_ctrl,
        .master_mode = false,
        /* Mic selection is a bitmask. Some esp_codec_dev releases no longer
         * expose the ES7210_SEL_MICx macros; keep the original MIC1/MIC3
         * bits used by the legacy codec interface (bit 0 and bit 2). */
        .mic_selected = (1U << 0) | (1U << 2),
        .mclk_src = ES7210_MCLK_FROM_PAD,
    };
    const audio_codec_if_t *rx_codec = rx_ctrl ? es7210_codec_new(&es7210_cfg) : NULL;
    if (rx_data && rx_ctrl && rx_codec) {
        esp_codec_dev_cfg_t rx_dev_cfg = {
            .codec_if = rx_codec,
            .data_if = rx_data,
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
        };
        s_record = esp_codec_dev_new(&rx_dev_cfg);
    }
    if (s_record) {
        esp_codec_dev_sample_info_t mic_fs = {
            .sample_rate = AUDIO_SAMPLE_RATE,
            .channel = AUDIO_CHANNELS,
            .bits_per_sample = AUDIO_BITS,
        };
        if (esp_codec_dev_open(s_record, &mic_fs) == ESP_CODEC_DEV_OK) {
            if (esp_codec_dev_set_in_gain(s_record, AUDIO_MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
                ESP_LOGW(TAG, "microphone gain setting failed");
            }
            s_capture_ready = true;
            ESP_LOGI(TAG, "ES7210 microphone RX ready: %d Hz, %d ch, %d bit, DIN GPIO%d",
                     AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS, AUDIO_I2S_DIN);
        } else {
            ESP_LOGW(TAG, "ES7210 microphone codec open failed");
        }
    } else {
        ESP_LOGW(TAG, "ES7210 microphone interface initialization failed");
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

    return esp_codec_dev_set_out_vol(s_playback, percent) == ESP_CODEC_DEV_OK
               ? ESP_OK : ESP_FAIL;
}

uint8_t audio_service_get_volume(void)
{
    return s_volume;
}

void audio_service_stop(void)
{
    /* Reserved for stopping future media playback. No alert worker remains. */
}

/* Continuous capture is deliberately consumer-driven: a future WakeNet
 * worker owns the read loop and must not create another I2S driver. */
bool audio_service_capture_ready(void)
{
    return s_capture_ready;
}

esp_err_t audio_service_capture_read(void *pcm, size_t bytes)
{
    if (!s_capture_ready || !s_record) return ESP_ERR_INVALID_STATE;
    if (!pcm || bytes == 0 || bytes % (AUDIO_CHANNELS * (AUDIO_BITS / 8))) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_codec_dev_read(s_record, pcm, (int)bytes) == ESP_CODEC_DEV_OK
               ? ESP_OK : ESP_FAIL;
}

static void capture_diagnostic_task(void *arg)
{
    (void)arg;
    /* 1024 bytes = 256 frames at 24 kHz stereo S16. */
    int16_t *pcm = heap_caps_malloc(1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!pcm) {
        ESP_LOGE(TAG, "microphone diagnostic buffer allocation failed");
        s_capture_task_running = false;
        vTaskDelete(NULL);
        return;
    }
    uint64_t sum_abs = 0;
    uint32_t peak = 0, blocks = 0, failures = 0;
    TickType_t report = xTaskGetTickCount();
    while (s_capture_task_running && s_capture_ready) {
        if (audio_service_capture_read(pcm, 1024) == ESP_OK) {
            for (int i = 0; i < 512; i++) {
                int32_t n = pcm[i];
                uint32_t a = (uint32_t)(n < 0 ? -n : n);
                sum_abs += a;
                if (a > peak) peak = a;
            }
            blocks++;
        } else {
            failures++;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (xTaskGetTickCount() - report >= pdMS_TO_TICKS(2000)) {
            uint32_t avg = blocks ? (uint32_t)(sum_abs / (blocks * 512ULL)) : 0;
            ESP_LOGI(TAG, "MIC RX 24k stereo S16: blocks=%lu avg_abs=%lu peak=%lu read_fail=%lu",
                     (unsigned long)blocks, (unsigned long)avg,
                     (unsigned long)peak, (unsigned long)failures);
            report = xTaskGetTickCount();
            blocks = failures = peak = 0;
            sum_abs = 0;
        }
    }
    free(pcm);
    s_capture_task_running = false;
    vTaskDelete(NULL);
}

esp_err_t audio_service_capture_diagnostic_start(void)
{
    if (!s_capture_ready) return ESP_ERR_INVALID_STATE;
    if (s_capture_task_running) return ESP_ERR_INVALID_STATE;
    s_capture_task_running = true;
    if (xTaskCreate(capture_diagnostic_task, "mic_diag", 4096, NULL, 4, NULL) != pdPASS) {
        s_capture_task_running = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
