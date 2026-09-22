/**
 * @file menu_touch.c
 * @brief Helper-функции для обработки жестов в меню
 * 
 * Вызываются из main.c через Menu_ProcessGesture()
 */

#include "menu_touch.h"
#include "menu.h"
#include "gui.h"
#include "buzzer.h"
#include "vibrator.h"
#include <stdint.h>

/* ===== ВНЕШНИЕ ПЕРЕМЕННЫЕ ===== */
extern uint8_t buzzerOnOff, vibroOnOff;

/* ========================================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ======================================================================== */

/**
 * @brief Скролл к элементу
 */
static void Touch_ScrollToItem(UIElement_t* lb, int8_t target) {
    uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
    uint16_t item_h = lb->font->char_height + pad;
    uint8_t visible_items = lb->h / item_h;
    if (visible_items == 0) visible_items = 1;
    
    if (target < (int16_t)lb->props.list_box.scroll_offset) {
        lb->props.list_box.scroll_offset = (uint8_t)target;
    } else if (target >= (int16_t)(lb->props.list_box.scroll_offset + visible_items)) {
        lb->props.list_box.scroll_offset = (uint8_t)(target - visible_items + 1);
    }
}

/**
 * @brief Перерисовка элемента списка
 */
static void Touch_RenderItem(UIElement_t* lb, uint8_t idx) {
    if (idx < lb->children_count) {
        UI_RenderListBoxItem(lb, idx);
    }
}

/**
 * @brief Навигация вверх по меню
 */
static void Touch_NavUp(UIElement_t* lb) {
    if (lb->props.list_box.selected_index > 0) {
        uint8_t old_idx = (uint8_t)lb->props.list_box.selected_index;
        lb->props.list_box.selected_index--;
        lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
        Touch_ScrollToItem(lb, (int8_t)lb->props.list_box.selected_index);
        Touch_RenderItem(lb, old_idx);
        Touch_RenderItem(lb, (uint8_t)lb->props.list_box.selected_index);
    }
}

/**
 * @brief Навигация вниз по меню
 */
static void Touch_NavDown(UIElement_t* lb) {
    if (lb->props.list_box.selected_index < (int16_t)current_menu_count - 1) {
        uint8_t old_idx = (uint8_t)lb->props.list_box.selected_index;
        lb->props.list_box.selected_index++;
        lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
        Touch_ScrollToItem(lb, (int8_t)lb->props.list_box.selected_index);
        Touch_RenderItem(lb, old_idx);
        Touch_RenderItem(lb, (uint8_t)lb->props.list_box.selected_index);
    }
}

/**
 * @brief Синхронизация выделения элемента меню
 */
static void Touch_SyncSelection(UIElement_t* lb, int8_t selected, MenuItem_t* item, UIElement_t* ui_item) {
    if (lb->props.list_box.selected_index != selected) {
        uint8_t old_idx = (uint8_t)lb->props.list_box.selected_index;
        lb->props.list_box.selected_index = selected;
        lb->props.list_box.last_leaf_selected = selected;
        Touch_ScrollToItem(lb, selected);
        Touch_RenderItem(lb, old_idx);
    } else {
        lb->props.list_box.last_leaf_selected = selected;
    }
    if (ui_item) Update_MenuItem_Text(ui_item, item);
    Touch_RenderItem(lb, (uint8_t)lb->props.list_box.selected_index);
}

/* ========================================================================
 *  ОБРАБОТКА ЖЕСТОВ ИЗ TOUCH_GESTURE SYSTEM
 * ======================================================================== */

/**
 * @brief Обработка жеста из touch_gesture системы
 * 
 * Вызывается из main.c после распознавания жеста.
 * Физические кнопки обрабатываются отдельно через Menu_ProcessInput().
 */
void Menu_ProcessGesture(TouchGesture_Event_t* event) {
    if (!event || !current_menu_listbox) return;
    
    switch (event->type) {
        case TOUCH_GESTURE_TAP:
        case TOUCH_GESTURE_DOUBLE_TAP:
            {
                // Определяем индекс пункта меню под пальцем
                int16_t local_y = event->y - current_menu_listbox->y;
                uint16_t font_h = (current_menu_listbox->font != NULL) ? 
                                  current_menu_listbox->font->char_height : font_arial_9_struct.char_height;
                uint8_t pad = (current_menu_listbox->props.list_box.item_padding > 0) ? 
                              current_menu_listbox->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = font_h + pad;
                int8_t target = current_menu_listbox->props.list_box.scroll_offset + 
                               (int8_t)(local_y / item_h);
                
                if (target >= 0 && (uint8_t)target < current_menu_listbox->children_count &&
                    (uint8_t)target < current_menu_count) {
                    MenuItem_t* item = &current_menu_items[target];
                    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[target];
                    
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    
                    if (event->type == TOUCH_GESTURE_DOUBLE_TAP) {
                        // Двойной тап — сброс значения к min
                        if (item->type == ITEM_TYPE_VALUE) {
                            if (item->value_size == 1) {
                                *(uint8_t*)item->data.ptr_value = (uint8_t)item->value_limits.min_val;
                            } else {
                                *(uint16_t*)item->data.ptr_value = (uint16_t)item->value_limits.min_val;
                            }
                            if (ui_item) {
                                Update_MenuItem_Text(ui_item, item);
                                Touch_RenderItem(current_menu_listbox, (uint8_t)target);
                            }
                            if (item->on_value_changed) {
                                item->on_value_changed();
                            }
                            if (buzzerOnOff) Buzzer_PlayTone(600, 30);
                        } else {
                            Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                        }
                    } else {
                        // Одинарный тап — выбор/выполнение
                        Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                    }
                }
            }
            break;
            
        case TOUCH_GESTURE_LONG_PRESS:
            {
                // Долгое нажатие — вход в редактирование или выполнение
                int16_t local_y = event->y - current_menu_listbox->y;
                uint16_t font_h = (current_menu_listbox->font != NULL) ? 
                                  current_menu_listbox->font->char_height : font_arial_9_struct.char_height;
                uint8_t pad = (current_menu_listbox->props.list_box.item_padding > 0) ? 
                              current_menu_listbox->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = font_h + pad;
                int8_t target = current_menu_listbox->props.list_box.scroll_offset + 
                               (int8_t)(local_y / item_h);
                
                if (target >= 0 && (uint8_t)target < current_menu_listbox->children_count &&
                    (uint8_t)target < current_menu_count) {
                    MenuItem_t* item = &current_menu_items[target];
                    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[target];
                    
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    
                    if (item->type == ITEM_TYPE_VALUE) {
                        if (ui_item) {
                            ui_item->background_color = RGB565_NAVY;
                            Touch_RenderItem(current_menu_listbox, (uint8_t)target);
                        }
                        Menu_EditMode_Enter(item, target);
                    } else {
                        Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                    }
                }
            }
            break;
            
        case TOUCH_GESTURE_SWIPE:
            {
                // Свайп — навигация вверх/вниз
                if (event->direction == TOUCH_DIR_UP) {
                    Touch_NavUp(current_menu_listbox);
                } else if (event->direction == TOUCH_DIR_DOWN) {
                    Touch_NavDown(current_menu_listbox);
                }
                if (buzzerOnOff) Buzzer_PlayTone(800, 30);
                if (vibroOnOff) Vibrator_Pulse(30);
            }
            break;
            
        case TOUCH_GESTURE_PINCH_IN:
        case TOUCH_GESTURE_PINCH_OUT:
            // Pinch — можно использовать для масштабирования графика
            break;
            
        default:
            break;
    }
}
