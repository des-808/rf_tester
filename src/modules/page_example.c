/**
 * @file page_example.c
 * @brief Примеры использования Page System
 * 
 * Показывает как создавать статические и динамические страницы.
 */

#include "page.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>

/* ========================================================================
 *  Пример 1: Статическая страница — "Настройки CC1101"
 * ========================================================================
 * 
 * Статическая страница определяется один раз как static PageDef_t.
 * on_init создаёт UI-элементы (Grid/StackPanel с детьми).
 * on_draw вызывает перерисовку (если нужно).
 * on_input обрабатывает нажатия кнопок.
 * on_deinit очищает ресурсы.
 */

/* Внутренний UI-элемент страницы */
static UIElement_t* s_page_ui = NULL;

static bool Page_Settings_Init(UIElement_t* page, UIElement_t* parent)
{
    /* Создаём StackPanel для контента страницы */
    UIElement_t* panel = Page_CreateStackPanel(page, ORIENTATION_VERTICAL, 8);
    if (!panel) return false;
    
    /* Добавляем заголовок */
    Page_AddText(panel, "=== CC1101 Settings ===");
    Page_AddText(panel, ""); // пустая строка
    
    /* Добавляем параметры */
    Page_AddText(panel, "Freq: 433.96 MHz");
    Page_AddText(panel, "BitRate: 9.60 kbps");
    Page_AddText(panel, "RxBw: 406 kHz");
    Page_AddText(panel, "Mod: GFSK");
    Page_AddText(panel, "Power: -10 dBm");
    Page_AddText(panel, "");
    Page_AddText(panel, "[UP/DOWN] Navigate");
    Page_AddText(panel, "[ENTER] Edit");
    Page_AddText(panel, "[CANCEL] Back");
    
    s_page_ui = panel;
    return true;
}

static void Page_Settings_Draw(UIElement_t* page)
{
    /* Если нужно перерисовать контент страницы */
    if (page && page->sprite) {
        page->sprite->needs_render = true;
    }
}

static bool Page_Settings_Input(UIElement_t* page, uint8_t key)
{
    /* Обработка ввода для этой страницы */
    switch (key) {
        case KEY_UP:
            /* Пример: увеличить частоту */
            printf("Page: UP pressed\n");
            return true;
            
        case KEY_DOWN:
            /* Пример: уменьшить частоту */
            printf("Page: DOWN pressed\n");
            return true;
            
        case KEY_ENTER:
            /* Пример: войти в редактирование */
            printf("Page: ENTER pressed\n");
            return true;
            
        case KEY_CANCEL:
            /* KEY_CANCEL обрабатывается автоматически (Page_CloseCurrent) */
            return false;
    }
    return false;
}

static void Page_Settings_Deinit(UIElement_t* page)
{
    /* Очистка ресурсов */
    s_page_ui = NULL;
    printf("Page: Settings deinitialized\n");
}

/* Описание статической страницы */
static PageDef_t page_settings_def = {
    .name = "CC1101 Settings",
    .container_type = UI_TYPE_STACK_PANEL,
    .orientation = ORIENTATION_VERTICAL,
    .spacing = 8,
    .on_init = Page_Settings_Init,
    .on_draw = Page_Settings_Draw,
    .on_input = Page_Settings_Input,
    .on_deinit = Page_Settings_Deinit,
    .user_data = NULL
};

/* ========================================================================
 *  Пример 2: Статическая страница — "About"
 * ======================================================================== */

static bool Page_About_Init(UIElement_t* page, UIElement_t* parent)
{
    UIElement_t* panel = Page_CreateStackPanel(page, ORIENTATION_VERTICAL, 6);
    if (!panel) return false;
    
    Page_AddText(panel, "RF Tester v1.0");
    Page_AddText(panel, "ESP32 + ST7796");
    Page_AddText(panel, "");
    Page_AddText(panel, "Radio: CC1101, NRF24L01");
    Page_AddText(panel, "Radio: SX1262");
    Page_AddText(panel, "Display: 320x240 TFT");
    Page_AddText(panel, "Touch: FT6336U");
    Page_AddText(panel, "");
    Page_AddText(panel, "[CANCEL] Back");
    
    return true;
}

static void Page_About_Deinit(UIElement_t* page)
{
    printf("Page: About deinitialized\n");
}

static PageDef_t page_about_def = {
    .name = "About",
    .container_type = UI_TYPE_STACK_PANEL,
    .orientation = ORIENTATION_VERTICAL,
    .spacing = 6,
    .on_init = Page_About_Init,
    .on_draw = NULL,
    .on_input = NULL,
    .on_deinit = Page_About_Deinit,
    .user_data = NULL
};

/* ========================================================================
 *  Пример 3: Динамическая страница — "Подтверждение"
 * ======================================================================== */

/* Данные для диалога подтверждения */
typedef struct {
    const char* message;
    void (*on_confirm)(void);
    void (*on_cancel)(void);
} ConfirmData_t;

static bool Page_Confirm_Init(UIElement_t* page, UIElement_t* parent)
{
    ConfirmData_t* data = (ConfirmData_t*)page->user_data;
    
    UIElement_t* panel = Page_CreateStackPanel(page, ORIENTATION_VERTICAL, 10);
    if (!panel) return false;
    
    /* Рамка вокруг контента */
    UIElement_t* border = Page_CreateStackPanel(panel, ORIENTATION_VERTICAL, 8);
    if (!border) return false;
    
    border->background_color = RGB565_WHITE;
    
    Page_AddText(border, "CONFIRM?");
    if (data && data->message) {
        Page_AddText(border, data->message);
    }
    Page_AddText(border, "");
    Page_AddText(border, "[ENTER] Yes");
    Page_AddText(border, "[CANCEL] No");
    
    return true;
}

static bool Page_Confirm_Input(UIElement_t* page, uint8_t key)
{
    ConfirmData_t* data = (ConfirmData_t*)page->user_data;
    
    switch (key) {
        case KEY_ENTER:
            if (data && data->on_confirm) {
                data->on_confirm();
            }
            Page_CloseCurrent();
            return true;
            
        case KEY_CANCEL:
            if (data && data->on_cancel) {
                data->on_cancel();
            }
            Page_CloseCurrent();
            return true;
    }
    return false;
}

static void Page_Confirm_Deinit(UIElement_t* page)
{
    /* Освобождаем данные если они динамические */
    ConfirmData_t* data = (ConfirmData_t*)page->user_data;
    if (data && data->message) {
        /* data->message = heap_caps_free(data->message); */
    }
    if (data) {
        heap_caps_free(data);
    }
    page->user_data = NULL;
}

static PageDef_t page_confirm_def = {
    .name = "Confirm",
    .container_type = UI_TYPE_STACK_PANEL,
    .orientation = ORIENTATION_VERTICAL,
    .spacing = 10,
    .on_init = Page_Confirm_Init,
    .on_draw = NULL,
    .on_input = Page_Confirm_Input,
    .on_deinit = Page_Confirm_Deinit,
    .user_data = NULL
};

/* ========================================================================
 *  Хелпер для открытия диалога подтверждения
 * ======================================================================== */

void Page_ShowConfirm(const char* message, void (*on_confirm)(void), void (*on_cancel)(void))
{
    /* Создаём данные для диалога */
    ConfirmData_t* data = (ConfirmData_t*)heap_caps_malloc(sizeof(ConfirmData_t), 0);
    if (!data) return;
    
    data->message = message;
    data->on_confirm = on_confirm;
    data->on_cancel = on_cancel;
    
    /* Копируем определение (без user_data) */
    PageDef_t def = page_confirm_def;
    def.user_data = data;
    
    /* Открываем динамически */
    Page_OpenDynamic(&def, &digits_node);
}

/* ========================================================================
 *  Экспорт определений для использования в menu.c
 * ======================================================================== */

/* Возвращает указатель на определение страницы по имени */
PageDef_t* Page_FindByName(const char* name)
{
    if (!name) return NULL;
    
    if (strcmp(name, "Settings") == 0) return &page_settings_def;
    if (strcmp(name, "About") == 0) return &page_about_def;
    
    return NULL;
}

/* ========================================================================
 *  Публичные API для примеров
 * ======================================================================== */

/** Открыть страницу настроек CC1101 */
void Page_OpenSettings(void)
{
    Page_OpenStatic(&page_settings_def, &digits_node);
}

/** Открыть страницу About */
void Page_OpenAbout(void)
{
    Page_OpenStatic(&page_about_def, &digits_node);
}
