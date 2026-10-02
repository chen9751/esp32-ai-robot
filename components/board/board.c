#include "board.h"
#include "board_config.h"

#include <stddef.h>

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "board";

static i2c_master_bus_handle_t s_system_i2c = NULL;
static i2c_master_bus_handle_t s_touch_i2c = NULL;
static esp_io_expander_handle_t s_io_expander = NULL;
static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_panel_io_handle_t s_touch_io = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static lv_display_t *s_display = NULL;
static lv_indev_t *s_touch_indev = NULL;

static bool s_lvgl_started = false;
static bool s_display_ready = false;
static bool s_backlight_ready = false;
static uint8_t s_backlight_percent = 0;

/* Waveshare's V2 LVGL example performs a hardware reset through TCA9554 and
 * then sends only sleep-out/display-on as panel-specific init commands. */
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
        .duty = 0,
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

    /* Keep the panel dark while UI objects are being created. */
    err = esp_io_expander_set_level(s_io_expander, BOARD_EXIO_PIN_BL_EN, 0);
    if (err != ESP_OK) return err;

    /* The battery-power example keeps SYS_EN asserted while the product is on. */
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

static esp_err_t init_lcd_panel(void)
{
    const spi_bus_config_t bus_cfg = AXS15231B_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_PIN_PCLK,
        BOARD_LCD_PIN_DATA0,
        BOARD_LCD_PIN_DATA1,
        BOARD_LCD_PIN_DATA2,
        BOARD_LCD_PIN_DATA3,
        BOARD_LCD_NATIVE_H_RES * 64 * sizeof(uint16_t));

    esp_err_t err = spi_bus_initialize(BOARD_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    const esp_lcd_panel_io_spi_config_t io_cfg =
        AXS15231B_PANEL_IO_QSPI_CONFIG(BOARD_LCD_PIN_CS, NULL, NULL);
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

    return esp_lcd_panel_disp_on_off(s_lcd_panel, true);
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
        /* These maxima describe the controller's raw orientation. The flags
         * below first map it into the LCD's native 172x640 coordinates. */
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

static esp_err_t init_lvgl(void)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    /* UI pages and transitions create a fair number of LVGL objects. Keep a
     * little more headroom than the port default without moving the task stack
     * into PSRAM. */
    port_cfg.task_stack = 8192;
    port_cfg.task_priority = 4;
    port_cfg.task_max_sleep_ms = 100;

    esp_err_t err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) return err;
    s_lvgl_started = true;

    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = s_lcd_io,
        .panel_handle = s_lcd_panel,
        .buffer_size = BOARD_LVGL_DRAW_BUF_PIXELS,
        .double_buffer = true,
        .hres = BOARD_LCD_NATIVE_H_RES,
        .vres = BOARD_LCD_NATIVE_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = true,
            .swap_bytes = true,
            .full_refresh = false,
            .direct_mode = false,
        },
    };

    s_display = lvgl_port_add_disp(&display_cfg);
    if (s_display == NULL) return ESP_ERR_NO_MEM;

    /* The physical panel remains 172x640; this is the single rotation point
     * that exposes the project's 640x172 logical canvas to every UI page. */
    if (!lvgl_port_lock(0)) return ESP_ERR_TIMEOUT;
    lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_90);
    lvgl_port_unlock();

    /* esp_lvgl_port 2.4.x does not yet expose touch scaling. The V2 panel is
     * already 1:1 after the native-coordinate transform above. */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = s_display,
        .handle = s_touch,
    };
    s_touch_indev = lvgl_port_add_touch(&touch_cfg);
    if (s_touch_indev == NULL) return ESP_ERR_NO_MEM;

    return ESP_OK;
}

esp_err_t board_init(void)
{
    if (s_display_ready) return ESP_OK;

    ESP_LOGI(TAG, "Waveshare ESP32-S3-Touch-LCD-3.49 V2 init");
    ESP_LOGI(TAG, "panel %dx%d -> UI %dx%d",
             BOARD_LCD_NATIVE_H_RES, BOARD_LCD_NATIVE_V_RES,
             BOARD_UI_H_RES, BOARD_UI_V_RES);

    esp_err_t err = init_backlight_pwm();
    if (err != ESP_OK) goto fail;

    err = init_i2c_bus(BOARD_SYS_I2C_PORT,
                       BOARD_SYS_I2C_SCL,
                       BOARD_SYS_I2C_SDA,
                       &s_system_i2c);
    if (err != ESP_OK) goto fail;

    err = init_io_expander();
    if (err != ESP_OK) goto fail;

    err = init_lcd_panel();
    if (err != ESP_OK) goto fail;

    err = init_i2c_bus(BOARD_TOUCH_I2C_PORT,
                       BOARD_TOUCH_I2C_SCL,
                       BOARD_TOUCH_I2C_SDA,
                       &s_touch_i2c);
    if (err != ESP_OK) goto fail;

    err = init_touch();
    if (err != ESP_OK) goto fail;

    err = init_lvgl();
    if (err != ESP_OK) goto fail;

    s_display_ready = true;
    ESP_LOGI(TAG, "display/touch/LVGL runtime ready; backlight remains off");
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
    if (!s_lvgl_started) return false;
    return lvgl_port_lock(timeout_ms);
}

void board_display_unlock(void)
{
    if (s_lvgl_started) lvgl_port_unlock();
}

esp_err_t board_backlight_set_percent(uint8_t percent)
{
    if (!s_backlight_ready || s_io_expander == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (percent > 100) percent = 100;
    const uint32_t duty = ((uint32_t)percent * 255U + 50U) / 100U;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    if (err != ESP_OK) return err;
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    if (err != ESP_OK) return err;

    err = esp_io_expander_set_level(s_io_expander,
                                    BOARD_EXIO_PIN_BL_EN,
                                    percent > 0 ? 1 : 0);
    if (err != ESP_OK) return err;

    s_backlight_percent = percent;
    return ESP_OK;
}

uint8_t board_backlight_get_percent(void)
{
    return s_backlight_percent;
}
