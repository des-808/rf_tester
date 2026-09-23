/**
 * @file menu_touch.c
 * @brief Система касаний для меню с tap-on-release и hold-to-edit
 * 
 * Вызываются из main.c через Menu_ProcessGesture()
 */

#include "menu_touch.h"
#include "menu.h"
#include "gui.h"
#include "buzzer.h"
#include "vibrator.h"
#include <stdint.h>
#include <string.h>

/* ===== ВНЕШНИЕ ПЕРЕМЕННЫЕ ===== */
extern uint8_t buzzerOnOff, vibroOnOff;

/* ========================================================================
 *  СОСТОЯНИЕ КАШАНИЙ МЕНЮ
 * ======================================================================== */

/* Состояния касания для каждого пункта меню (максимум 32 пункта) */
#define MAX_MENU_TOUCH_ITEMS 32
static MenuItemTouchState_t g_item_states[MAX_MENU_TOUCH_ITEMS];

/* Текущее активное касание (палец на экране) */
int8_t g_active_touch_index = -1;
static uint8_t g_active_touch_id = 0;

/* Время удержания для разных действий (мс) */
#define HOLD_SHORT_MS    300  /* Короткое удержание — инкремент/декремент */
#define HOLD_LONG_MS     800  /* Длинное удержание — вход в edit mode */

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

/**
 * @brief Получить состояние касания пункта меню
 */
MenuItemTouchState_t* MenuTouch_GetItemState(int8_t index) {
    if (index < 0 || index >= MAX_MENU_TOUCH_ITEMS) return NULL;
    return &g_item_states[index];
}

/**
 * @brief Обновить состояние касания при движении пальца
 */
void MenuTouch_UpdatePosition(int8_t index, uint16_t x, uint16_t y) {
    if (index < 0 || index >= MAX_MENU_TOUCH_ITEMS) return;
    
    MenuItemTouchState_t* state = &g_item_states[index];
    if (state->is_active) {
        (void)x;
        (void)y;
        /* Можно сохранить координаты если нужно */
    }
}

/* ========================================================================
 *  ИНИЦИАЛИЗАЦИЯ
 * ======================================================================== */

/**
 * @brief Инициализация системы касаний меню
 */
void MenuTouch_Init(void) {
    memset(g_item_states, 0, sizeof(g_item_states));
    g_active_touch_index = -1;
}

/* ========================================================================
 *  ОБРАБОТКА ЖЕСТОВ
 * ======================================================================== */

/**
 * @brief Обработка жеста из touch_gesture системы
 * 
 * Вызывается из main.c после распознавания жеста.
 * Физические кнопки обрабатываются отдельно через Menu_ProcessInput().
 */
void Menu_ProcessGesture(TouchGesture_Event_t* event) {
    if (!event || !current_menu_listbox || !current_menu_items) return;
    
    /* Определяем индекс пункта меню под пальцем */
    int16_t local_y = event->y - current_menu_listbox->y;
    uint16_t font_h = (current_menu_listbox->font != NULL) ? 
                      current_menu_listbox->font->char_height : font_arial_9_struct.char_height;
    uint8_t pad = (current_menu_listbox->props.list_box.item_padding > 0) ? 
                  current_menu_listbox->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
    uint16_t item_h = font_h + pad;
    int8_t target = current_menu_listbox->props.list_box.scroll_offset + 
                   (int8_t)(local_y / item_h);
    
    if (target < 0 || (uint8_t)target >= current_menu_listbox->children_count || 
        (uint8_t)target >= current_menu_count) {
        return;
    }
    
    MenuItem_t* item = &current_menu_items[target];
    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[target];
    
    /* Получаем состояние касания для этого пункта */
    MenuItemTouchState_t* state = MenuTouch_GetItemState(target);
    if (!state) return;
    
    uint32_t now = HAL_GetTick();
    uint32_t duration = now - state->press_tick;
    
    switch (event->type) {
        case TOUCH_GESTURE_TAP:
            {
                /* Определяем действие по флагу пункта */
                if (item->touch_flags.tap_on_release) {
                    /* Тап подтверждён только при отпускании — выполняем действие */
                    state->tap_confirmed = 1;
                    
                    /* Синхронизируем выделение */
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    
                    /* Выполняем действие в зависимости от типа */
                    switch (item->type) {
                        case ITEM_TYPE_SUBMENU:
                            /* Вход в подменю */
                            Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                            if (buzzerOnOff) Buzzer_PlayTone(800, 30);
                            if (vibroOnOff) Vibrator_Pulse(30);
                            break;
                            
                        case ITEM_TYPE_ACTION:
                            /* Выполнение action */
                            if (item->data.action_func) {
                                item->data.action_func();
                            }
                            if (buzzerOnOff) Buzzer_PlayTone(1000, 30);
                            if (vibroOnOff) Vibrator_Pulse(30);
                            break;
                            
                        case ITEM_TYPE_VALUE:
                            /* Для значений — тап тоже меняет значение на +1/-1 */
                            if (item->value_size == 1) {
                                int8_t v = *(int8_t*)item->data.ptr_value;
                                v += (int8_t)item->value_limits.step;
                                if (v > item->value_limits.max_val) v = item->value_limits.min_val;
                                *(int8_t*)item->data.ptr_value = v;
                            } else {
                                int32_t v = *(int32_t*)item->data.ptr_value;
                                v += item->value_limits.step;
                                if (v > item->value_limits.max_val) v = item->value_limits.min_val;
                                *(int32_t*)item->data.ptr_value = v;
                            }
                            if (ui_item) {
                                Update_MenuItem_Text(ui_item, item);
                                Touch_RenderItem(current_menu_listbox, (uint8_t)target);
                            }
                            if (item->on_value_changed) {
                                item->on_value_changed();
                            }
                            if (buzzerOnOff) Buzzer_PlayTone(600, 30);
                            if (vibroOnOff) Vibrator_Pulse(30);
                            break;
                            
                        default:
                            break;
                    }
                    
                    /* Сбрасываем состояние */
                    state->is_active = 0;
                    state->is_pressed = 0;
                    state->hold_triggered = 0;
                    state->tap_confirmed = 0;
                } else {
                    /* Мгновенное выполнение (без tap_on_release) */
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                    
                    state->is_active = 0;
                    state->is_pressed = 0;
                }
            }
            break;
            
        case TOUCH_GESTURE_DOUBLE_TAP:
            {
                /* Двойной тап — сброс к min (если разрешено) */
                if (item->touch_flags.double_tap_reset && item->type == ITEM_TYPE_VALUE) {
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    
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
                    /* Обычный двойной тап — выполняем действие */
                    Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                }
                
                state->is_active = 0;
                state->is_pressed = 0;
            }
            break;
            
        case TOUCH_GESTURE_LONG_PRESS:
            {
                /* Долгое нажатие — удержание сработало */
                state->hold_triggered = 1;
                
                if (item->touch_flags.hold_edit && item->type == ITEM_TYPE_VALUE) {
                    /* Вход в режим редактирования */
                    if (ui_item) {
                        ui_item->background_color = RGB565_NAVY;
                        Touch_RenderItem(current_menu_listbox, (uint8_t)target);
                    }
                    Menu_EditMode_Enter(item, target);
                    if (buzzerOnOff) Buzzer_PlayTone(400, 50);
                    if (vibroOnOff) Vibrator_Pulse(30);
                } else if (item->touch_flags.hold_increment && item->type == ITEM_TYPE_VALUE) {
                    /* Удержание = инкремент/декремент */
                    /* Это обрабатывается в PointMove при удержании */
                } else {
                    /* Для других типов — выполняем действие */
                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                }
            }
            break;
            
        case TOUCH_GESTURE_SWIPE:
            {
                /* Свайп — навигация вверх/вниз */
                if (item->touch_flags.swipe_select) {
                    /* Свайп на пункте — навигация, не выбор */
                    if (event->direction == TOUCH_DIR_UP) {
                        Touch_NavUp(current_menu_listbox);
                    } else if (event->direction == TOUCH_DIR_DOWN) {
                        Touch_NavDown(current_menu_listbox);
                    }
                    if (buzzerOnOff) Buzzer_PlayTone(800, 30);
                    if (vibroOnOff) Vibrator_Pulse(30);
                } else {
                    /* Обычный свайп — навигация по всему меню */
                    if (event->direction == TOUCH_DIR_UP) {
                        Touch_NavUp(current_menu_listbox);
                    } else if (event->direction == TOUCH_DIR_DOWN) {
                        Touch_NavDown(current_menu_listbox);
                    }
                    if (buzzerOnOff) Buzzer_PlayTone(800, 30);
                    if (vibroOnOff) Vibrator_Pulse(30);
                }
            }
            break;
            
        case TOUCH_GESTURE_PINCH_IN:
        case TOUCH_GESTURE_PINCH_OUT:
            /* Pinch — можно использовать для масштабирования графика */
            break;
            
        default:
            break;
    }
}

/* ========================================================================
 *  ОБРАБОТКА ДВИЖЕНИЯ ПАЛЬЦА (для hold-to-increment)
 * ======================================================================== */

/**
 * @brief Обработка движения пальца по меню
 * Вызывается из main.c при PointMove
 */
void MenuTouch_ProcessMove(uint16_t x, uint16_t y) {
    if (g_active_touch_index < 0) return;
    
    MenuItemTouchState_t* state = MenuTouch_GetItemState(g_active_touch_index);
    if (!state || !state->is_active) return;
    
    uint32_t now = HAL_GetTick();
    uint32_t duration = now - state->press_tick;
    
    /* Проверяем удержание для инкремента/декремента */
    if (duration >= HOLD_SHORT_MS && !state->hold_triggered) {
        /* Короткое удержание — инкремент/декремент */
        /* Это будет обработано в Menu_ProcessGesture при LONG_PRESS */
    }
}
