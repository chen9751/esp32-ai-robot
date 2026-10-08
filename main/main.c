#include "board.h"
#include "ui_manager.h"
#include "network_service.h"
#include "ha_lights.h"
#include "ha_devices.h"
#include "bluetooth_service.h"
#include "audio_service.h"
#include "voice_wakeup.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "ai_robot";

static void log_memory(const char *stage)
{
    ESP_LOGI(TAG,
             "MEM %s: internal=%u largest=%u DMA=%u PSRAM=%u",
             stage,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

#define PHYS_KEY_CUSTOM GPIO_NUM_0
#define PHYS_KEY_PWR    GPIO_NUM_16
#define PHYS_KEY_POLL_MS 20
#define PHYS_KEY_DEBOUNCE_MS 60
#define PHYS_PWR_LONG_MS 1500

static void handle_custom_key(void)
{
    /* BOOT is safe to read as a normal input after startup. It acts as the
     * product back/acknowledge key; RESET/CHIP_PU is intentionally untouched. */
    if (board_display_lock(0)) {
        ui_handle_back_action();
        board_display_unlock();
    }
}

static void enter_power_off(void)
{
    ESP_LOGI(TAG, "PWR long press: shutting down");
    audio_service_stop();
    (void)board_power_off();

    /* On battery, SYS_EN normally removes power before this point. If USB is
     * still supplying the ESP32-S3, wait for the key release and enter deep
     * sleep. GPIO16 is an RTC-capable pin on ESP32-S3, so the next active-low
     * PWR press wakes the device. */
    while (gpio_get_level(PHYS_KEY_PWR) == 0) {
        vTaskDelay(pdMS_TO_TICKS(PHYS_KEY_POLL_MS));
    }

    esp_err_t err = esp_sleep_enable_ext1_wakeup_io(
        1ULL << PHYS_KEY_PWR, ESP_EXT1_WAKEUP_ANY_LOW);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PWR wake source setup failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "entering deep sleep fallback");
    esp_deep_sleep_start();
}

static void physical_buttons_task(void *arg)
{
    (void)arg;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PHYS_KEY_CUSTOM) | (1ULL << PHYS_KEY_PWR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "physical button GPIO setup failed");
        vTaskDelete(NULL);
        return;
    }

    bool custom_down = false;
    bool pwr_down = false;
    bool pwr_fired = false;
    TickType_t custom_changed = xTaskGetTickCount();
    TickType_t pwr_pressed_at = 0;
    TickType_t next_memory_report = xTaskGetTickCount() + pdMS_TO_TICKS(60000);

    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        const bool custom_now = gpio_get_level(PHYS_KEY_CUSTOM) == 0;
        const bool pwr_now = gpio_get_level(PHYS_KEY_PWR) == 0;

        if (custom_now != custom_down &&
            (now - custom_changed) >= pdMS_TO_TICKS(PHYS_KEY_DEBOUNCE_MS)) {
            custom_down = custom_now;
            custom_changed = now;
            if (custom_down) {
                ESP_LOGI(TAG, "custom/BOOT key pressed");
                handle_custom_key();
            }
        }

        if (pwr_now && !pwr_down) {
            pwr_down = true;
            pwr_fired = false;
            pwr_pressed_at = now;
        }
        else if (!pwr_now && pwr_down) {
            pwr_down = false;
            pwr_fired = false;
        }

        if (pwr_down && !pwr_fired &&
            (now - pwr_pressed_at) >= pdMS_TO_TICKS(PHYS_PWR_LONG_MS)) {
            pwr_fired = true;
            enter_power_off();
        }

        if ((int32_t)(now - next_memory_report) >= 0) {
            ESP_LOGI(TAG,
                     "MEM runtime: internal=%u min_internal=%u largest=%u DMA=%u PSRAM=%u",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            next_memory_report = now + pdMS_TO_TICKS(60000);
        }
        vTaskDelay(pdMS_TO_TICKS(PHYS_KEY_POLL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AI Robot booting");

    /* Bluetooth is opt-in from Settings. Keep the controller and NimBLE
     * uninitialized at boot to preserve internal DRAM for Wi-Fi and AFE. */
    log_memory("before board");
    esp_err_t err = board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board startup failed: %s", esp_err_to_name(err));
        return;
    }

    log_memory("after board");

    /* LVGL is owned by the board runtime task. Build the complete first UI
     * tree under the shared mutex, then force one synchronous refresh before
     * revealing the backlight. This makes first-frame delivery deterministic
     * on real hardware instead of waiting for the periodic refresh timer. */
    if (!board_display_lock(0)) {
        ESP_LOGE(TAG, "unable to lock LVGL runtime");
        return;
    }

    ui_init();
    lv_obj_invalidate(lv_screen_active());

    ESP_LOGI(TAG, "forcing first LVGL frame");
    lv_refr_now(NULL);
    ESP_LOGI(TAG, "first LVGL frame returned");

    board_display_unlock();

    /* Reveal the screen only after the first frame has been pushed. Hardware
     * bring-up is complete, so start at the product default brightness. */
    err = board_backlight_set_percent(20);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "backlight setup failed: %s", esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG, "backlight enabled at %u%%",
                 (unsigned)board_backlight_get_percent());
    }

    log_memory("after UI");

    /* I2S DMA requires contiguous internal/DMA-capable RAM. Reserve the
     * small audio DMA ring before Wi-Fi fragments the remaining internal
     * heap. Wi-Fi itself is configured with a reduced buffer profile in
     * network_service_init(), appropriate for this control-panel workload. */
    err = audio_service_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "audio service init failed: %s", esp_err_to_name(err));
    }

    log_memory("after audio");

    /* Wi-Fi is initialized before AFE so the radio driver gets its
     * contiguous internal memory while it is still available. */
    ESP_LOGI(TAG, "starting Wi-Fi before WakeNet AFE");
    err = network_service_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "network service init failed: %s", esp_err_to_name(err));
    }

    /* HA state polling and command queues run independently of LVGL. */
    esp_err_t ha_lights_err = ha_lights_init();
    if (ha_lights_err != ESP_OK) {
        ESP_LOGW(TAG, "HA lights init failed: %s", esp_err_to_name(ha_lights_err));
    }
    esp_err_t ha_devices_err = ha_devices_init();
    if (ha_devices_err != ESP_OK) {
        ESP_LOGW(TAG, "HA devices init failed: %s", esp_err_to_name(ha_devices_err));
    }

    log_memory("after HA init");
    log_memory("after Wi-Fi init");

    /* Microphone has one reader: WakeNet AFE. Diagnostic capture must
     * not run simultaneously or steal audio frames. */
    if (audio_service_capture_ready()) {
        esp_err_t wake_err = voice_wakeup_start(NULL, NULL);
        if (wake_err != ESP_OK) {
            ESP_LOGW(TAG, "WakeNet startup failed: %s", esp_err_to_name(wake_err));
        }
    }

    log_memory("after WakeNet start");

    if (xTaskCreate(physical_buttons_task, "phys_buttons", 3072,
                    NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "physical button task start failed");
    }

    ESP_LOGI(TAG, "UI ready at logical resolution 640x172");
}
