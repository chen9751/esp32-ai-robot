#include "board.h"
#include "ui_manager.h"

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "ai_robot";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AI Robot booting");

    esp_err_t err = board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board startup failed: %s", esp_err_to_name(err));
        return;
    }

    /* esp_lvgl_port owns the LVGL task. UI creation from app_main therefore
     * follows the same mutex contract as any future network/HA/AI task. */
    if (!board_display_lock(0)) {
        ESP_LOGE(TAG, "unable to lock LVGL runtime");
        return;
    }
    ui_init();
    board_display_unlock();

    /* Reveal the screen only after the first UI tree exists, avoiding the
     * white/garbage flash common during LCD boot. */
    err = board_backlight_set_percent(60);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "backlight setup failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "UI ready at logical resolution 640x172");
}
