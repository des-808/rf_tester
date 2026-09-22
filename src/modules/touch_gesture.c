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
    g_state.event_ready = 0;
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
    
    /* ===== PINCH TRACKING (мультитач) ===== */
    if (g_state.point_count >= 2) {
        /* Ищем вторую активную точку */
        for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
            if (g_state.points[i].is_active && &g_state.points[i] != point) {
                uint16_t current_dist = TouchGestures_PinchDistance(
                    point->x, point->y, g_state.points[i].x, g_state.points[i].y);
                
                if (g_state.pinch_confirmed) {
                    int16_t delta = (int16_t)(current_dist - g_state.last_pinch_dist);
                    if (delta < 0) delta = -delta;
                    
                    if (delta >= TOUCH_PINCH_MIN_DELTA) {
                        g_state.current_event.type = (current_dist > g_state.last_pinch_dist)
                                                      ? TOUCH_GESTURE_PINCH_OUT
                                                      : TOUCH_GESTURE_PINCH_IN;
                        g_state.current_event.direction = TOUCH_DIR_NONE;
                        g_state.current_event.point_count = 2;
                        g_state.current_event.x = (point->x + g_state.points[i].x) / 2;
                        g_state.current_event.y = (point->y + g_state.points[i].y) / 2;
                        g_state.current_event.pinch_delta = (uint16_t)delta;
                        g_state.current_event.duration = HAL_GetTick() - point->press_tick;
                        g_state.event_ready = 1;
                    }
                } else {
                    int16_t delta = (int16_t)(current_dist - g_state.last_pinch_dist);
                    if (delta < 0) delta = -delta;
                    
                    if (delta >= TOUCH_PINCH_MIN_DELTA) {
                        g_state.pinch_confirmed = 1;
                        g_state.last_pinch_dist = current_dist;
                    }
                }
                break;
            }
        }
    }
    
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
 * @brief Обработка отпускания точки (распознавание жеста)
 * @param point указатель на точку
 * @return событие жеста (TOUCH_GESTURE_NONE если нет)
 */
TouchGesture_t TouchGestures_PointUp(TouchPoint_t* point) {
    if (!point || !point->is_active) return TOUCH_GESTURE_NONE;
    
    uint32_t now = HAL_GetTick();
    uint32_t duration = now - point->press_tick;
    TouchGesture_t gesture = TOUCH_GESTURE_NONE;
    
    /* ===== PINCH (мультитач) ===== */
    /* Pinch уже отслеживается в PointMove — здесь проверяем, был ли сгенерирован event */
    if (g_state.event_ready && 
        (g_state.current_event.type == TOUCH_GESTURE_PINCH_IN || 
         g_state.current_event.type == TOUCH_GESTURE_PINCH_OUT)) {
        gesture = g_state.current_event.type;
    }
    
    /* ===== SINGLE-TOUCH ЖЕСТЫ ===== */
    if (gesture == TOUCH_GESTURE_NONE) {
        uint16_t dx = (point->x > point->start_x) 
                      ? (point->x - point->start_x) 
                      : (point->start_x - point->x);
        uint16_t dy = (point->y > point->start_y) 
                      ? (point->y - point->start_y) 
                      : (point->start_y - point->y);
        uint16_t dist = (uint16_t)sqrtf((float)(dx * dx + dy * dy));
        
        /* Long press */
        if (duration >= TOUCH_LONG_PRESS_MIN && duration <= TOUCH_LONG_PRESS_MAX) {
            if (dist <= TOUCH_TAP_MAX_DIST) {
                gesture = TOUCH_GESTURE_LONG_PRESS;
                
                g_state.current_event.type = gesture;
                g_state.current_event.direction = TOUCH_DIR_NONE;
                g_state.current_event.point_count = 1;
                g_state.current_event.x = point->x;
                g_state.current_event.y = point->y;
                g_state.current_event.duration = duration;
                g_state.event_ready = 1;
            }
        }
        
        /* Swipe */
        if (gesture == TOUCH_GESTURE_NONE && dist >= TOUCH_SWIPE_MIN_DIST) {
            TouchDirection_t dir = TOUCH_DIR_NONE;
            int16_t vel_x = 0, vel_y = 0;
            
            if (TouchGestures_GetSwipeDirection(point, &dir, &vel_x, &vel_y)) {
                /* Проверяем скорость — swipe должен быть быстрым */
                int16_t abs_vel_x = (vel_x < 0) ? -vel_x : vel_x;
                int16_t abs_vel_y = (vel_y < 0) ? -vel_y : vel_y;
                int16_t max_vel = (abs_vel_x > abs_vel_y) ? abs_vel_x : abs_vel_y;
                
                if (max_vel >= TOUCH_SWIPE_MIN_VEL && duration <= TOUCH_SWIPE_MAX_TIME) {
                    gesture = TOUCH_GESTURE_SWIPE;
                    
                    g_state.current_event.type = gesture;
                    g_state.current_event.direction = dir;
                    g_state.current_event.point_count = 1;
                    g_state.current_event.x = point->x;
                    g_state.current_event.y = point->y;
                    g_state.current_event.velocity_x = vel_x;
                    g_state.current_event.velocity_y = vel_y;
                    g_state.current_event.duration = duration;
                    g_state.event_ready = 1;
                }
            }
        }
        
        /* Tap / Double tap */
        if (gesture == TOUCH_GESTURE_NONE && dist <= TOUCH_TAP_MAX_DIST) {
            /* Проверяем double tap */
            uint32_t time_since_last_tap = now - g_state.last_tap_tick;
            if (time_since_last_tap < TOUCH_TAP_MAX_TIME) {
                uint16_t tap_dx = (point->x > g_state.last_tap_x)
                                  ? (point->x - g_state.last_tap_x)
                                  : (g_state.last_tap_x - point->x);
                uint16_t tap_dy = (point->y > g_state.last_tap_y)
                                  ? (point->y - g_state.last_tap_y)
                                  : (g_state.last_tap_y - point->y);
                uint16_t tap_dist = (uint16_t)sqrtf((float)(tap_dx * tap_dx + tap_dy * tap_dy));
                
                if (tap_dist <= TOUCH_TAP_MAX_DIST) {
                    gesture = TOUCH_GESTURE_DOUBLE_TAP;
                    
                    g_state.current_event.type = gesture;
                    g_state.current_event.direction = TOUCH_DIR_NONE;
                    g_state.current_event.point_count = 1;
                    g_state.current_event.x = point->x;
                    g_state.current_event.y = point->y;
                    g_state.current_event.duration = duration;
                    g_state.event_ready = 1;
                }
            }
            
            if (gesture == TOUCH_GESTURE_NONE) {
                gesture = TOUCH_GESTURE_TAP;
                
                g_state.current_event.type = gesture;
                g_state.current_event.direction = TOUCH_DIR_NONE;
                g_state.current_event.point_count = 1;
                g_state.current_event.x = point->x;
                g_state.current_event.y = point->y;
                g_state.current_event.duration = duration;
                g_state.event_ready = 1;
            }
            
            g_state.last_tap_x = point->x;
            g_state.last_tap_y = point->y;
            g_state.last_tap_tick = now;
        }
        
        /* Drag (если точка двигалась, но жест не определен) */
        if (gesture == TOUCH_GESTURE_NONE && dist >= TOUCH_DRAG_MIN_DIST) {
            gesture = TOUCH_GESTURE_DRAG;
            
            g_state.current_event.type = gesture;
            g_state.current_event.direction = TOUCH_DIR_NONE;
            g_state.current_event.point_count = 1;
            g_state.current_event.x = point->x;
            g_state.current_event.y = point->y;
            g_state.current_event.duration = duration;
            g_state.event_ready = 1;
        }
    }
    
    /* Сброс состояния pinch */
    if (gesture != TOUCH_GESTURE_PINCH_IN && gesture != TOUCH_GESTURE_PINCH_OUT) {
        g_state.pinch_confirmed = 0;
    }
    
    /* Сброс точки */
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
    
    return gesture;
}

/**
 * @brief Получить количество активных точек
 */
uint8_t TouchGestures_GetPointCount(void) {
    return g_state.point_count;
}

/**
 * @brief Получить указатель на точку по индексу
 */
TouchPoint_t* TouchGestures_GetPoint(uint8_t index) {
    if (index >= TOUCH_MAX_POINTS) return NULL;
    return &g_state.points[index];
}

/* ========================================================================
 *  ПОЛУЧЕНИЕ СОБЫТИЯ И СБРОС
 * ======================================================================== */

/**
 * @brief Получить готовое событие жеста
 */
bool TouchGestures_GetEvent(TouchGesture_Event_t* event) {
    if (!event) return false;
    if (!g_state.event_ready) return false;
    
    memcpy(event, &g_state.current_event, sizeof(TouchGesture_Event_t));
    return true;
}

/**
 * @brief Сбросить готовое событие (после обработки)
 */
void TouchGestures_ClearEvent(void) {
    g_state.event_ready = 0;
}

/* ========================================================================
 *  ОПРЕДЕЛЕНИЕ НАПРАВЛЕНИЯ SWIPE
 * ======================================================================== */

/**
 * @brief Получить направление swipe по истории точек
 */
bool TouchGestures_GetSwipeDirection(TouchPoint_t* point,
                                      TouchDirection_t* direction,
                                      int16_t* velocity_x,
                                      int16_t* velocity_y) {
    if (!point || point->history_count < 2) return false;
    
    /* Индекс первого сэмпла в кольцевом буфере */
    uint8_t first_idx = (point->history_head - point->history_count + TOUCH_HISTORY_SIZE) % TOUCH_HISTORY_SIZE;
    
    uint16_t start_x = point->history_x[first_idx];
    uint16_t start_y = point->history_y[first_idx];
    uint32_t start_ts = point->history_ts[first_idx];
    
    /* Последний сэмпл */
    uint8_t last_idx = (first_idx + point->history_count - 1) % TOUCH_HISTORY_SIZE;
    uint16_t end_x = point->history_x[last_idx];
    uint16_t end_y = point->history_y[last_idx];
    uint32_t end_ts = point->history_ts[last_idx];
    
    int32_t dx = (int32_t)end_x - (int32_t)start_x;
    int32_t dy = (int32_t)end_y - (int32_t)start_y;
    uint32_t dt = end_ts - start_ts;
    
    if (dt == 0) return false;
    
    /* Скорость в px/sec */
    *velocity_x = (int16_t)((dx * 1000) / dt);
    *velocity_y = (int16_t)((dy * 1000) / dt);
    
    /* Определяем направление по большей оси */
    int32_t abs_dx = (dx < 0) ? -dx : dx;
    int32_t abs_dy = (dy < 0) ? -dy : dy;
    
    if (abs_dx > abs_dy) {
        *direction = (dx > 0) ? TOUCH_DIR_RIGHT : TOUCH_DIR_LEFT;
    } else {
        *direction = (dy > 0) ? TOUCH_DIR_DOWN : TOUCH_DIR_UP;
    }
    
    return true;
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
