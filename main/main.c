#include "board.h"
#include "ui_manager.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ai_robot";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AI Robot booting");

    esp_err_t err = board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board startup failed: %s", esp_err_to_name(err));
        return;
    }

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
    err = board_backlight_set_percent(60);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "backlight setup failed: %s", esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG, "backlight enabled at %u%%",
                 (unsigned)board_backlight_get_percent());
    }

    ESP_LOGI(TAG, "UI ready at logical resolution 640x172");
}
