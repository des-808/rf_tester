/**
 * @file touch_gesture.h
 * @brief Система отслеживания касаний и распознавания жестов
 * 
 * Вдохновлено LVGL input device system:
 * - Point history buffer для анализа движения
 * - State machine для распознавания жестов
 * - Multi-touch поддержка (до 5 точек FT6336U)
 * - Swipe, tap, long-press, pinch-zoom, drag
 */

#ifndef TOUCH_GESTURE_H
#define TOUCH_GESTURE_H

#include <stdint.h>
#include <stdbool.h>

/* ===== КОНФИГУРАЦИЯ ===== */

#define TOUCH_MAX_POINTS          5       /* Макс. точек (FT6336U поддерживает 5) */
#define TOUCH_HISTORY_SIZE        16      /* Размер буфера истории точек */
#define TOUCH_SAMPLE_MS           15      /* Минимальный интервал между сэмплами (мс) */

/* Tap */
#define TOUCH_TAP_MAX_DIST        15      /* Макс. смещение для тапа (px) */
#define TOUCH_TAP_MAX_TIME        500     /* Макс. время нажатия для тапа (мс) */

/* Long press */
#define TOUCH_LONG_PRESS_MIN      400     /* Мин. время для long press (мс) */
#define TOUCH_LONG_PRESS_MAX      2000    /* Макс. время (после этого — ignore) */

/* Swipe */
#define TOUCH_SWIPE_MIN_DIST      40      /* Мин. расстояние для swipe (px) */
#define TOUCH_SWIPE_MIN_VEL       60      /* Мин. скорость для swipe (px/сек) */
#define TOUCH_SWIPE_MAX_TIME      600     /* Макс. время для swipe (мс) */

/* Pinch */
#define TOUCH_PINCH_MIN_DELTA     15      /* Мин. изменение расстояния для pinch (px) */

/* Drag */
#define TOUCH_DRAG_MIN_DIST       5       /* Мин. расстояние для начала drag (px) */

/* Stability */
#define TOUCH_STABLE_THRESH       3       /* Макс. размах для "стабильности" (px) */

/* ===== ТИПЫ ЖЕСТОВ ===== */

typedef enum {
    TOUCH_GESTURE_NONE = 0,       /* Нет жеста */
    TOUCH_GESTURE_TAP,            /* Короткий тап */
    TOUCH_GESTURE_DOUBLE_TAP,     /* Двойной тап */
    TOUCH_GESTURE_LONG_PRESS,     /* Долгое нажатие */
    TOUCH_GESTURE_SWIPE,          /* Свайп */
    TOUCH_GESTURE_DRAG,           /* Перетаскивание */
    TOUCH_GESTURE_PINCH_IN,       /* Щипок сближение (zoom out) */
    TOUCH_GESTURE_PINCH_OUT,      /* Щипок разведение (zoom in) */
} TouchGesture_t;

/* ===== НАПРАВЛЕНИЕ ===== */

typedef enum {
    TOUCH_DIR_NONE = 0,
    TOUCH_DIR_UP,
    TOUCH_DIR_DOWN,
    TOUCH_DIR_LEFT,
    TOUCH_DIR_RIGHT,
} TouchDirection_t;

/* ===== СОБЫТИЕ ЖЕСТА ===== */

typedef struct {
    TouchGesture_t  type;             /* Тип жеста */
    TouchDirection_t direction;       /* Направление (для swipe) */
    uint8_t         point_count;      /* Кол-во точек */
    uint16_t        x;                /* Позиция события */
    uint16_t        y;
    int16_t         velocity_x;       /* Скорость X (px/sec) */
    int16_t         velocity_y;       /* Скорость Y (px/sec) */
    uint16_t        pinch_delta;      /* Изменение расстояния (для pinch) */
    uint32_t        duration;         /* Длительность нажатия (мс) */
} TouchGesture_Event_t;

/* ===== ОДНА ТОЧКА КОНТАКТА ===== */

typedef struct {
    uint8_t         id;               /* ID точки (0-4 от FT6336U) */
    uint16_t        x;                /* Текущая X */
    uint16_t        y;                /* Текущая Y */
    uint16_t        start_x;          /* X начала нажатия */
    uint16_t        start_y;          /* Y начала нажатия */
    uint32_t        press_tick;       /* Время начала нажатия (HAL_GetTick) */
    uint8_t         is_active;        /* Точка активна (палец на экране) */
    uint8_t         is_new;           /* Новая точка (только создана) */
    
    /* История позиций для анализа жеста */
    uint16_t        history_x[TOUCH_HISTORY_SIZE];
    uint16_t        history_y[TOUCH_HISTORY_SIZE];
    uint32_t        history_ts[TOUCH_HISTORY_SIZE];
    uint8_t         history_count;    /* Кол-во сэмплов в истории */
    uint8_t         history_head;     /* Индекс следующего сэмпла (кольцевой) */
    
    /* Состояние жеста */
    uint8_t         gesture_started;  /* Жест уже определён */
    uint8_t         drag_confirmed;   /* Drag подтверждён (> DRAG_MIN_DIST) */
} TouchPoint_t;

/* ===== СОСТОЯНИЕ СИСТЕМЫ ===== */

typedef struct {
    TouchPoint_t    points[TOUCH_MAX_POINTS];
    TouchGesture_Event_t current_event;   /* Текущее событие */
    uint8_t         point_count;          /* Активных точек */
    uint8_t         event_ready;          /* 1 = событие готово к обработке */
    
    /* Для double-tap */
    uint16_t        last_tap_x;
    uint16_t        last_tap_y;
    uint32_t        last_tap_tick;
    
    /* Для pinch */
    uint16_t        last_pinch_dist;
    uint8_t         pinch_confirmed;
} TouchGestures_State;

/* ===== API ===== */

/**
 * @brief Инициализация системы жестов
 */
void TouchGestures_Init(void);

/**
 * @brief Добавить новую точку контакта
 * @param x  координата X
 * @param y  координата Y
 * @param id ID точки от тач-контроллера (0-4)
 * @return указатель на созданную точку или NULL
 */
TouchPoint_t* TouchGestures_PointDown(uint16_t x, uint16_t y, uint8_t id);

/**
 * @brief Обновить существующую точку контакта
 * @param point указатель на точку (возвращена PointDown или PointFind)
 */
void TouchGestures_PointMove(TouchPoint_t* point, uint16_t x, uint16_t y);

/**
 * @brief Обработка отпускания точки
 * @param point указатель на точку
 * @return событие жеста (TOUCH_GESTURE_NONE если нет)
 */
TouchGesture_t TouchGestures_PointUp(TouchPoint_t* point);

/**
 * @brief Найти точку по координатам
 * @param x координата X
 * @param y координата Y
 * @return указатель на точку или NULL
 */
TouchPoint_t* TouchGestures_PointFind(uint16_t x, uint16_t y);

/**
 * @brief Получить количество активных точек
 */
uint8_t TouchGestures_GetPointCount(void);

/**
 * @brief Получить готовое событие жеста
 * @param event указатель на структуру для заполнения
 * @return true если событие готово
 */
bool TouchGestures_GetEvent(TouchGesture_Event_t* event);

/**
 * @brief Сбросить готовое событие (после обработки)
 */
void TouchGestures_ClearEvent(void);

/**
 * @brief Получить направление swipe по истории точек
 * @param point указатель на точку
 * @param direction[out] направление
 * @param velocity_x[out] скорость X
 * @param velocity_y[out] скорость Y
 * @return true если swipe определён
 */
bool TouchGestures_GetSwipeDirection(TouchPoint_t* point, 
                                      TouchDirection_t* direction,
                                      int16_t* velocity_x,
                                      int16_t* velocity_y);

/**
 * @brief Рассчитать расстояние между двумя точками (для pinch)
 * @param x1, y1 первая точка
 * @param x2, y2 вторая точка
 * @return расстояние в пикселях
 */
uint16_t TouchGestures_PinchDistance(uint16_t x1, uint16_t y1, 
                                      uint16_t x2, uint16_t y2);

/**
 * @brief Проверить стабильность точки (не двигается ли)
 * @param point указатель на точку
 * @return true если точка стабильна
 */
bool TouchGestures_IsPointStable(TouchPoint_t* point);

/**
 * @brief Получить время последнего тапа
 */
uint32_t TouchGestures_GetLastTapTick(void);

/**
 * @brief Получить позицию последнего тапа
 */
void TouchGestures_GetLastTapPos(uint16_t* x, uint16_t* y);

#endif /* TOUCH_GESTURE_H */
