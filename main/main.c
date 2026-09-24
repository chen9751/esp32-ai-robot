#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ai_robot";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 AI Robot booting");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
