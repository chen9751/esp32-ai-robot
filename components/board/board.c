#include "board.h"
#include "board_config.h"

#include <stddef.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "board";

#define LVGL_TICK_PERIOD_MS     5
#define LVGL_TASK_MIN_DELAY_MS  5
#define LVGL_TASK_MAX_DELAY_MS  100
#define LVGL_TASK_STACK_SIZE    (8 * 1024)
#define LVGL_TASK_PRIORITY      4

static i2c_master_bus_handle_t s_system_i2c = NULL;
static i2c_master_bus_handle_t s_touch_i2c = NULL;
static esp_io_expander_handle_t s_io_expander = NULL;
static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_panel_io_handle_t s_touch_io = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static i2c_master_dev_handle_t s_imu_dev = NULL;
static lv_display_t *s_display = NULL;
static lv_indev_t *s_touch_indev = NULL;

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static SemaphoreHandle_t s_flush_done = NULL;
static esp_timer_handle_t s_lvgl_tick_timer = NULL;
static TaskHandle_t s_lvgl_task = NULL;

static uint8_t *s_frame_buffer_1 = NULL;
static uint8_t *s_frame_buffer_2 = NULL;
static uint8_t *s_rotate_buffer = NULL;
static uint16_t *s_dma_buffer = NULL;

static bool s_lvgl_started = false;
static bool s_display_ready = false;
static bool s_backlight_ready = false;
static uint8_t s_backlight_percent = 0;
static uint32_t s_flush_count = 0;

#define QMI8658_ADDR              0x6B
#define QMI8658_REG_WHOAMI        0x00
#define QMI8658_WHOAMI_VALUE      0x05
#define QMI8658_REG_CTRL1         0x02
#define QMI8658_REG_CTRL2         0x03
#define QMI8658_REG_CTRL7         0x08
#define QMI8658_REG_AX_L          0x35
#define QMI8658_ORIENTATION_MIN   7000
#define QMI8658_ORIENTATION_BIAS  2500

/* Waveshare's V2 example performs hardware reset through TCA9554 and then
 * supplies sleep-out/display-on as the AXS15231B custom initialization list. */
static const axs15231b_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0x11, NULL, 0, 100},
    {0x29, NULL, 0, 100},
};

static esp_err_t init_backlight_pwm(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_3,
        .freq_hz = 50000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) return err;

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = BOARD_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_3,
        /* Waveshare V2 backlight PWM is active-low: duty 0 = brightest,
         * duty 255 = off. Keep it off until the first frame is ready. */
        .duty = 255,
        .hpoint = 0,
        .flags = {
            .output_invert = 0,
        },
    };
    err = ledc_channel_config(&channel_cfg);
    if (err == ESP_OK) {
        s_backlight_ready = true;
        s_backlight_percent = 0;
    }
    return err;
}

static esp_err_t init_i2c_bus(i2c_port_num_t port,
                              gpio_num_t scl,
                              gpio_num_t sda,
                              i2c_master_bus_handle_t *out_bus)
{
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = port,
        .scl_io_num = scl,
        .sda_io_num = sda,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    return i2c_new_master_bus(&cfg, out_bus);
}

static esp_err_t init_io_expander(void)
{
    esp_err_t err = esp_io_expander_new_i2c_tca9554(
        s_system_i2c,
        ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
        &s_io_expander);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_dir(s_io_expander,
                                  BOARD_EXIO_PIN_TOUCH_INT,
                                  IO_EXPANDER_INPUT);
    if (err != ESP_OK) return err;

    const uint32_t outputs = BOARD_EXIO_PIN_BL_EN |
                             BOARD_EXIO_PIN_LCD_RST |
                             BOARD_EXIO_PIN_SYS_EN;
    err = esp_io_expander_set_dir(s_io_expander, outputs, IO_EXPANDER_OUTPUT);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_level(s_io_expander, BOARD_EXIO_PIN_BL_EN, 0);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_level(s_io_expander, BOARD_EXIO_PIN_SYS_EN, 1);
    if (err != ESP_OK) return err;

    return esp_io_expander_set_level(s_io_expander, BOARD_EXIO_PIN_LCD_RST, 1);
}

static esp_err_t reset_lcd(void)
{
    esp_err_t err = esp_io_expander_set_level(s_io_expander,
                                               BOARD_EXIO_PIN_LCD_RST, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(30));

    err = esp_io_expander_set_level(s_io_expander,
                                    BOARD_EXIO_PIN_LCD_RST, 0);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(250));

    err = esp_io_expander_set_level(s_io_expander,
                                    BOARD_EXIO_PIN_LCD_RST, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(30));
    return ESP_OK;
}

static bool lcd_color_transfer_done(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;

    BaseType_t high_task_awoken = pdFALSE;
    if (s_flush_done != NULL) {
        xSemaphoreGiveFromISR(s_flush_done, &high_task_awoken);
    }
    return high_task_awoken == pdTRUE;
}

static esp_err_t init_lcd_panel(void)
{
    const spi_bus_config_t bus_cfg = AXS15231B_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_PIN_PCLK,
        BOARD_LCD_PIN_DATA0,
        BOARD_LCD_PIN_DATA1,
        BOARD_LCD_PIN_DATA2,
        BOARD_LCD_PIN_DATA3,
        BOARD_LCD_DMA_BYTES);

    esp_err_t err = spi_bus_initialize(BOARD_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    const esp_lcd_panel_io_spi_config_t io_cfg =
        AXS15231B_PANEL_IO_QSPI_CONFIG(BOARD_LCD_PIN_CS,
                                      lcd_color_transfer_done,
                                      NULL);
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST,
                                   &io_cfg,
                                   &s_lcd_io);
    if (err != ESP_OK) return err;

    const axs15231b_vendor_config_t vendor_cfg = {
        .init_cmds = s_lcd_init_cmds,
        .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };

    err = esp_lcd_new_panel_axs15231b(s_lcd_io, &panel_cfg, &s_lcd_panel);
    if (err != ESP_OK) return err;

    err = reset_lcd();
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_init(s_lcd_panel);
    if (err != ESP_OK) return err;

    /* The Waveshare V2 init sequence already sends 0x29 (Display ON).
     * Do not call esp_lcd_panel_disp_on_off(panel, true) here: in
     * esp_lcd_axs15231b 2.1.1 the driver's callback interprets its boolean
     * as "off", while ESP-IDF's public API defines true as "on". Calling the
     * public API with true therefore sends DISPOFF and leaves a fully working
     * backlight/flush path showing only black. */
    return ESP_OK;
}

static esp_err_t qmi8658_write_reg(uint8_t reg, uint8_t value)
{
    if (s_imu_dev == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t packet[2] = { reg, value };
    return i2c_master_transmit(s_imu_dev, packet, sizeof(packet), 100);
}

static esp_err_t qmi8658_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (s_imu_dev == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(s_imu_dev, &reg, 1, data, len, 100);
}

static esp_err_t init_imu(void)
{
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(s_system_i2c, &dev_cfg, &s_imu_dev);
    if (err != ESP_OK) return err;

    uint8_t whoami = 0;
    err = qmi8658_read(QMI8658_REG_WHOAMI, &whoami, 1);
    if (err != ESP_OK) return err;
    if (whoami != QMI8658_WHOAMI_VALUE) {
        ESP_LOGE(TAG, "QMI8658 WHOAMI mismatch: 0x%02x", whoami);
        return ESP_ERR_NOT_FOUND;
    }

    /* Match Waveshare's verified setup closely enough for orientation:
     * CTRL1 bit6 enables register auto-increment.
     * CTRL2 = 4G range + 1000 Hz ODR.
     * CTRL7 bit0 enables accelerometer output. */
    err = qmi8658_write_reg(QMI8658_REG_CTRL1, 0x40);
    if (err != ESP_OK) return err;
    err = qmi8658_write_reg(QMI8658_REG_CTRL2, 0x13);
    if (err != ESP_OK) return err;

    uint8_t ctrl7 = 0;
    err = qmi8658_read(QMI8658_REG_CTRL7, &ctrl7, 1);
    if (err != ESP_OK) return err;
    err = qmi8658_write_reg(QMI8658_REG_CTRL7, (uint8_t)(ctrl7 | 0x01));
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "QMI8658 orientation sensor ready");
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    esp_lcd_panel_io_i2c_config_t touch_io_cfg =
        ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG_EX(BOARD_TOUCH_I2C_HZ);
    esp_err_t err = esp_lcd_new_panel_io_i2c(s_touch_i2c,
                                             &touch_io_cfg,
                                             &s_touch_io);
    if (err != ESP_OK) return err;

    const esp_lcd_touch_config_t touch_cfg = {
        .x_max = BOARD_TOUCH_RAW_X_MAX,
        .y_max = BOARD_TOUCH_RAW_Y_MAX,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = BOARD_TOUCH_SWAP_XY,
            .mirror_x = BOARD_TOUCH_MIRROR_X,
            .mirror_y = BOARD_TOUCH_MIRROR_Y,
        },
    };

    return esp_lcd_touch_new_i2c_axs15231b(s_touch_io, &touch_cfg, &s_touch);
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t point_count = 0;

    esp_err_t err = esp_lcd_touch_read_data(s_touch);
    if (err != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    const bool pressed = esp_lcd_touch_get_coordinates(s_touch,
                                                        &x,
                                                        &y,
                                                        NULL,
                                                        &point_count,
                                                        1);
    if (pressed && point_count > 0) {
        /* Touch driver coordinates are in the panel's native 172x640 space.
         * The final display mapping is:
         *     nx = 171 - ly
         *     ny = lx
         * Apply the inverse transform so touch follows the 180-degree-corrected
         * 640x172 UI orientation. */
        int32_t logical_x = (int32_t)y;
        int32_t logical_y = (BOARD_UI_V_RES - 1) - (int32_t)x;
        if (logical_x < 0) logical_x = 0;
        if (logical_x >= BOARD_UI_H_RES) logical_x = BOARD_UI_H_RES - 1;
        if (logical_y < 0) logical_y = 0;
        if (logical_y >= BOARD_UI_V_RES) logical_y = BOARD_UI_V_RES - 1;

        data->point.x = logical_x;
        data->point.y = logical_y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void display_flush_cb(lv_display_t *display,
                             const lv_area_t *area,
                             uint8_t *color_map)
{
    const uint32_t flush_id = ++s_flush_count;
    if (flush_id <= 3) {
        ESP_LOGI(TAG, "LVGL flush #%u area=(%ld,%ld)-(%ld,%ld)",
                 (unsigned)flush_id,
                 (long)area->x1, (long)area->y1,
                 (long)area->x2, (long)area->y2);
    }

    if (s_lcd_panel == NULL || s_dma_buffer == NULL || s_rotate_buffer == NULL) {
        lv_display_flush_ready(display);
        return;
    }

    /* LVGL always renders a complete 640x172 logical frame. Do not use
     * LVGL's display-rotation path here: this board is physically 172x640,
     * while the product UI is permanently 640x172. Convert the framebuffer
     * explicitly so its memory layout is deterministic across LVGL versions.
     *
     * Mapping (90 degrees):
     *     native_x = logical_y
     *     native_y = 639 - logical_x
     *
     * AXS15231B expects RGB565 bytes in wire order, so swap each 16-bit
     * pixel while copying into the native framebuffer. */
    const int32_t logical_width = lv_area_get_width(area);
    const int32_t logical_height = lv_area_get_height(area);
    if (logical_width != BOARD_UI_H_RES ||
        logical_height != BOARD_UI_V_RES ||
        area->x1 != 0 || area->y1 != 0) {
        ESP_LOGE(TAG, "unexpected LVGL FULL area: (%ld,%ld)-(%ld,%ld)",
                 (long)area->x1, (long)area->y1,
                 (long)area->x2, (long)area->y2);
        lv_display_flush_ready(display);
        return;
    }

    const uint16_t *logical_pixels = (const uint16_t *)color_map;
    uint16_t *native_pixels_mut = (uint16_t *)s_rotate_buffer;

    for (int32_t logical_y = 0; logical_y < BOARD_UI_V_RES; ++logical_y) {
        const size_t src_row = (size_t)logical_y * BOARD_UI_H_RES;
        for (int32_t logical_x = 0; logical_x < BOARD_UI_H_RES; ++logical_x) {
            const uint16_t pixel = logical_pixels[src_row + (size_t)logical_x];
            const uint16_t wire_pixel =
                (uint16_t)((pixel << 8) | (pixel >> 8));
            const int32_t native_x = (BOARD_UI_V_RES - 1) - logical_y;
            const int32_t native_y = logical_x;
            native_pixels_mut[(size_t)native_y * BOARD_LCD_NATIVE_H_RES +
                              (size_t)native_x] = wire_pixel;
        }
    }

    /* Drain a stale completion token if a previous aborted flush left one. */
    while (xSemaphoreTake(s_flush_done, 0) == pdTRUE) {
    }

    const uint16_t *native_pixels = (const uint16_t *)s_rotate_buffer;
    const size_t pixels_per_chunk = BOARD_LCD_DMA_BYTES / sizeof(uint16_t);

    bool flush_ok = true;
    for (int chunk = 0; chunk < BOARD_LCD_DMA_CHUNKS; ++chunk) {
        memcpy(s_dma_buffer,
               native_pixels + ((size_t)chunk * pixels_per_chunk),
               BOARD_LCD_DMA_BYTES);

        const int y_start = chunk * BOARD_LCD_DMA_ROWS;
        const int y_end = y_start + BOARD_LCD_DMA_ROWS;
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_lcd_panel,
                                                   0,
                                                   y_start,
                                                   BOARD_LCD_NATIVE_H_RES,
                                                   y_end,
                                                   s_dma_buffer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD flush chunk %d failed: %s",
                     chunk, esp_err_to_name(err));
            flush_ok = false;
            break;
        }

        if (xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "LCD flush chunk %d timed out", chunk);
            flush_ok = false;
            break;
        }
    }

    if (flush_id <= 3) {
        ESP_LOGI(TAG, "LVGL flush #%u %s",
                 (unsigned)flush_id, flush_ok ? "complete" : "incomplete");
    }

    lv_display_flush_ready(display);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    for (;;) {
        uint32_t delay_ms = LVGL_TASK_MAX_DELAY_MS;
        if (board_display_lock(0)) {
            delay_ms = lv_timer_handler();
            board_display_unlock();
        }

        if (delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        else if (delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            delay_ms = LVGL_TASK_MAX_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static esp_err_t init_lvgl(void)
{
    s_lvgl_mutex = xSemaphoreCreateMutex();
    s_flush_done = xSemaphoreCreateBinary();
    if (s_lvgl_mutex == NULL || s_flush_done == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_frame_buffer_1 = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_frame_buffer_2 = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_rotate_buffer = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_dma_buffer = heap_caps_malloc(BOARD_LCD_DMA_BYTES,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (s_frame_buffer_1 == NULL ||
        s_frame_buffer_2 == NULL ||
        s_rotate_buffer == NULL ||
        s_dma_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_init();

    s_display = lv_display_create(BOARD_UI_H_RES,
                                  BOARD_UI_V_RES);
    if (s_display == NULL) return ESP_ERR_NO_MEM;

    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, display_flush_cb);
    lv_display_set_buffers(s_display,
                           s_frame_buffer_1,
                           s_frame_buffer_2,
                           BOARD_LCD_FRAME_BYTES,
                           LV_DISPLAY_RENDER_MODE_FULL);
    s_touch_indev = lv_indev_create();
    if (s_touch_indev == NULL) return ESP_ERR_NO_MEM;
    lv_indev_set_type(s_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch_indev, touch_read_cb);
    lv_indev_set_display(s_touch_indev, s_display);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    esp_err_t err = esp_timer_create(&tick_args, &s_lvgl_tick_timer);
    if (err != ESP_OK) return err;
    err = esp_timer_start_periodic(s_lvgl_tick_timer,
                                   LVGL_TICK_PERIOD_MS * 1000ULL);
    if (err != ESP_OK) return err;

    s_lvgl_started = true;
    if (xTaskCreate(lvgl_task,
                    "lvgl",
                    LVGL_TASK_STACK_SIZE,
                    NULL,
                    LVGL_TASK_PRIORITY,
                    &s_lvgl_task) != pdPASS) {
        s_lvgl_started = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG,
             "LVGL 640x172 full-frame runtime: 2x%u-byte render + %u-byte native-map + %u-byte DMA",
             (unsigned)BOARD_LCD_FRAME_BYTES,
             (unsigned)BOARD_LCD_FRAME_BYTES,
             (unsigned)BOARD_LCD_DMA_BYTES);
    return ESP_OK;
}

esp_err_t board_init(void)
{
    if (s_display_ready) return ESP_OK;

    ESP_LOGI(TAG, "Waveshare ESP32-S3-Touch-LCD-3.49 V2 init");
    ESP_LOGI(TAG, "panel %dx%d -> UI %dx%d",
             BOARD_LCD_NATIVE_H_RES,
             BOARD_LCD_NATIVE_V_RES,
             BOARD_UI_H_RES,
             BOARD_UI_V_RES);

    esp_err_t err = init_backlight_pwm();
    if (err != ESP_OK) goto fail;

    err = init_i2c_bus(BOARD_SYS_I2C_PORT,
                       BOARD_SYS_I2C_SCL,
                       BOARD_SYS_I2C_SDA,
                       &s_system_i2c);
    if (err != ESP_OK) goto fail;

    err = init_io_expander();
    if (err != ESP_OK) goto fail;

    /* IMU is useful but not required to render the UI. Keep booting if the
     * sensor is temporarily unavailable and let the remote page stay at its
     * last orientation. */
    err = init_imu();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 init failed: %s", esp_err_to_name(err));
        s_imu_dev = NULL;
    }

    /* Create the transfer semaphore before panel IO registers its callback. */
    s_flush_done = xSemaphoreCreateBinary();
    if (s_flush_done == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    err = init_lcd_panel();
    if (err != ESP_OK) goto fail;

    err = init_i2c_bus(BOARD_TOUCH_I2C_PORT,
                       BOARD_TOUCH_I2C_SCL,
                       BOARD_TOUCH_I2C_SDA,
                       &s_touch_i2c);
    if (err != ESP_OK) goto fail;

    err = init_touch();
    if (err != ESP_OK) goto fail;

    /* init_lvgl reuses the flush semaphore created before panel IO setup. */
    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (s_lvgl_mutex == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    s_frame_buffer_1 = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_frame_buffer_2 = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_rotate_buffer = heap_caps_malloc(BOARD_LCD_FRAME_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_dma_buffer = heap_caps_malloc(BOARD_LCD_DMA_BYTES,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (s_frame_buffer_1 == NULL ||
        s_frame_buffer_2 == NULL ||
        s_rotate_buffer == NULL ||
        s_dma_buffer == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    lv_init();
    s_display = lv_display_create(BOARD_UI_H_RES,
                                  BOARD_UI_V_RES);
    if (s_display == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, display_flush_cb);
    lv_display_set_buffers(s_display,
                           s_frame_buffer_1,
                           s_frame_buffer_2,
                           BOARD_LCD_FRAME_BYTES,
                           LV_DISPLAY_RENDER_MODE_FULL);
    s_touch_indev = lv_indev_create();
    if (s_touch_indev == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    lv_indev_set_type(s_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch_indev, touch_read_cb);
    lv_indev_set_display(s_touch_indev, s_display);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    err = esp_timer_create(&tick_args, &s_lvgl_tick_timer);
    if (err != ESP_OK) goto fail;
    err = esp_timer_start_periodic(s_lvgl_tick_timer,
                                   LVGL_TICK_PERIOD_MS * 1000ULL);
    if (err != ESP_OK) goto fail;

    s_lvgl_started = true;
    if (xTaskCreate(lvgl_task,
                    "lvgl",
                    LVGL_TASK_STACK_SIZE,
                    NULL,
                    LVGL_TASK_PRIORITY,
                    &s_lvgl_task) != pdPASS) {
        s_lvgl_started = false;
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    s_display_ready = true;
    ESP_LOGI(TAG,
             "display/touch/LVGL ready: logical 640x172, manual native mapping; backlight off");
    ESP_LOGI(TAG,
             "LVGL buffers: 2x%u logical frame + %u native frame + %u DMA bytes",
             (unsigned)BOARD_LCD_FRAME_BYTES,
             (unsigned)BOARD_LCD_FRAME_BYTES,
             (unsigned)BOARD_LCD_DMA_BYTES);
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "board init failed: %s", esp_err_to_name(err));
    s_display_ready = false;
    return err;
}

bool board_display_ready(void)
{
    return s_display_ready;
}

bool board_display_lock(uint32_t timeout_ms)
{
    if (!s_lvgl_started || s_lvgl_mutex == NULL) return false;
    const TickType_t wait_ticks = timeout_ms == 0
                                      ? portMAX_DELAY
                                      : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(s_lvgl_mutex, wait_ticks) == pdTRUE;
}

void board_display_unlock(void)
{
    if (s_lvgl_mutex != NULL) {
        xSemaphoreGive(s_lvgl_mutex);
    }
}

esp_err_t board_backlight_set_percent(uint8_t percent)
{
    if (!s_backlight_ready || s_io_expander == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (percent > 100) percent = 100;

    /* GPIO42 is active-low, but the LED driver is not visually linear across
     * the full 0..255 electrical range. On this V2 panel the lower ~30% of
     * raw drive is already below the visible threshold. Preserve 0% as true
     * off, then map user 1..100% across the complete visible range so values
     * such as 10%, 20% and 30% remain distinct instead of collapsing to off. */
    uint32_t brightness = 0;
    if (percent > 0) {
        const uint32_t min_visible = 90U; /* ~35% raw drive, above panel cutoff */
        brightness = min_visible +
                     ((uint32_t)(percent - 1U) * (255U - min_visible) + 49U) / 99U;
    }
    const uint32_t duty = 255U - brightness;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    if (err != ESP_OK) return err;
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_level(s_io_expander,
                                    BOARD_EXIO_PIN_BL_EN,
                                    percent > 0 ? 1 : 0);
    if (err != ESP_OK) return err;

    s_backlight_percent = percent;
    ESP_LOGI(TAG, "backlight: PWM duty=%u/255, BL_EN=%d, percent=%u",
             (unsigned)duty, percent > 0 ? 1 : 0, (unsigned)percent);
    return ESP_OK;
}

uint8_t board_backlight_get_percent(void)
{
    return s_backlight_percent;
}

esp_err_t board_imu_get_orientation(board_orientation_t *orientation)
{
    if (orientation == NULL) return ESP_ERR_INVALID_ARG;
    *orientation = BOARD_ORIENTATION_UNKNOWN;
    if (s_imu_dev == NULL) return ESP_ERR_INVALID_STATE;

    uint8_t raw[6] = {0};
    esp_err_t err = qmi8658_read(QMI8658_REG_AX_L, raw, sizeof(raw));
    if (err != ESP_OK) return err;

    const int16_t ax = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    const int16_t ay = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    const int32_t abs_x = ax < 0 ? -(int32_t)ax : (int32_t)ax;
    const int32_t abs_y = ay < 0 ? -(int32_t)ay : (int32_t)ay;

    /* Flat-on-table means gravity is mostly Z. In that state don't invent a
     * new screen orientation; the UI keeps the last stable one. */
    if (abs_x < QMI8658_ORIENTATION_MIN &&
        abs_y < QMI8658_ORIENTATION_MIN) {
        return ESP_OK;
    }

    if (abs_y > abs_x + QMI8658_ORIENTATION_BIAS) {
        *orientation = ay > 0
                           ? BOARD_ORIENTATION_LANDSCAPE
                           : BOARD_ORIENTATION_LANDSCAPE_INVERTED;
    }
    else if (abs_x > abs_y + QMI8658_ORIENTATION_BIAS) {
        *orientation = ax > 0
                           ? BOARD_ORIENTATION_PORTRAIT_RIGHT
                           : BOARD_ORIENTATION_PORTRAIT_LEFT;
    }

    return ESP_OK;
}

static uint16_t rgb565_to_wire(uint16_t color)
{
    return (uint16_t)((color << 8) | (color >> 8));
}

static esp_err_t fill_panel_rgb565(uint16_t color)
{
    if (s_lcd_panel == NULL || s_dma_buffer == NULL || s_flush_done == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint16_t wire_color = rgb565_to_wire(color);
    const size_t pixels_per_chunk = BOARD_LCD_DMA_BYTES / sizeof(uint16_t);

    for (size_t i = 0; i < pixels_per_chunk; ++i) {
        s_dma_buffer[i] = wire_color;
    }

    while (xSemaphoreTake(s_flush_done, 0) == pdTRUE) {
    }

    for (int chunk = 0; chunk < BOARD_LCD_DMA_CHUNKS; ++chunk) {
        const int y_start = chunk * BOARD_LCD_DMA_ROWS;
        const int y_end = y_start + BOARD_LCD_DMA_ROWS;

        esp_err_t err = esp_lcd_panel_draw_bitmap(s_lcd_panel,
                                                   0,
                                                   y_start,
                                                   BOARD_LCD_NATIVE_H_RES,
                                                   y_end,
                                                   s_dma_buffer);
        if (err != ESP_OK) {
            return err;
        }

        if (xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(200)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }

    return ESP_OK;
}

esp_err_t board_display_run_color_test(void)
{
    if (!s_display_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!board_display_lock(0)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = board_backlight_set_percent(100);
    if (err != ESP_OK) {
        board_display_unlock();
        return err;
    }

    static const struct {
        const char *name;
        uint16_t color;
    } test_frames[] = {
        { "RED",   0xF800 },
        { "GREEN", 0x07E0 },
        { "BLUE",  0x001F },
        { "WHITE", 0xFFFF },
        { "BLACK", 0x0000 },
    };

    for (size_t i = 0; i < sizeof(test_frames) / sizeof(test_frames[0]); ++i) {
        ESP_LOGI(TAG, "direct LCD color test: %s", test_frames[i].name);
        err = fill_panel_rgb565(test_frames[i].color);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "direct LCD color test failed on %s: %s",
                     test_frames[i].name, esp_err_to_name(err));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(i + 1 ==
                                 sizeof(test_frames) / sizeof(test_frames[0])
                                 ? 250
                                 : 1200));
    }

    board_display_unlock();
    return err;
}
