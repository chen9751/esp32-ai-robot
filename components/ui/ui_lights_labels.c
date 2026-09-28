#include "ui_lights_labels.h"

void ui_lights_label_living_init(void);
void ui_lights_label_study_init(void);
void ui_lights_label_bedroom_init(void);
void ui_lights_label_bedside_init(void);
void ui_lights_label_small_bedroom_init(void);
void ui_lights_label_rgb_strip_init(void);
void ui_lights_label_bathroom_init(void);
void ui_lights_label_balcony_init(void);

void ui_lights_labels_init(void)
{
    ui_lights_label_living_init();
    ui_lights_label_study_init();
    ui_lights_label_bedroom_init();
    ui_lights_label_bedside_init();
    ui_lights_label_small_bedroom_init();
    ui_lights_label_rgb_strip_init();
    ui_lights_label_bathroom_init();
    ui_lights_label_balcony_init();
}
