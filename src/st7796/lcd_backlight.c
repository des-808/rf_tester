#include "lcd_backlight.h"
#include <stdbool.h>

extern TIM_HandleTypeDef htim1;

// ============================================================
// КОНВЕРТЕР УРОВЕНЬ -> PWM (CCR2)
// ============================================================
// API: 0 = выкл, 1-10 = тускло -> ярко
// TIM1: Period=416, PWM1 режим, CH2N (инвертированный)
// level=0  -> CCR2=416 -> LOW на выходе (выкл)
// level=10 -> CCR2=0   -> HIGH на выходе (макс яркость)

static uint32_t LCD_Backlight_LevelToCCR2(uint8_t level) {
    if (level == 0) return 416;  // Полностью выключено
    // level 1-10 -> CCR2 416-0 (инвертировано для CH2N)
    return (uint32_t)(LCD_BACKLIGHT_LEVELS - level) * 416 / LCD_BACKLIGHT_LEVELS;
}

uint8_t LCD_Backlight_PWMToLevel(uint32_t ccr2) {
    if (ccr2 >= 416) return 0;    // Выключено
    // CCR2 0-415 -> level 10-1
    uint8_t level = (uint8_t)((416 - ccr2) * LCD_BACKLIGHT_LEVELS / 416);
    if (level == 0) level = 1;
    if (level > LCD_BACKLIGHT_LEVELS) level = LCD_BACKLIGHT_LEVELS;
    return level;
}

// ============================================================
// СОСТОЯНИЕ ПЛАВНОЙ АНИМАЦИИ
// ============================================================
typedef enum {
    BACKLIGHT_IDLE = 0,
    BACKLIGHT_SMOOTH_OFF,
    BACKLIGHT_SMOOTH_ON
} backlight_smooth_state_t;

static struct {
    backlight_smooth_state_t state;
    uint8_t current_level;    // Уровень меню (0-10)
    uint8_t target_level;     // Целевой уровень (0-10)
    uint32_t elapsed_ms;      //Elapsed время
    uint32_t duration_ms;     // Общая длительность
    uint32_t step_ms;         // Шаг обновления
    uint32_t last_step_time;  // Время последнего шага
} backlight_smooth = {
    .state = BACKLIGHT_IDLE,
    .current_level = 5,
    .target_level = 10
};

// ============================================================
// ОСНОВНЫЕ ФУНКЦИИ
// ============================================================

void LCD_Backlight_Init(void) {
    // Запуск PWM
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    htim1.Instance->CCER |= TIM_CCER_CC2NE;  // Включаем CH2N на PB0
    
    LCD_Backlight_SetLevel(5);  // Стартовый уровень — середина (1-10)
    backlight_smooth.current_level = 5;
    backlight_smooth.state = BACKLIGHT_IDLE;
}

void LCD_Backlight_SetLevel(uint8_t level) {
    if (level > LCD_BACKLIGHT_LEVELS) level = LCD_BACKLIGHT_LEVELS;
    uint32_t ccr2 = LCD_Backlight_LevelToCCR2(level);
    htim1.Instance->CCR2 = ccr2;
    backlight_smooth.current_level = level;
    backlight_smooth.state = BACKLIGHT_IDLE;
}

uint8_t LCD_Backlight_GetLevel(void) {
    return backlight_smooth.current_level;
}

// ============================================================
// ПЛАВНОЕ ЗАТИХАНИЕ
// ============================================================

void LCD_Backlight_SmoothOff(uint32_t duration_ms, uint32_t step_ms) {
    backlight_smooth.state = BACKLIGHT_SMOOTH_OFF;
    backlight_smooth.target_level = 0;
    backlight_smooth.elapsed_ms = 0;
    backlight_smooth.duration_ms = duration_ms;
    backlight_smooth.step_ms = step_ms;
    backlight_smooth.last_step_time = 0;
}

// ============================================================
// ПЛАВНОЕ РАЗГОРАНИЕ
// ============================================================

bool LCD_Backlight_SmoothOn(uint32_t duration_ms, uint8_t target_level, uint32_t step_ms) {
    if (target_level > LCD_BACKLIGHT_LEVELS) target_level = LCD_BACKLIGHT_LEVELS;
    backlight_smooth.state = BACKLIGHT_SMOOTH_ON;
    backlight_smooth.target_level = target_level;
    backlight_smooth.elapsed_ms = 0;
    backlight_smooth.duration_ms = duration_ms;
    backlight_smooth.step_ms = step_ms;
    backlight_smooth.last_step_time = 0;
    
    return false;
}

// ============================================================
// НЕБЛОКИРУЮЩЕЕ ОБНОВЛЕНИЕ (вызывать регулярно)
// ============================================================

void LCD_Backlight_SmoothUpdate(void) {
    if (backlight_smooth.state == BACKLIGHT_IDLE) {
        return;
    }
    
    uint32_t now = HAL_GetTick();
    
    // Проверяем, пора ли делать шаг
    if (now - backlight_smooth.last_step_time >= backlight_smooth.step_ms) {
        backlight_smooth.last_step_time = now;
        backlight_smooth.elapsed_ms += backlight_smooth.step_ms;
        
        // Вычисляем прогресс (0.0 — 1.0)
        float progress = (float)backlight_smooth.elapsed_ms / (float)backlight_smooth.duration_ms;
        if (progress > 1.0f) progress = 1.0f;
        
        uint8_t new_level = 0;
        bool finished = false;
        
        if (backlight_smooth.state == BACKLIGHT_SMOOTH_OFF) {
            // Затухание: от current_level до 0
            new_level = (uint8_t)((float)backlight_smooth.current_level * (1.0f - progress));
            if (progress >= 1.0f) {
                finished = true;
                new_level = 0;
            }
        } else if (backlight_smooth.state == BACKLIGHT_SMOOTH_ON) {
            // Разгорание: от 0 до target_level
            new_level = (uint8_t)((float)backlight_smooth.target_level * progress);
            if (progress >= 1.0f) {
                finished = true;
                new_level = backlight_smooth.target_level;
            }
        }
        
        // Применяем новый уровень
        backlight_smooth.current_level = new_level;
        LCD_Backlight_SetLevel(new_level);
        
        // Завершаем анимацию
        if (finished) {
            backlight_smooth.state = BACKLIGHT_IDLE;
        }
    }
}