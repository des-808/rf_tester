/**
 * @file touch_gesture.c
 * @brief Система отслеживания касаний и распознавания жестов
 * 
 * Архитектура вдохновлена LVGL input device system:
 * - Кольцевой буфер истории точек для анализа движения
 * - State machine для распознавания жестов
 * - Multi-touch поддержка (до 5 точек FT6336U)
 * - Swipe, tap, double-tap, long-press, drag, pinch
 */

#include "touch_gesture.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* ===== ГЛОБАЛЬНОЕ СОСТОЯНИЕ ===== */
static TouchGestures_State g_state;

/* ========================================================================
 *  ИНИЦИАЛИЗАЦИЯ
 * ======================================================================== */

void TouchGestures_Init(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.current_event.type = TOUCH_GESTURE_NONE;
    g_state.current_event.direction = TOUCH_DIR_NONE;
}

/* ========================================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ======================================================================== */

/**
 * @brief Добавить сэмпл в кольцевой буфер истории точки
 */
static void Point_PushHistory(TouchPoint_t* point, uint16_t x, uint16_t y, uint32_t tick) {
    point->history_x[point->history_head] = x;
    point->history_y[point->history_head] = y;
    point->history_ts[point->history_head] = tick;
    point->history_head = (point->history_head + 1) % TOUCH_HISTORY_SIZE;
    if (point->history_count < TOUCH_HISTORY_SIZE) {
        point->history_count++;
    }
}

/**
 * @brief Найти свободную точку контакта
 */
static TouchPoint_t* Point_FindFree(void) {
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (!g_state.points[i].is_active) {
            return &g_state.points[i];
        }
    }
    return NULL;
}

/**
 * @brief Найти точку по ID (от FT6336U)
 */
static TouchPoint_t* Point_FindById(uint8_t id) {
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (g_state.points[i].is_active && g_state.points[i].id == id) {
            return &g_state.points[i];
        }
    }
    return NULL;
}

/**
 * @brief Найти точку по координатам (с допуском)
 */
TouchPoint_t* TouchGestures_PointFind(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (!g_state.points[i].is_active) continue;
        
        int16_t dx = (int16_t)x - (int16_t)g_state.points[i].x;
        int16_t dy = (int16_t)y - (int16_t)g_state.points[i].y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        
        /* Точка считается той же, если смещение <= 15px */
        if (dx <= 15 && dy <= 15) {
            return &g_state.points[i];
        }
    }
    return NULL;
}

/**
 * @brief Квадрат расстояния между двумя точками (без sqrt)
 */
static uint32_t Point_DistSq(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    int32_t dx = (int32_t)x2 - (int32_t)x1;
    int32_t dy = (int32_t)y2 - (int32_t)y1;
    return (uint32_t)(dx * dx + dy * dy);
}

/* ========================================================================
 *  УПРАВЛЕНИЕ ТОЧКАМИ КОНТАКТА
 * ======================================================================== */

/**
 * @brief Создать новую точку контакта (палец нажал на экран)
 */
TouchPoint_t* TouchGestures_PointDown(uint16_t x, uint16_t y, uint8_t id) {
    /* Сначала ищем точку с таким же ID */
    TouchPoint_t* point = Point_FindById(id);
    
    if (!point) {
        /* Ищем свободную точку */
        point = Point_FindFree();
        if (!point) {
            /* Нет свободных точек — перезаписываем самую старую (первую) */
            point = &g_state.points[0];
        }
        
        /* Инициализация новой точки */
        memset(point, 0, sizeof(TouchPoint_t));
        point->id = id;
        point->x = x;
        point->y = y;
        point->start_x = x;
        point->start_y = y;
        point->press_tick = HAL_GetTick();
        point->is_active = 1;
        point->is_new = 1;
        point->gesture_started = 0;
        point->drag_confirmed = 0;
        point->history_count = 0;
        point->history_head = 0;
        
        /* Добавляем первый сэмпл в историю */
        Point_PushHistory(point, x, y, point->press_tick);
    } else {
        /* Обновляем существующую точку */
        point->x = x;
        point->y = y;
        point->is_new = 1;
        Point_PushHistory(point, x, y, HAL_GetTick());
    }
    
    /* Увеличиваем счётчик активных точек */
    g_state.point_count = 0;
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (g_state.points[i].is_active) {
            g_state.point_count++;
        }
    }
    
    return point;
}

/**
 * @brief Обновить позицию существующей точки
 */
void TouchGestures_PointMove(TouchPoint_t* point, uint16_t x, uint16_t y) {
    if (!point || !point->is_active) return;
    
    point->x = x;
    point->y = y;
    point->is_new = 0;
    
    /* Добавляем сэмпл в историю */
    uint32_t now = HAL_GetTick();
    
    /* Проверяем минимальный интервал между сэмплами */
    if (point->history_count > 0) {
        uint8_t last_idx = (point->history_head - 1 + TOUCH_HISTORY_SIZE) % TOUCH_HISTORY_SIZE;
        uint32_t last_ts = point->history_ts[last_idx];
        if (now - last_ts < TOUCH_SAMPLE_MS) {
            return;  /* Слишком рано — пропускаем */
        }
    }
    
    Point_PushHistory(point, x, y, now);
}

/**
 * @brief Освободить точку контакта (палец убран)
 * @return указатель на точку или NULL
 */
TouchPoint_t* TouchGestures_PointUp(TouchPoint_t* point) {
    if (!point || !point->is_active) return NULL;
    
    point->is_active = 0;
    point->is_new = 0;
    point->gesture_started = 0;
    point->drag_confirmed = 0;
    
    /* Пересчитываем счётчик активных точек */
    g_state.point_count = 0;
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (g_state.points[i].is_active) {
            g_state.point_count++;
        }
    }
    
    return point;
}

/**
 * @brief Получить количество активных точек
 */
uint8_t TouchGestures_GetPointCount(void) {
    return g_state.point_count;
}

/* ========================================================================
 *  ПРОВЕРКА СТАБИЛЬНОСТИ ТОЧКИ
 * ======================================================================== */

bool TouchGestures_IsPointStable(TouchPoint_t* point) {
    if (!point || point->history_count < 3) return true;
    
    /* Находим размах (max - min) по X и Y за историю */
    uint16_t min_x = point->history_x[0];
    uint16_t max_x = point->history_x[0];
    uint16_t min_y = point->history_y[0];
    uint16_t max_y = point->history_y[0];
    
    for (uint8_t i = 1; i < point->history_count; i++) {
        if (point->history_x[i] < min_x) min_x = point->history_x[i];
        if (point->history_x[i] > max_x) max_x = point->history_x[i];
        if (point->history_y[i] < min_y) min_y = point->history_y[i];
        if (point->history_y[i] > max_y) max_y = point->history_y[i];
    }
    
    uint16_t range_x = max_x - min_x;
    uint16_t range_y = max_y - min_y;
    
    return (range_x <= TOUCH_STABLE_THRESH && range_y <= TOUCH_STABLE_THRESH);
}

/* ========================================================================
 *  РАСЧЁТ РАССТОЯНИЯ ДЛЯ PINCH
 * ======================================================================== */

uint16_t TouchGestures_PinchDistance(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    return (uint16_t)sqrtf(Point_DistSq(x1, y1, x2, y2));
}

/* ========================================================================
 *  ПОЛУЧЕНИЕ ВРЕМЕНИ И ПОЗИЦИИ ПОСЛЕДНЕГО ТАПА
 * ======================================================================== */

uint32_t TouchGestures_GetLastTapTick(void) {
    return g_state.last_tap_tick;
}

void TouchGestures_GetLastTapPos(uint16_t* x, uint16_t* y) {
    if (x) *x = g_state.last_tap_x;
    if (y) *y = g_state.last_tap_y;
}
