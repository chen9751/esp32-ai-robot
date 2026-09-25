#include "ui_assets.h"

void ui_asset_remote_init(void);
void ui_asset_music_init(void);
void ui_asset_light_init(void);
void ui_asset_devices_init(void);
void ui_asset_alarm_init(void);
void ui_asset_settings_init(void);

void ui_assets_init(void)
{
    ui_asset_remote_init();
    ui_asset_music_init();
    ui_asset_light_init();
    ui_asset_devices_init();
    ui_asset_alarm_init();
    ui_asset_settings_init();
}
