/**
 * @file lvgl_example.c
 * @brief LVGL example UI with basic widgets
 */

#include "lvgl_example.h"
#include "lvgl.h"

/* Example UI creation */
void lvgl_create_example_ui(void) {
    /* Create a label on the main screen */
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_hex(0x007000));
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);
    
    /* Create a container */
    lv_obj_t * cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 300, 200);
    lv_obj_center(cont);
    lv_obj_add_style(cont, &style, 0);
    
    /* Create a label */
    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, "LVGL Working!");
    lv_obj_center(label);
    
    /* Create a button */
    lv_obj_t * btn = lv_button_create(cont);
    lv_obj_set_pos(btn, 80, 60);
    lv_obj_set_size(btn, 140, 40);
    
    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Press Me");
    lv_obj_center(btn_label);
    
    /* Create a slider */
    lv_obj_t * slider = lv_slider_create(cont);
    lv_obj_set_size(slider, 200, 10);
    lv_obj_set_pos(slider, 50, 120);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    
    /* Cleanup style */
    lv_style_reset(&style);
    
    printf("Example UI created\r\n");
}
