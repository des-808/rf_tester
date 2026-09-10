#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "gui.h"

/* ========================================================================
 *  PAGE SYSTEM — UIElement-контейнеры, заменяющие ListBox меню
 * ========================================================================
 *
 *  Страница (Page) — это UIElement-контейнер (Grid/StackPanel), который
 *  рисуется на месте ListBox меню. При открытии страницы ListBox collapse,
 *  при закрытии — restore.
 *
 *  Два режима создания:
 *    1. Статические — определены как static PageDef_t в .c файлах модулей.
 *       Создаются один раз, не требуют аллокации.
 *    2. Динамические — создаются через Page_CreateDynamic(), удаляются через
 *       Page_DestroyDynamic().
 *
 *  Стек страниц:
 *    Page_Open()   — push (сохраняет текущее состояние)
 *    Page_Close()  — pop (восстанавливает предыдущий уровень)
 * ======================================================================== */

// Максимальная глубина вложенности страниц
#define MAX_PAGE_DEPTH  5

/* ------------------------------------------------------------------------
 *  Page lifecycle callbacks
 * ------------------------------------------------------------------------ */

/**
 * @brief Колбэк инициализации страницы (вызывается один раз при открытии)
 * @param page Указатель на страницу
 * @param parent_container Родительский контейнер (digits_node)
 * @return true если страница успешно инициализирована
 */
typedef bool (*Page_InitCallback)(UIElement_t* page, UIElement_t* parent_container);

/**
 * @brief Колбэк отрисовки страницы (вызывается через render_callback UI_DrawTree)
 * @param page Указатель на страницу
 */
typedef void (*Page_DrawCallback)(UIElement_t* page);

/**
 * @brief Колбэк обновления страницы (вызывается каждый кадр из main loop)
 *        Для страниц, которым нужно обновлять данные без ожидания отрисовки
 *        (RSSI Plotter — чтение из ring buffer, спектр-анализатор и т.п.)
 * @param page Указатель на страницу
 */
typedef void (*Page_UpdateCallback)(UIElement_t* page);

/**
 * @brief Колбэк обработки ввода (вызывается при нажатии кнопок)
 * @param page Указатель на страницу
 * @param key KEY_UP, KEY_DOWN, KEY_ENTER, KEY_CANCEL
 * @return true если ввод обработан (например, KEY_CANCEL = back)
 */
typedef bool (*Page_InputCallback)(UIElement_t* page, uint8_t key);

/**
 * @brief Колбэк деинициализации (вызывается при закрытии страницы)
 * @param page Указатель на страницу
 */
typedef void (*Page_DeinitCallback)(UIElement_t* page);

/* ------------------------------------------------------------------------
 *  Page definition (static)
 * ------------------------------------------------------------------------ */

/**
 * @brief Описание статической страницы
 */
typedef struct {
    const char* name;           // Имя страницы (для отладки)
    UIType_t    container_type; // UI_TYPE_GRID или UI_TYPE_STACK_PANEL
    uint8_t     rows;           // Для Grid: количество строк
    uint8_t     cols;           // Для Grid: количество колонок
    uint16_t    spacing;        // Отступы между детьми (для StackPanel)
    Orientation_t orientation;  // Ориентация StackPanel (ORIENTATION_VERTICAL / HORIZONTAL)
    
    // Callbacks
    Page_InitCallback   on_init;
    Page_DrawCallback   on_draw;
    Page_UpdateCallback on_update;   // Обновление данных (каждый кадр)
    Page_InputCallback  on_input;
    Page_DeinitCallback on_deinit;
    
    // Указатель на пользовательские данные (если нужно)
    void* user_data;
} PageDef_t;

/* ------------------------------------------------------------------------
 *  Dynamic page (created at runtime)
 * ------------------------------------------------------------------------ */

/**
 * @brief Динамическая страница (создаётся через Page_CreateDynamic)
 */
typedef struct {
    UIElement_t   element;    // UIElement-контейнер страницы
    PageDef_t     def;        // Описание страницы
    bool          is_dynamic; // true для динамических, false для статических
} DynamicPage_t;

/* ------------------------------------------------------------------------
 *  Page stack entry (state saved when switching pages)
 * ------------------------------------------------------------------------ */

/**
 * @brief Запись в стеке страниц
 */
typedef struct {
    UIElement_t*  saved_container;  // Сохранённый контейнер (ListBox или Page)
    bool          was_listbox;      // true если это было меню (ListBox)
    uint8_t       list_scroll;      // Scroll offset ListBox (если was_listbox)
    int16_t       list_selected;    // Selected index (если was_listbox)
} PageStackEntry_t;

/* ========================================================================
 *  API — Инициализация
 * ======================================================================== */

/**
 * @brief Инициализировать систему страниц (вызывается один раз при старте)
 */
void Page_Init(void);

/* ========================================================================
 *  API — Статические страницы
 * ======================================================================== */

/**
 * @brief Открыть статическую страницу
 * @param def Описание страницы (static PageDef_t)
 * @param parent_container Родительский контейнер (digits_node)
 * @return true если страница открыта
 *
 * Пример:
 *   static PageDef_t my_page = { ... };
 *   Page_OpenStatic(&my_page, &digits_node);
 */
bool Page_OpenStatic(PageDef_t* def, UIElement_t* parent_container);

/**
 * @brief Закрыть текущую статическую страницу
 * @return true если страница закрыта и возвращено меню
 */
bool Page_CloseStatic(void);

/* ========================================================================
 *  API — Динамические страницы
 * ======================================================================== */

/**
 * @brief Создать и открыть динамическую страницу
 * @param def Описание страницы
 * @param parent_container Родительский контейнер
 * @return указатель на DynamicPage_t или NULL при ошибке
 */
DynamicPage_t* Page_OpenDynamic(PageDef_t* def, UIElement_t* parent_container);

/**
 * @brief Закрыть и удалить динамическую страницу
 * @param page Страница для закрытия
 * @return true если страница закрыта и возвращено меню
 */
bool Page_CloseDynamic(DynamicPage_t* page);

/* ========================================================================
 *  API — Общие функции
 * ======================================================================== */

/**
 * @brief Закрыть текущую страницу (любого типа)
 * @return true если страница закрыта и возвращено меню
 *
 * Автоматически определяет тип страницы (static/dynamic) и вызывает
 * соответствующую функцию.
 */
bool Page_CloseCurrent(void);

/**
 * @brief Проверить, активна ли страница
 * @return true если страница открыта
 */
bool Page_IsActive(void);

/**
 * @brief Получить текущую активную страницу
 * @return указатель на UIElement_t текущей страницы или NULL
 */
UIElement_t* Page_GetCurrent(void);

/**
 * @brief Получить текущее описание страницы
 * @return указатель на PageDef_t или NULL
 */
PageDef_t* Page_GetCurrentDef(void);

/* ========================================================================
 *  API — Рендеринг и ввод
 * ======================================================================== */

/**
 * @brief Отрисовать текущую страницу (вызывается из GUI loop)
 */
void Page_DrawCurrent(void);

/**
 * @brief Обновить данные текущей страницы (вызывается из main loop, каждый кадр)
 *        Вызывает on_update, если он определён.
 */
void Page_UpdateAll(void);

/**
 * @brief Передать ввод текущей странице (вызывается из Menu_ProcessInput)
 * @param key KEY_UP, KEY_DOWN, KEY_ENTER, KEY_CANCEL
 * @return true если ввод обработан страницей
 */
bool Page_ProcessInput(uint8_t key);

/* ========================================================================
 *  Хелперы для построения UI внутри страниц
 * ======================================================================== */

/**
 * @brief Создать Grid-контейнер в пуле panel_rows
 * @param parent Родительский контейнер (добавит себя как child)
 * @param rows Количество строк
 * @param cols Количество колонок
 * @return указатель на UIElement_t или NULL
 */
UIElement_t* Page_CreateGrid(UIElement_t* parent, uint8_t rows, uint8_t cols);

/**
 * @brief Создать StackPanel-контейнер в пуле panel_rows
 * @param parent Родительский контейнер (добавит себя как child)
 * @param orientation Ориентация
 * @param spacing Отступы между детьми
 * @return указатель на UIElement_t или NULL
 */
UIElement_t* Page_CreateStackPanel(UIElement_t* parent, Orientation_t orientation, uint16_t spacing);

/**
 * @brief Добавить текстовую строку в StackPanel
 * @param panel StackPanel-контейнер
 * @param text Текст строки
 * @return указатель на UIElement_t или NULL
 */
UIElement_t* Page_AddText(UIElement_t* panel, const char* text);

/**
 * @brief Добавить текстовую строку с форматированием
 * @param panel StackPanel-контейнер
 * @param format printf-формат
 */
void Page_AddTextF(UIElement_t* panel, const char* format, ...);

/**
 * @brief Настроить Grid row как pixel
 * @param grid Grid-контейнер
 * @param row_idx Индекс строки
 * @param size_px Размер в пикселях
 */
void Page_SetGridRowPixel(UIElement_t* grid, uint8_t row_idx, uint8_t size_px);

/**
 * @brief Настроить Grid col как percent
 * @param grid Grid-контейнер
 * @param col_idx Индекс колонки
 * @param percent Процент
 */
void Page_SetGridColPercent(UIElement_t* grid, uint8_t col_idx, uint8_t percent);

/**
 * @brief Настроить Grid col как pixel
 * @param grid Grid-контейнер
 * @param col_idx Индекс колонки
 * @param size_px Размер в пикселях
 */
void Page_SetGridColPixel(UIElement_t* grid, uint8_t col_idx, uint8_t size_px);

/* ========================================================================
 *  Примеры (для справки)
 * ======================================================================== */

/** Открыть страницу настроек CC1101 */
void Page_OpenSettings(void);

/** Открыть страницу About */
void Page_OpenAbout(void);

/** Показать диалог подтверждения */
void Page_ShowConfirm(const char* message, void (*on_confirm)(void), void (*on_cancel)(void));

/** Открыть динамическую страницу Frequency Analyzer */
bool Page_OpenFreqAnalyzer(uint32_t freq_mhz, uint32_t bitrate, uint8_t rxbw_index);

/** Открыть RSSI Plotter как страницу */
void Page_OpenRssiPlotter(void);

/** Получить описание RSSI страницы (для menu.c) */
PageDef_t* Page_GetRssiPageDef(void);

#endif // PAGE_H
