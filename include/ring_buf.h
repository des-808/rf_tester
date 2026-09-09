/**
 * @file    ring_buf.h
 * @brief   Generic lock-free ring buffer for STM32
 * @note    Single-producer / single-consumer, interrupt-safe via critical section
 */

#ifndef RING_BUF_H
#define RING_BUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================================================================== */
/*  Конфигурация                                                             */
/* ======================================================================== */

/**
 * @brief Максимальный размер ring buffer (статический массив)
 *        Должен быть степенью двойки для оптимизации modulo
 */
#ifndef RING_BUF_MAX_SIZE
#define RING_BUF_MAX_SIZE   512U
#endif

/* ======================================================================== */
/*  Типы данных                                                              */
/* ======================================================================== */

/**
 * @brief Структура ring buffer
 * @tparam T — тип элементов (определяется через ring_buf_t_with_type)
 */
typedef struct {
    uint8_t     *data;          /* Указатель на буфер (статический) */
    uint16_t     mask;          /* mask = size - 1 (size must be power of 2) */
    volatile uint16_t  head;    /* Write pointer (producer) */
    volatile uint16_t  tail;    /* Read pointer  (consumer) */
    volatile uint16_t  count;   /* Количество элементов в буфере */
} ring_buf_t;

/* ======================================================================== */
/*  Макросы для создания типизированных ring buffer                          */
/* ======================================================================== */

/**
 * @brief Создать типизированный ring buffer статически
 * @param name  Имя переменной ring buffer
 * @param buf   Имя статического массива-хранилища
 * @param size  Размер в элементах (степень двойки)
 * @param type  Тип элементов
 */
#define RING_BUF_DEFINE(name, buf, size, type)    \
    static type buf[size];                        \
    static ring_buf_t name = {                    \
        .data = (uint8_t*)buf,                    \
        .mask = (size) - 1,                       \
        .head = 0,                                \
        .tail = 0,                                \
        .count = 0                                \
    }

/**
 * @brief Инициализировать ring buffer (динамический буфер)
 * @param rb   Указатель на ring_buf_t
 * @param buf  Указатель на статический массив-хранилище
 * @param size Размер в элементах (степень двойки)
 */
static __INLINE void ring_buf_init(ring_buf_t *rb, uint8_t *buf, uint16_t size)
{
    rb->data    = buf;
    rb->mask    = size - 1;
    rb->head    = 0;
    rb->tail    = 0;
    rb->count   = 0;
}

/**
 * @brief Сбросить ring buffer
 */
static __INLINE void ring_buf_reset(ring_buf_t *rb)
{
    rb->head    = 0;
    rb->tail    = 0;
    rb->count   = 0;
}

/* ======================================================================== */
/*  Базовые операции                                                         */
/* ======================================================================== */

/**
 * @brief Проверить, пуст ли ring buffer
 */
static __INLINE bool ring_buf_is_empty(const ring_buf_t *rb)
{
    return rb->count == 0;
}

/**
 * @brief Проверить, полон ли ring buffer
 */
static __INLINE bool ring_buf_is_full(const ring_buf_t *rb)
{
    return rb->count >= (rb->mask + 1);
}

/**
 * @brief Количество доступных элементов для чтения
 */
static __INLINE uint16_t ring_buf_count(const ring_buf_t *rb)
{
    return rb->count;
}

/**
 * @brief Количество свободных слотов
 */
static __INLINE uint16_t ring_buf_space(const ring_buf_t *rb)
{
    return (rb->mask + 1) - rb->count;
}

/* ======================================================================== */
/*  Критические секции (STM32 — отключение прерываний)                       */
/* ======================================================================== */

/** Войти в критическую секцию */
#define RING_BUF_ENTER_CRITICAL()     __disable_irq()

/** Выйти из критической секции */
#define RING_BUF_EXIT_CRITICAL()      __enable_irq()

/* ======================================================================== */
/*  Producer: запись в ring buffer                                             */
/* ======================================================================== */

/**
 * @brief Поместить элемент в ring buffer (вызывается из producer)
 * @param rb   Указатель на ring buffer
 * @param val  Значение для записи
 * @return true если успешно
 */
static __INLINE bool ring_buf_push(ring_buf_t *rb, uint16_t val)
{
    RING_BUF_ENTER_CRITICAL();

    if (ring_buf_is_full(rb)) {
        RING_BUF_EXIT_CRITICAL();
        return false;  /* Буфер полон — переполнение */
    }

    uint16_t idx = rb->head & rb->mask;
    uint16_t *buf = (uint16_t*)rb->data;
    buf[idx] = val;

    rb->head++;
    rb->count++;

    RING_BUF_EXIT_CRITICAL();
    return true;
}

/**
 * @brief Поместить элемент в ring buffer (без критической секции)
 * @note  Вызывать ТОЛЬКО из контекста с уже отключёнными прерываниями
 */
static __INLINE bool ring_buf_push_raw(ring_buf_t *rb, uint16_t val)
{
    if (ring_buf_is_full(rb)) {
        return false;
    }

    uint16_t idx = rb->head & rb->mask;
    uint16_t *buf = (uint16_t*)rb->data;
    buf[idx] = val;

    rb->head++;
    rb->count++;

    return true;
}

/* ======================================================================== */
/*  Consumer: чтение из ring buffer                                          */
/* ======================================================================== */

/**
 * @brief Прочитать элемент из ring buffer (вызывается из consumer)
 * @param rb   Указатель на ring buffer
 * @param val  Указатель на переменную для чтения
 * @return true если успешно (элемент прочитан)
 */
static __INLINE bool ring_buf_pop(ring_buf_t *rb, uint16_t *val)
{
    RING_BUF_ENTER_CRITICAL();

    if (ring_buf_is_empty(rb)) {
        RING_BUF_EXIT_CRITICAL();
        return false;  /* Буфер пуст */
    }

    uint16_t idx = rb->tail & rb->mask;
    uint16_t *buf = (uint16_t*)rb->data;
    *val = buf[idx];

    rb->tail++;
    rb->count--;

    RING_BUF_EXIT_CRITICAL();
    return true;
}

/**
 * @brief Прочитать элемент из ring buffer (без критической секции)
 * @note  Вызывать ТОЛЬКО из контекста с уже отключёнными прерываниями
 */
static __INLINE bool ring_buf_pop_raw(ring_buf_t *rb, uint16_t *val)
{
    if (ring_buf_is_empty(rb)) {
        return false;
    }

    uint16_t idx = rb->tail & rb->mask;
    uint16_t *buf = (uint16_t*)rb->data;
    *val = buf[idx];

    rb->tail++;
    rb->count--;

    return true;
}

/**
 * @brief Прочитать последний элемент без удаления (peek)
 */
static __INLINE bool ring_buf_peek(const ring_buf_t *rb, uint16_t *val)
{
    RING_BUF_ENTER_CRITICAL();

    if (ring_buf_is_empty(rb)) {
        RING_BUF_EXIT_CRITICAL();
        return false;
    }

    uint16_t idx = rb->tail & rb->mask;
    uint16_t *buf = (uint16_t*)rb->data;
    *val = buf[idx];

    RING_BUF_EXIT_CRITICAL();
    return true;
}

/**
 * @brief Скопировать все доступные элементы в массив
 * @param rb       Указатель на ring buffer
 * @param out      Буфер для вывода
 * @param max_len  Максимальное количество элементов для копирования
 * @return реальное количество скопированных элементов
 */
static __INLINE uint16_t ring_buf_copy_to(ring_buf_t *rb, uint16_t *out, uint16_t max_len)
{
    RING_BUF_ENTER_CRITICAL();

    uint16_t available = rb->count;
    uint16_t count = (available < max_len) ? available : max_len;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (rb->tail + i) & rb->mask;
        uint16_t *buf = (uint16_t*)rb->data;
        out[i] = buf[idx];
    }

    rb->tail   += count;
    rb->count  -= count;

    RING_BUF_EXIT_CRITICAL();
    return count;
}

/**
 * @brief Пропустить N элементов в ring buffer (удалить без чтения)
 * @param rb  Указатель на ring buffer
 * @param n   Количество элементов для пропуска
 * @return реальное количество пропущенных элементов
 */
static __INLINE uint16_t ring_buf_skip(ring_buf_t *rb, uint16_t n)
{
    RING_BUF_ENTER_CRITICAL();

    uint16_t available = rb->count;
    uint16_t count = (n < available) ? n : available;

    rb->tail   += count;
    rb->count  -= count;

    RING_BUF_EXIT_CRITICAL();
    return count;
}

#endif /* RING_BUF_H */
