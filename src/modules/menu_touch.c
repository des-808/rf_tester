#include "menu_touch.h"
#include "menu.h"
#include "gui.h"
#include "buzzer.h"
#include <stdint.h>
#include <string.h>

/* ===== ВНЕШНИЕ ПЕРЕМЕННЫЕ ===== */
/* touch_lock_tick объявлен в menu.h */
/* edit_* переменные объявлены в menu.h */

/* ===== ВНЕШНИЕ ПЕРЕМЕННЫЕ ===== */
extern uint8_t buzzerOnOff, vibroOnOff;
extern uint32_t touch_lock_tick;
extern uint32_t cc1101FreqFixed;
extern uint16_t cc1101BitRateFixed;
extern uint8_t cc1101RxBwIndex, cc1101Modulation, cc1101PowerIndex;

/* ===== КОНСТАНТЫ ===== */
#define BOTTOM_BAR_DEBOUNCE_MS  200  /* Минимальный интервал между нажатиями кнопок */
#define TOUCH_LOCK_MS           300  /* Блокировка тач-элементов после перехода */

/* ===== КОНСТАНТЫ ТИПОВ НАЖАТИЙ ===== */
#define SHORT_PRESS_MS       200  /* Минимальное время для короткого нажатия */
#define LONG_PRESS_MS       1000  /* Время для длинного нажатия */
#define DOUBLE_CLICK_MS      400  /* Максимальный интервал между кликами */
#define DOUBLE_CLICK_THRESH 15   /* Допуск смещения для двойного клика (px) */
                                                                                                           
/* ===== ТИПЫ НАЖАТИЙ ===== */
typedef enum {
    TOUCH_TYPE_NONE = 0,
    TOUCH_TYPE_SHORT,
    TOUCH_TYPE_LONG,
    TOUCH_TYPE_DOUBLE
} TouchType_t;

/* ===== СОСТОЯНИЕ ОДНОГО ТОЧЕЧНОГО НАЖАТИЯ ===== */
typedef struct {
    uint16_t       start_x;     /* Координаты начала нажатия */
    uint16_t       start_y;
    uint16_t       current_x;   /* Текущие координаты */
    uint16_t       current_y;
    uint32_t       press_start; /* Время начала нажатия */
    uint32_t       last_click;  /* Время последнего клика (для двойного) */
    uint8_t        click_count; /* Счётчик кликов */
    int8_t         item_idx;    /* Индекс пункта меню под пальцем */
    uint8_t        is_active;   /* Флаг активного нажатия */
    uint8_t        is_locked;   /* Флаг блокировки (после короткого нажатия) */
    uint8_t        long_triggered; /* Флаг срабатывания длинного нажатия */
} TouchPoint_t;

/* ===== ГЛОБАЛЬНОЕ СОСТОЯНИЕ ТОЧЕК ===== */
#define MAX_TOUCH_POINTS 1
static TouchPoint_t touch_points[MAX_TOUCH_POINTS];

/* ===== Удерживаемая кнопка нижней панели ===== */
static int8_t last_held_btn_idx = -1;
static int8_t edit_touch_btn_idx = -1;
static uint32_t edit_touch_start_tick = 0;

/* ===== ИНИЦИАЛИЗАЦИЯ ТОЧКИ НАЖАТИЯ ===== */
static void TouchPoint_Init(TouchPoint_t* tp, uint16_t x, uint16_t y) {
    tp->start_x = x;
    tp->start_y = y;
    tp->current_x = x;
    tp->current_y = y;
    tp->press_start = HAL_GetTick();
    tp->last_click = 0;
    tp->click_count = 0;
    tp->item_idx = -1;
    tp->is_active = 1;
    tp->is_locked = 0;
    tp->long_triggered = 0;
}

/* ===== ОБНОВЛЕНИЕ СОСТОЯНИЯ ТОЧКИ ===== */
static void TouchPoint_Update(TouchPoint_t* tp, uint16_t x, uint16_t y) {
    tp->current_x = x;
    tp->current_y = y;
}

/* ===== ПРОВЕРКА: точка всё ещё стабильна (не сдвинулась сильно) ===== */
static int8_t TouchPoint_IsStable(TouchPoint_t* tp) {
    int16_t dx = (int16_t)tp->current_x - (int16_t)tp->start_x;
    int16_t dy = (int16_t)tp->current_y - (int16_t)tp->start_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx <= DOUBLE_CLICK_THRESH && dy <= DOUBLE_CLICK_THRESH) ? 1 : 0;
}

/* ===== ОПРЕДЕЛЕНИЕ ТИПА НАЖАТИЯ ===== */
static TouchType_t TouchPoint_GetType(TouchPoint_t* tp, uint32_t now) {
    /* Если уже было длинное нажатие — возвращаем LONG */
    if (tp->long_triggered) {
        return TOUCH_TYPE_LONG;
    }
    
    /* Проверяем двойной клик */
    if (tp->click_count >= 2) {
        return TOUCH_TYPE_DOUBLE;
    }
    
    /* Проверяем время нажатия */
    uint32_t duration = now - tp->press_start;
    
    /* Если держим дольше LONG_PRESS_MS — это длинное нажатие */
    if (duration >= LONG_PRESS_MS) {
        tp->long_triggered = 1;
        return TOUCH_TYPE_LONG;
    }
    
    /* Если отпустили (будет проверено в Touch_PointRelease) */
    return TOUCH_TYPE_NONE;
}

/* ===== ОСВОБОЖДЕНИЕ ТОЧКИ (палец убран) ===== */
static TouchType_t TouchPoint_Release(TouchPoint_t* tp, uint32_t now) {
    tp->is_active = 0;
    
    if (tp->click_count == 0) {
        /* Первый клик — проверяем время */
        uint32_t duration = now - tp->press_start;
        if (duration >= SHORT_PRESS_MS) {
            /* Было достаточно долго — считаем как короткое нажатие */
            tp->click_count = 1;
            return TOUCH_TYPE_SHORT;
        }
    }
    
    /* Если не определили тип — возвращаем NONE */
    return TOUCH_TYPE_NONE;
}

/* ===== ПОИСК ТОЧКИ ПО КООРДИНАТАМ ===== */
static TouchPoint_t* TouchPoint_FindByCoords(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touch_points[i].is_active) {
            int16_t dx = (int16_t)x - (int16_t)touch_points[i].current_x;
            int16_t dy = (int16_t)y - (int16_t)touch_points[i].current_y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx <= 10 && dy <= 10) {  /* В пределах 10px считаем той же точкой */
                return &touch_points[i];
            }
        }
    }
    return NULL;
}

/* ===== СБРОС ВСЕХ ТОЧЕК ===== */
static void TouchPoint_ResetAll(void) {
    for (uint8_t i = 0; i < MAX_TOUCH_POINTS; i++) {
        touch_points[i].is_active = 0;
        touch_points[i].is_locked = 0;
        touch_points[i].long_triggered = 0;
        touch_points[i].click_count = 0;
    }
}

/* ===== КОНСТАНТЫ ФИЛЬТРАЦИИ КООРДИНАТ ===== */
#define TOUCH_SAMPLE_COUNT  5  /* Количество последних показаний для проверки стабильности */
#define TOUCH_STABLE_THRESH 8  /* Макс. размах координат (px) для признания стабильным */

/* ===== ФИЛЬТРАЦИЯ: проверка стабильности координат ===== */
/* Если палец на месте — координаты почти не меняются.
 * Если палец движется (скролл) — координаты прыгают. */
static int8_t Touch_IsStable(uint16_t tx, uint16_t ty) {
    static uint16_t buf_x[TOUCH_SAMPLE_COUNT] = {0};
    static uint16_t buf_y[TOUCH_SAMPLE_COUNT] = {0};
    static uint8_t  buf_count = 0;
    
    /* Записываем новое показание в кольцевой буфер */
    buf_x[buf_count % TOUCH_SAMPLE_COUNT] = tx;
    buf_y[buf_count % TOUCH_SAMPLE_COUNT] = ty;
    buf_count++;
    
    /* Нужно минимум 3 показания для проверки */
    if (buf_count < 3) return 1;
    
    /* Находим размах (max - min) по X и Y за последние N показаний */
    uint16_t min_x = buf_x[0], max_x = buf_x[0];
    uint16_t min_y = buf_y[0], max_y = buf_y[0];
    
    uint8_t start = (buf_count > TOUCH_SAMPLE_COUNT) ? 
                    (buf_count - TOUCH_SAMPLE_COUNT) : 0;
    uint8_t end = buf_count;
    if (end - start > TOUCH_SAMPLE_COUNT) end = start + TOUCH_SAMPLE_COUNT;
    
    for (uint8_t i = start; i < end; i++) {
        uint16_t x = buf_x[i % TOUCH_SAMPLE_COUNT];
        uint16_t y = buf_y[i % TOUCH_SAMPLE_COUNT];
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    
    uint16_t range_x = max_x - min_x;
    uint16_t range_y = max_y - min_y;
    
    /* Если размах больше порога — палец движется, игнорируем */
    return (range_x <= TOUCH_STABLE_THRESH && range_y <= TOUCH_STABLE_THRESH) ? 1 : 0;
}

/* ===== ВСПОМОГАТЕЛЬНАЯ: скролл к элементу ===== */
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

/* ===== ВСПОМОГАТЕЛЬНАЯ: рендер строки ===== */
static void Touch_RenderItem(UIElement_t* lb, uint8_t idx) {
    if (idx < lb->children_count) {
        UI_RenderListBoxItem(lb, idx);
    }
}

/* ===== ВСПОМОГАТЕЛЬНАЯ: навигация Up ===== */
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

/* ===== ВСПОМОГАТЕЛЬНАЯ: навигация Down ===== */
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

/* ===== ВСПОМОГАТЕЛЬНАЯ: синхронизация selected_index ===== */
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
 *  ОБРАБОТКА НИЖНЕЙ ПАНЕЛИ (Cancel / Up / Down / Enter)
 * ======================================================================== */
static void ProcessBottomBar(UIElement_t* lb, int8_t btn_idx, uint32_t now) {
    switch (btn_idx) {
        case 0: /* Cancel — выход из подменю */
            Menu_PopMenu(lb);
            touch_lock_tick = now;
            if (buzzerOnOff) Buzzer_PlayTone(400, 50);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 1: /* Up — вверх по меню */
            Touch_NavUp(lb);
            if (buzzerOnOff) Buzzer_PlayTone(800, 30);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 2: /* Down — вниз по меню */
            Touch_NavDown(lb);
            if (buzzerOnOff) Buzzer_PlayTone(800, 30);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 3: /* Enter — выбрать/открыть пункт */
            {
            uint8_t idx = (uint8_t)lb->props.list_box.selected_index;
            if (idx < current_menu_count) {
                Menu_ExecuteSelected(lb, idx);
                if (buzzerOnOff) Buzzer_PlayTone(1000, 50);
                if (vibroOnOff) Vibrator_Pulse(30);
            }
            }
            break;
    }
}

/* ========================================================================
 *  ОБРАБОТКА НИЖНЕЙ ПАНЕЛИ В РЕЖИМЕ РЕДАКТИРОВАНИЯ
 * ======================================================================== */
static void ProcessEditBar(UIElement_t* lb, int8_t edit_btn, uint32_t now,
                           MenuItem_t* edit_source_item, uint8_t coarse) {
    uint16_t adjustment_step = edit_step;
    if (coarse && (strcmp(edit_source_item->text, "Freq MHz") == 0 ||
                   strcmp(edit_source_item->text, "Freq") == 0 ||
                   strcmp(edit_source_item->text, "BitRate") == 0)) {
        adjustment_step = (strcmp(edit_source_item->text, "Freq MHz") == 0 ||
                   strcmp(edit_source_item->text, "Freq") == 0) ?
                  CC1101_FREQ_COARSE_STEP : CC1101_BITRATE_COARSE_STEP;
    }
    switch (edit_btn) {
        case 0: /* Cancel — отмена */
            if (edit_source_item->value_size == 1) {
                *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_original_value;
            } else if (edit_source_item->value_size == 4) {
                *(uint32_t*)edit_source_item->data.ptr_value = edit_original_value;
            } else {
                *(uint16_t*)edit_source_item->data.ptr_value = edit_original_value;
            }
            edit_temp_value = edit_original_value;
            Menu_EditMode_PreviewValue();
            Menu_EditMode_Exit();
            if (buzzerOnOff) Buzzer_PlayTone(400, 50);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 1: /* Up — увеличить */
            if (edit_source_item->value_size == 1) {
                int8_t v = (int8_t)edit_temp_value;
                v += (int8_t)adjustment_step;
                if (v > edit_source_item->value_limits.max_val) v = edit_source_item->value_limits.min_val;
                edit_temp_value = (uint16_t)v;
            } else {
                int32_t v = (int32_t)edit_temp_value;
                v += adjustment_step;
                if (v > edit_source_item->value_limits.max_val) v = edit_source_item->value_limits.min_val;
                edit_temp_value = (uint32_t)v;
            }
            if (edit_source_item->value_size == 4) {
                edit_temp_value = Menu_NormalizeFrequency(edit_temp_value, 1);
            }
            Menu_EditMode_PreviewValue();
            Menu_Update_EditDisplay();
            if (buzzerOnOff) Buzzer_PlayTone(800, 30);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 2: /* Down — уменьшить */
            if (edit_source_item->value_size == 1) {
                int8_t v = (int8_t)edit_temp_value;
                v -= (int8_t)adjustment_step;
                if (v < edit_source_item->value_limits.min_val) v = edit_source_item->value_limits.max_val;
                edit_temp_value = (uint16_t)v;
            } else {
                int32_t v = (int32_t)edit_temp_value - adjustment_step;
                if (v < edit_source_item->value_limits.min_val) v = edit_source_item->value_limits.max_val;
                edit_temp_value = (uint32_t)v;
            }
            if (edit_source_item->value_size == 4) {
                edit_temp_value = Menu_NormalizeFrequency(edit_temp_value, -1);
            }
            Menu_EditMode_PreviewValue();
            Menu_Update_EditDisplay();
            if (buzzerOnOff) Buzzer_PlayTone(800, 30);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
        case 3: /* Enter — сохранить */
            if (edit_source_item->value_size == 1) {
                *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_temp_value;
            } else if (edit_source_item->value_size == 4) {
                *(uint32_t*)edit_source_item->data.ptr_value = edit_temp_value;
            } else {
                *(uint16_t*)edit_source_item->data.ptr_value = edit_temp_value;
            }
            if (edit_source_item->on_value_changed) {
                edit_source_item->on_value_changed();
            }
            Menu_EditMode_Exit();
            if (buzzerOnOff) Buzzer_PlayTone(1000, 50);
            if (vibroOnOff) Vibrator_Pulse(30);
            break;
    }
}

/* ========================================================================
 *  ОСНОВНАЯ ФУНКЦИЯ ОБРАБОТКИ КАСАНИЯ
 * ======================================================================== */
void Menu_ProcessTouch(uint16_t tx, uint16_t ty) {
    if (!current_menu_listbox || !current_menu_items) return;
    UIElement_t* lb = current_menu_listbox;

    if (lb->touch_state.drag_active) return;

    /* ===== ФИЛЬТРАЦИЯ: проверяем что палец на месте ===== */
    if (!Touch_IsStable(tx, ty)) {
        return;  /* Палец движется — игнорируем */
    }

    uint32_t now = HAL_GetTick();
    int8_t in_lock = (now - touch_lock_tick < TOUCH_LOCK_MS);
    static uint32_t last_btn_tick[4] = {0, 0, 0, 0};

    /* ===== Ищем существующую точку или создаём новую ===== */
    TouchPoint_t* tp = TouchPoint_FindByCoords(tx, ty);
    
    if (!tp) {
        /* Новая точка — ищем свободную или перезаписываем первую */
        tp = NULL;
        for (uint8_t i = 0; i < MAX_TOUCH_POINTS; i++) {
            if (!touch_points[i].is_active) {
                tp = &touch_points[i];
                break;
            }
        }
        if (!tp) {
            tp = &touch_points[0];  /* Перезаписываем первую */
        }
        TouchPoint_Init(tp, tx, ty);
    } else {
        /* Обновляем существующую точку */
        TouchPoint_Update(tp, tx, ty);
    }
    
    /* ===== РЕЖИМ РЕДАКТИРОВАНИЯ ЗНАЧЕНИЯ (TOUCH) ===== */
    if (edit_mode_active && edit_source_item) {
        int8_t edit_btn = GUI_GetBottomBarTouch(tx, ty);
        if (edit_btn >= 0) {
            /* Если эта кнопка уже удерживается — игнорируем до отпускания */
            if (edit_btn == last_held_btn_idx) {
                return;
            }
            if (now - last_btn_tick[edit_btn] < BOTTOM_BAR_DEBOUNCE_MS) {
                return;
            }
            last_btn_tick[edit_btn] = now;
            last_held_btn_idx = (int8_t)edit_btn;
            edit_touch_btn_idx = edit_btn;
            edit_touch_start_tick = now;
            return;
        }
    }

    /* ===== НИЖНЯЯ ПАНЕЛЬ КНОПОК ===== */
    int8_t btn_idx = GUI_GetBottomBarTouch(tx, ty);
    if (btn_idx >= 0) {
        /* Если эта кнопка уже удерживается — игнорируем до отпускания */
        if (btn_idx == last_held_btn_idx) {
            return;
        }
        /* Проверка debounce */
        if (now - last_btn_tick[btn_idx] < BOTTOM_BAR_DEBOUNCE_MS) {
            return;
        }
        last_btn_tick[btn_idx] = now;
        last_held_btn_idx = (int8_t)btn_idx;  /* Запоминаем удерживаемую кнопку */
        ProcessBottomBar(lb, btn_idx, now);
        return;
    }

    /* ===== БЛОКИРОВКА: после перехода не обрабатываем тач элементов ===== */
    if (in_lock) return;

    /* ===== ОПРЕДЕЛЯЕМ ПУНКТ МЕНЮ ПОД ПАЛЬЦЕМ ===== */
    int16_t local_y = ty - lb->y;
    uint16_t font_h = (lb->font != NULL) ? lb->font->char_height : font_arial_9_struct.char_height;
    uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
    uint16_t item_h = font_h + pad;
    int8_t target = lb->props.list_box.scroll_offset + (int8_t)(local_y / item_h);
    
    if (target < 0 || (uint8_t)target >= lb->children_count || 
        (uint8_t)target >= current_menu_count) {
        return;
    }
    
    /* Обновляем индекс пункта в точке */
    tp->item_idx = target;

    /* ===== ОПРЕДЕЛЕНИЕ ТИПА НАЖАТИЯ ===== */
    TouchType_t touch_type = TouchPoint_GetType(tp, now);
    
    /* Если точка заблокирована (после короткого нажатия) — пропускаем */
    if (tp->is_locked) {
        /* Проверяем, что палец всё ещё на том же месте */
        if (tp->item_idx == target && TouchPoint_IsStable(tp)) {
            return;  /* Блокировка активна — игнорируем */
        }
        /* Палец сдвинулся — разблокируем и продолжаем */
        tp->is_locked = 0;
    }
    
    /* ===== ОБРАБОТКА РАЗНЫХ ТИПОВ НАЖАТИЙ ===== */
    switch (touch_type) {
        case TOUCH_TYPE_SHORT:
            /* Короткое нажатие — выделение И выбор пункта */
            {
            MenuItem_t* item = &current_menu_items[target];
            UIElement_t* ui_item = (UIElement_t*)lb->children[target];
            
            Touch_SyncSelection(lb, target, item, ui_item);
            Menu_ExecuteSelected(lb, (uint8_t)target);
            
            /* Блокируем повторное нажатие до отпускания пальца */
            tp->is_locked = 1;
            }
            break;
            
        case TOUCH_TYPE_LONG:
            /* Длинное нажатие (≥1 сек) — редактирование или выполнение */
            {
            MenuItem_t* item = &current_menu_items[target];
            UIElement_t* ui_item = (UIElement_t*)lb->children[target];
            
            Touch_SyncSelection(lb, target, item, ui_item);
            
            /* Для ITEM_TYPE_VALUE — входим в режим редактирования */
            if (item->type == ITEM_TYPE_VALUE) {
                /* Визуальная индикация: подсвечиваем строку */
                if (ui_item) {
                    ui_item->background_color = RGB565_NAVY;
                    Touch_RenderItem(lb, (uint8_t)target);
                }
                /* Входим в edit mode */
                Menu_EditMode_Enter(item, target);
                /* Блокируем точку пока в edit mode */
                tp->is_locked = 1;
            }
            /* Для ACTION/SUBMENU — выполняем действие */
            else {
                Menu_ExecuteSelected(lb, (uint8_t)target);
                tp->is_locked = 1;
            }
            break;
            }
            
        case TOUCH_TYPE_DOUBLE:
            /* Двойной клик — сброс значения к минимальному */
            {
            MenuItem_t* item = &current_menu_items[target];
            UIElement_t* ui_item = (UIElement_t*)lb->children[target];
            
            Touch_SyncSelection(lb, target, item, ui_item);
            
            /* Для ITEM_TYPE_VALUE — сброс к min */
            if (item->type == ITEM_TYPE_VALUE) {
                /* Устанавливаем минимальное значение */
                if (item->value_size == 1) {
                    *(uint8_t*)item->data.ptr_value = (uint8_t)item->value_limits.min_val;
                } else {
                    *(uint16_t*)item->data.ptr_value = (uint16_t)item->value_limits.min_val;
                }
                /* Обновляем текст и перерисовываем */
                if (ui_item) {
                    Update_MenuItem_Text(ui_item, item);
                    Touch_RenderItem(lb, (uint8_t)target);
                }
                /* Вызываем колбэк если есть */
                if (item->on_value_changed) {
                    item->on_value_changed();
                }
                /* Звуковая индикация сброса */
                if (buzzerOnOff) Buzzer_PlayTone(600, 30);
            }
            /* Для SUBMENU — вход в подменю */
            else if (item->type == ITEM_TYPE_SUBMENU) {
                Menu_ExecuteSelected(lb, (uint8_t)target);
            }
            /* Для ACTION — обычное действие */
            else {
                Menu_ExecuteSelected(lb, (uint8_t)target);
            }
            break;
            }
            
        default:
            break;
    }
}

/* ========================================================================
 *  ОБРАБОТКА ОТПУСКАНИЯ ПАЛЬЦА
 * ======================================================================== */
void Menu_ProcessTouchRelease(void) {
    uint32_t now = HAL_GetTick();

    if (edit_touch_btn_idx >= 0 && edit_mode_active && edit_source_item) {
        uint8_t coarse = (now - edit_touch_start_tick >= BUTTON_HOLD_TIMEOUT_MS);
        ProcessEditBar(current_menu_listbox, edit_touch_btn_idx, now,
                       edit_source_item, coarse);
        edit_touch_btn_idx = -1;
    }
    
    /* Сброс удерживаемой кнопки нижней панели */
    last_held_btn_idx = -1;
    
    for (uint8_t i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touch_points[i].is_active) {
            /* Определяем тип отпускания */
            TouchType_t touch_type = TouchPoint_Release(&touch_points[i], now);
            
            /* Если было короткое нажатие — сбрасываем блокировку */
            if (touch_type == TOUCH_TYPE_SHORT) {
                int8_t target = touch_points[i].item_idx;
                if (target >= 0 && (uint8_t)target < current_menu_count &&
                    (uint8_t)target < current_menu_listbox->children_count) {
                    MenuItem_t* item = &current_menu_items[target];
                    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[target];

                    Touch_SyncSelection(current_menu_listbox, target, item, ui_item);
                    Menu_ExecuteSelected(current_menu_listbox, (uint8_t)target);
                }
                touch_points[i].is_locked = 0;
            }
            
            /* Если было длинное нажатие (edit mode) — выходим из edit mode */
            if (touch_points[i].long_triggered && edit_mode_active) {
                /* Сохраняем текущее значение */
                if (edit_source_item) {
                    if (edit_value_size == 1) {
                        *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_temp_value;
                    } else {
                        *(uint16_t*)edit_source_item->data.ptr_value = edit_temp_value;
                    }
                    if (edit_source_item->on_value_changed) {
                        edit_source_item->on_value_changed();
                    }
                }
                Menu_EditMode_Exit();
                if (buzzerOnOff) Buzzer_PlayTone(1000, 50);
                if (vibroOnOff) Vibrator_Pulse(30);
            }
        }
    }
}
