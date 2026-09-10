/**
 * @file page_dynamic_example.c
 * @brief Пример динамической страницы — "Frequency Analyzer"
 * 
 * Динамическая страница создаётся на лету с параметрами.
 * Показывает как использовать Page_OpenDynamic.
 */

#include "page.h"
#include "buttons.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ========================================================================
 *  Динамическая страница: Frequency Analyzer
 * ======================================================================== */

/* Данные страницы */
typedef struct {
    uint32_t freq_mhz;
    uint32_t bitrate;
    uint8_t  rxbw_index;
} FreqAnalyzerData_t;

/* Внутренние UI-элементы */
static UIElement_t* s_freq_label = NULL;
static UIElement_t* s_bitrate_label = NULL;
static UIElement_t* s_rxbw_label = NULL;

static bool FreqAnalyzer_Init(UIElement_t* page, UIElement_t* parent)
{
    FreqAnalyzerData_t* data = (FreqAnalyzerData_t*)page->user_data;
    
    /* Создаём StackPanel */
    UIElement_t* panel = Page_CreateStackPanel(page, ORIENTATION_VERTICAL, 12);
    if (!panel) return false;
    
    /* Заголовок */
    UIElement_t* title = Page_AddText(panel, "=== Freq Analyzer ===");
    if (title) {
        title->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
    }
    
    Page_AddText(panel, "");
    
    /* Частота */
    s_freq_label = Page_AddText(panel, "Freq: --- MHz");
    s_bitrate_label = Page_AddText(panel, "BitRate: --- kbps");
    s_rxbw_label = Page_AddText(panel, "RxBw: --- kHz");
    
    Page_AddText(panel, "");
    Page_AddText(panel, "[UP] +Freq");
    Page_AddText(panel, "[DOWN] -Freq");
    Page_AddText(panel, "[ENTER] +BitRate");
    Page_AddText(panel, "[CANCEL] Back");
    
    /* Обновляем значения из данных */
    if (data) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Freq: %lu.%02lu MHz", 
                 (unsigned long)data->freq_mhz / 100, 
                 (unsigned long)data->freq_mhz % 100);
        if (s_freq_label) {
            strncpy(s_freq_label->text_content, buf, sizeof(s_freq_label->text_content) - 1);
        }
        
        snprintf(buf, sizeof(buf), "BitRate: %lu.%02lu kbps",
                 (unsigned long)data->bitrate / 100,
                 (unsigned long)data->bitrate % 100);
        if (s_bitrate_label) {
            strncpy(s_bitrate_label->text_content, buf, sizeof(s_bitrate_label->text_content) - 1);
        }
        
        static const char* rxbw_str[] = {
            "58k", "68k", "81k", "102k", "116k", "135k", "162k", "203k",
            "232k", "270k", "325k", "406k", "464k", "541k", "650k", "812k"
        };
        if (s_rxbw_label && data->rxbw_index < 16) {
            snprintf(buf, sizeof(buf), "RxBw: %s", rxbw_str[data->rxbw_index]);
            strncpy(s_rxbw_label->text_content, buf, sizeof(s_rxbw_label->text_content) - 1);
        }
    }
    
    return true;
}

static void FreqAnalyzer_Draw(UIElement_t* page)
{
    if (page && page->sprite) {
        page->sprite->needs_render = true;
    }
}

static bool FreqAnalyzer_Input(UIElement_t* page, uint8_t key)
{
    FreqAnalyzerData_t* data = (FreqAnalyzerData_t*)page->user_data;
    char buf[64];
    
    switch (key) {
        case KEY_UP:
            if (data) {
                data->freq_mhz += 100; /* +1.00 MHz */
                snprintf(buf, sizeof(buf), "Freq: %lu.%02lu MHz", 
                         (unsigned long)data->freq_mhz / 100, 
                         (unsigned long)data->freq_mhz % 100);
                if (s_freq_label) {
                    strncpy(s_freq_label->text_content, buf, sizeof(s_freq_label->text_content) - 1);
                    s_freq_label->sprite->needs_render = true;
                }
            }
            return true;
            
        case KEY_DOWN:
            if (data) {
                if (data->freq_mhz >= 100) {
                    data->freq_mhz -= 100; /* -1.00 MHz */
                }
                snprintf(buf, sizeof(buf), "Freq: %lu.%02lu MHz", 
                         (unsigned long)data->freq_mhz / 100, 
                         (unsigned long)data->freq_mhz % 100);
                if (s_freq_label) {
                    strncpy(s_freq_label->text_content, buf, sizeof(s_freq_label->text_content) - 1);
                    s_freq_label->sprite->needs_render = true;
                }
            }
            return true;
            
        case KEY_ENTER:
            if (data) {
                data->bitrate += 100; /* +1.00 kbps */
                snprintf(buf, sizeof(buf), "BitRate: %lu.%02lu kbps",
                         (unsigned long)data->bitrate / 100,
                         (unsigned long)data->bitrate % 100);
                if (s_bitrate_label) {
                    strncpy(s_bitrate_label->text_content, buf, sizeof(s_bitrate_label->text_content) - 1);
                    s_bitrate_label->sprite->needs_render = true;
                }
            }
            return true;
            
        case KEY_CANCEL:
            return false; /* Обработается автоматически */
    }
    return false;
}

static void FreqAnalyzer_Deinit(UIElement_t* page)
{
    FreqAnalyzerData_t* data = (FreqAnalyzerData_t*)page->user_data;
    if (data) {
        heap_caps_free(data);
        page->user_data = NULL;
    }
    s_freq_label = NULL;
    s_bitrate_label = NULL;
    s_rxbw_label = NULL;
    printf("Page: FreqAnalyzer closed\n");
}

static PageDef_t page_freq_analyzer_def = {
    .name = "FreqAnalyzer",
    .container_type = UI_TYPE_STACK_PANEL,
    .orientation = ORIENTATION_VERTICAL,
    .spacing = 12,
    .on_init = FreqAnalyzer_Init,
    .on_draw = FreqAnalyzer_Draw,
    .on_input = FreqAnalyzer_Input,
    .on_deinit = FreqAnalyzer_Deinit,
    .user_data = NULL
};

/* ========================================================================
 *  Публичный API для создания динамической страницы
 * ======================================================================== */

/**
 * @brief Открыть динамическую страницу Frequency Analyzer
 * @param freq_mhz Начальная частота в сотых МГц (43396 = 433.96 MHz)
 * @param bitrate Начальный битрейт в сотых kbps (960 = 9.60 kbps)
 * @param rxbw_index Индекс полосы приёмника (0..15)
 * @return true если страница открыта
 */

bool Page_OpenFreqAnalyzer(uint32_t freq_mhz, uint32_t bitrate, uint8_t rxbw_index)
{
    /* Создаём данные страницы */
    FreqAnalyzerData_t* data = (FreqAnalyzerData_t*)heap_caps_malloc(sizeof(FreqAnalyzerData_t), 0);
    if (!data) return false;
    
    data->freq_mhz = freq_mhz;
    data->bitrate = bitrate;
    data->rxbw_index = rxbw_index;
    
    /* Копируем определение */
    PageDef_t def = page_freq_analyzer_def;
    def.user_data = data;
    extern UIElement_t main_work_grid;
    /* Открываем динамически */
    Page_OpenDynamic(&def, &digits_node);
    
    
    return true;
}
