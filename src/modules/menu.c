#include "menu.h"
#include "gui.h"
#include "st7796.h"
#include "lcd_backlight.h"
#include "buzzer.h"
#include "ds3231.h"
#include "i2c.h"
#include "esp32_ToWiFiandBluetooth.h"
#include "rssi_plotter_screen.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// === ВНЕШНИЕ ПЕРЕМЕННЫЕ (Ваш код) ===
extern Sprite_t main_screen_sprite;
extern uint16_t Display_Width;
extern uint16_t Display_Height;
// Предполагаем, что эти переменные объявлены где-то в main.c или globals.h
extern uint16_t sys, room, btn;
extern void transmit(uint16_t sys, uint16_t room, uint16_t btn, uint8_t type);

extern void initRfTransmitter(int is_pager);
extern void enterReceiverMode();
extern void showNC();
extern void showAbout();
extern void rs485ToggleBluetoothMode();



// Для настроек
extern uint8_t rs485BaudIndex;
extern uint8_t oledBrightness;
extern uint32_t cc1101FreqFixed;
extern uint16_t cc1101BitRateFixed;
extern int bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff, vibroOnOff;
extern bool rs485toBt;

static uint8_t clock_date = 1;
static uint8_t clock_month = 1;
static uint8_t clock_day = DS3231_MONDAY;
static uint8_t clock_hour = 0;
static uint8_t clock_minute = 0;
static uint8_t clock_second = 0;
static uint8_t clock_year = 0;

uint8_t lcd_backlight_level = 5; // Локальная копия для меню (0-10)
extern void toggleWiFi();
extern void manualSyncTimeWithNTP();
extern void clearWiFiCredentials();

// Для перерисовки статус-бара
extern void GUI_InvalidateStatusBar(void);

/* Мост к CC1101 hardware */
#include "radio_config_bridge.h"
void cc1101ApplySettingsFromMenu(void);

/* Forward declarations */
static void RssiPlotter_Action(void);

/* External references (из gui.c) */
extern uint8_t saved_menu_scroll_offset;
extern int16_t saved_menu_selected_index;
extern uint8_t panel_rows_count;

/* ========================================================================
 *  Collapse/Expand Menu — для экономии RAM в полноэкранных режимах
 * ======================================================================== */

/**
 * @brief Скрыть ListBox меню, освободить память детей
 * @return true если меню было скрыто
 */
bool Menu_Collapse(void) {
    if (!current_menu_listbox) return false;
    
    UIElement_t* lb = current_menu_listbox;
    
    /* Сохраняем состояние */
    saved_menu_scroll_offset = lb->props.list_box.scroll_offset;
    saved_menu_selected_index = lb->props.list_box.selected_index;
    
    /* Освобождаем память детей ListBox (пункты меню) */
    if (lb->children_count > 0) {
        /* Уменьшаем panel_rows_count, "возвращая" память пулу */
        panel_rows_count -= lb->children_count;
        lb->children_count = 0;
        
        /* Очищаем указатели на детей */
        memset(lb->children, 0, sizeof(lb->children));
    }
    
    /* Устанавливаем флаг collapsed */
    lb->props.list_box.collapsed = 1;
    
    /* Инвалидируем спрайт для перерисовки */
    extern Sprite_t main_screen_sprite;
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        main_screen_sprite.needs_render = true;
    }
    
    return true;
}

/**
 * @brief Восстановить ListBox меню и перерисовать
 */
void Menu_Expand(void) {
    if (!current_menu_listbox) return;
    
    UIElement_t* lb = current_menu_listbox;
    
    /* Сбрасываем флаг collapsed */
    lb->props.list_box.collapsed = 0;
    
    /* Перерисовываем ListBox (создаст children заново) */
    Menu_Draw(lb, current_menu_items, current_menu_count);
    
    /* Восстанавливаем scroll и selected с проверкой границ */
    uint8_t max_scroll = (current_menu_count > lb->props.list_box.visible_row_count &&
                          lb->props.list_box.visible_row_count > 0) ?
                         (current_menu_count - lb->props.list_box.visible_row_count) : 0;
    if (saved_menu_scroll_offset > max_scroll) {
        saved_menu_scroll_offset = max_scroll;
    }
    lb->props.list_box.scroll_offset = saved_menu_scroll_offset;
    
    if (saved_menu_selected_index >= 0 && (uint8_t)saved_menu_selected_index < current_menu_count) {
        lb->props.list_box.selected_index = saved_menu_selected_index;
        lb->props.list_box.last_leaf_selected = saved_menu_selected_index;
    } else {
        lb->props.list_box.selected_index = 0;
        lb->props.list_box.last_leaf_selected = 0;
    }
    
    /* Инвалидируем спрайт для перерисовки */
    extern Sprite_t main_screen_sprite;
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        main_screen_sprite.needs_render = true;
    }
}

/* Таблицы форматирования (локальные, не экспортируются) */
static const char* mod_str[] = { "ASK", "FSK", "2FSK", "GFSK", "OOK", "4FSK", "MSK" };
static const int8_t power_dbm[] = { -30, -20, -15, -10, -3, 0, 5, 10 };
static const char* rxbw_str[] = {
    "58k", "68k", "81k", "102k", "116k", "135k", "162k", "203k",
    "232k", "270k", "325k", "406k", "464k", "541k", "650k", "812k"
};

static const uint32_t rs485_baud_rates[] = {
    110U, 300U, 600U, 1200U, 2400U, 4800U, 9600U, 14400U,
    19200U, 38400U, 56000U, 57600U, 115200U, 128000U, 256000U
};

static void RS485_Baud_Update_Callback(void)
{
    Settings_t* settings = SettingsManager_GetMutable();
    if (!settings) return;

    if (rs485BaudIndex >= RS485_BAUD_COUNT) {
        rs485BaudIndex = RS485_BAUD_COUNT - 1U;
    }
    settings->rs485_baud_index = rs485BaudIndex;
    markSettingDirty();
    saveAllSettings();
}

/* Авто-применение при изменении параметра */
static void Cc1101_AutoApplyFreq(void)
{
    Settings_t* s = SettingsManager_GetMutable();
    if (!s) return;
    s->cc1101_freq_fixed = cc1101FreqFixed;
    markSettingDirty();
    saveAllSettings();
    Bridge_ApplyCC1101Param(BRIDGE_PARAM_FREQ);
}
static void Cc1101_AutoApplyBitrate(void)
{
    Settings_t* s = SettingsManager_GetMutable();
    if (!s) return;
    s->cc1101_bitrate_fixed = cc1101BitRateFixed;
    markSettingDirty();
    saveAllSettings();
    Bridge_ApplyCC1101Param(BRIDGE_PARAM_BITRATE);
}
static void Cc1101_AutoApplyRxBw(void)
{
    Settings_t* s = SettingsManager_GetMutable();
    if (!s) return;
    s->cc1101_rxbw_index = cc1101RxBwIndex;
    markSettingDirty();
    saveAllSettings();
    Bridge_ApplyCC1101Param(BRIDGE_PARAM_RXBW);
}
static void Cc1101_AutoApplyMod(void)
{
    /* Индекс 0..6 → реальное значение модуляции CC1101 */
    static const uint8_t mod_values[] = {
        CC1101_MOD_ASK, CC1101_MOD_FSK, CC1101_MOD_2FSK,
        CC1101_MOD_GFSK, CC1101_MOD_OOK, CC1101_MOD_4FSK, CC1101_MOD_MSK
    };
    
    Settings_t* s = SettingsManager_GetMutable();
    if (!s) return;
    
    if (cc1101Modulation < 7) {
        s->cc1101_modulation = mod_values[cc1101Modulation];
    }
    markSettingDirty();
    saveAllSettings();
    Bridge_ApplyCC1101Param(BRIDGE_PARAM_MODULATION);
}
static void Cc1101_AutoApplyPower(void)
{
    Settings_t* s = SettingsManager_GetMutable();
    if (!s) return;
    s->cc1101_power_index = cc1101PowerIndex;
    markSettingDirty();
    saveAllSettings();
    Bridge_ApplyCC1101Param(BRIDGE_PARAM_POWER);
}

// === ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МЕНЮ ===
UIElement_t* current_menu_listbox = NULL;
MenuItem_t* current_menu_items = NULL;
uint8_t current_menu_count = 0;

// === СТЕК НАВИГАЦИИ ===
MenuState_t menu_stack[MAX_MENU_DEPTH];
int menu_stack_top = -1;  // -1 = стек пуст (находимся в главном меню)

// === РЕЖИМ РЕДАКТИРОВАНИЯ ЗНАЧЕНИЙ (INLINE) ===
uint8_t  edit_mode_active = 0;
uint32_t edit_temp_value = 0;      // Временное значение для редактирования
uint8_t  edit_value_size = 0;      // 1=uint8_t, 2=uint16_t
uint32_t edit_original_value = 0;  // Оригинальное значение (для отмены)
uint8_t  edit_step = 1;            // Шаг изменения
uint8_t menu_hold_multiplier = 1;
MenuItem_t* edit_source_item = NULL; // Исходный пункт меню
int8_t   edit_selected_index = -1;  // Индекс редактируемой строки в ListBox
char     edit_original_text[64];   // Оригинальный текст строки

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Пример простой иконки 16x16 (синий квадрат для примера)
// Лучше заменить эти данные на реальные иконки
/* static const uint16_t icon_tx[] = {
    // ... ваши пиксели 16x16 ...
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

static const uint16_t icon_settings[] = {
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

static const uint16_t icon_nc[] = {
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

// Массив всех доступных иконок, чтобы обращаться по ID
static const Icon_t all_icons[] = {
    { .pixels = (uint16_t*)icon_tx, .width = 16, .height = 16 },
    { .pixels = (uint16_t*)icon_settings, .width = 16, .height = 16 },
    { .pixels = (uint16_t*)icon_nc, .width = 16, .height = 16 },
    // Добавьте другие иконки сюда
};

#define ICON_COUNT (sizeof(all_icons) / sizeof(all_icons[0])) */
////////////////////////////////////////////////////////////////////////////////////////////////////////
// === ОПРЕДЕЛЕНИЕ МЕНЮ (Перенос из вашего C++ кода) ===

// --- Вспомогательные колбэки ---
static void Action_Send_Key() { transmit(sys, room, btn, 0); /* DEVICE_TYPE_KEY */ }
static void Action_Send_Pager() { transmit(sys, room, btn, 1); /* DEVICE_TYPE_PAGER */ }
static void Action_GoBack() { /* Логика возврата к главному меню */ }

// Колбэк для обновления статус-бара при изменении настроек
static void StatusBar_Update_Callback() {
    GUI_InvalidateStatusBar();
}

static void Bluetooth_Update_Callback() {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->bluetooth_enabled = bluetoothEnabled;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

static void RS485ToBt_Update_Callback() {
    GUI_InvalidateStatusBar();
}

static void WiFi_Update_Callback() {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->wifi_enabled = wifiEnabled;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

static void Clock_LoadFromDS3231(void)
{
    DS3231_Time_t time;
    if (DS3231_GetTime(&hi2c1, &time) != DS3231_OK) return;
    clock_date = time.Date;
    clock_month = time.Month;
    clock_day = time.Day;
    clock_hour = time.Hour;
    clock_minute = time.Minute;
    clock_second = time.Second;
    clock_year = time.Year;
}

static void Clock_SaveToDS3231(void)
{
    DS3231_Time_t time = {
        .Second = clock_second, .Minute = clock_minute, .Hour = clock_hour,
        .AM_PM = 0, .Day = clock_day, .Date = clock_date,
        .Month = clock_month, .Year = clock_year
    };
    DS3231_SetTime(&hi2c1, &time);
}

static void Clock_ValueChanged_Callback(void)
{
    Clock_SaveToDS3231();
}

static void Clock_NTP_Update_Callback(void)
{
    if (wifiEnabled && ESP32_WiFi_IsConnected()) ESP32_NTP_Sync();
}

static void Clock_SyncNow_Callback(void)
{
    if (wifiEnabled && ESP32_WiFi_IsConnected()) ESP32_NTP_Sync();
}

static void NTP_Update_Callback() {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->ntp_sync_enabled = ntpSyncEnabled;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

static void Buzzer_Update_Callback() {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->buzzer_enabled = buzzerOnOff;
        markSettingDirty();
        saveAllSettings();
    }
    /* Если выключили buzzer — остановить текущий звук */
    if (!buzzerOnOff) {
        Buzzer_Stop();
    }
    GUI_InvalidateStatusBar();
}

static void Vibro_Update_Callback() {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->vibro_enabled = vibroOnOff;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

// Колбэк для обновления подсветки экрана
static void Backlight_Update_Callback() {
    LCD_Backlight_SetLevel(lcd_backlight_level);
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->oled_brightness = lcd_backlight_level;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

// --- Колбэки сохранения sys/room/btn в энергонезависимую память ---
static void Sys_ValueChanged(void) {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->sys = (uint8_t)sys;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

static void Room_ValueChanged(void) {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->room = (uint8_t)room;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

static void Btn_ValueChanged(void) {
    Settings_t* s = SettingsManager_GetMutable();
    if (s) {
        s->btn = (uint8_t)btn;
        markSettingDirty();
        saveAllSettings();
    }
    GUI_InvalidateStatusBar();
}

// --- Подменю передачи ---
static MenuItem_t buttonSubMenu[] = {
    { "Sys:", 0, ITEM_TYPE_VALUE, 0, Sys_ValueChanged, { .ptr_value = &sys }, SYS_MIN, SYS_MAX, 1, 0 },
    { "Room:", 0, ITEM_TYPE_VALUE, 0, Room_ValueChanged, { .ptr_value = &room }, ROOM_MIN, ROOM_MAX, 1, 0 },
    { "Btn:", 0, ITEM_TYPE_VALUE, 0, Btn_ValueChanged, { .ptr_value = &btn }, BTN_MIN, BTN_MAX, 1, 0 },
    { "Send", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = Action_Send_Key } },
};

static MenuItem_t pagerSubMenu[] = {
    { "Sys:", 0, ITEM_TYPE_VALUE, 0, Sys_ValueChanged, { .ptr_value = &sys }, SYS_MIN, SYS_MAX, 1, 0 },
    { "Room:", 0, ITEM_TYPE_VALUE, 0, Room_ValueChanged, { .ptr_value = &room }, ROOM_MIN, ROOM_MAX, 1, 0 },
    { "Btn:", 0, ITEM_TYPE_VALUE, 0, Btn_ValueChanged, { .ptr_value = &btn }, BTN_MIN, BTN_MAX, 1, 0 },
    { "Send", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = Action_Send_Pager } },
};

static MenuItem_t transmitterSubMenu[] = {
    { "Buttons", 0, ITEM_TYPE_SUBMENU, sizeof(buttonSubMenu)/sizeof(buttonSubMenu[0]), NULL, { .submenu_items = buttonSubMenu } },
    { "Pager", 0, ITEM_TYPE_SUBMENU, sizeof(pagerSubMenu)/sizeof(pagerSubMenu[0]), NULL, { .submenu_items = pagerSubMenu } },
};

// --- Главное меню ---
static MenuItem_t getCallMenu[] = {
    { "1.Transmitter", 0, ITEM_TYPE_SUBMENU, sizeof(transmitterSubMenu)/sizeof(transmitterSubMenu[0]), NULL, { .submenu_items = transmitterSubMenu } },
    { "2.Receiver", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = enterReceiverMode } },
};

// --- Подменю CC1101 ---
static MenuItem_t cc1101SubMenu[] = {
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyFreq, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, CC1101_FREQ_FINE_STEP, 4 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyBitrate, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, CC1101_BITRATE_FINE_STEP, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyRxBw, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyMod, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyPower, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } },
};

// --- Меню CC1101 ---
static MenuItem_t cc1101Menu[] = {
    { "1. GetCall", 0, ITEM_TYPE_SUBMENU, sizeof(getCallMenu)/sizeof(getCallMenu[0]), NULL, { .submenu_items = getCallMenu } },
    { "2. RSSI Plotter", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = RssiPlotter_Action } },
    { "3. Settings CC1101", 0, ITEM_TYPE_SUBMENU, sizeof(cc1101SubMenu)/sizeof(cc1101SubMenu[0]), NULL, { .submenu_items = cc1101SubMenu }, },
};

static MenuItem_t nrf24l01SubMenu[] = {
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, CC1101_FREQ_FINE_STEP, 4 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, CC1101_BITRATE_FINE_STEP, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } }
};

static MenuItem_t sx1262SubMenu[] = {
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, CC1101_FREQ_FINE_STEP, 4 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, CC1101_BITRATE_FINE_STEP, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } },
};


static MenuItem_t irda_SubMenu[] = {
    { "Freq", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, CC1101_FREQ_FINE_STEP, 4 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, CC1101_BITRATE_FINE_STEP, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } },
};

static MenuItem_t irda_Menu[] = {
    { "1. IR RX", 0, ITEM_TYPE_SUBMENU, sizeof(sx1262SubMenu)/sizeof(sx1262SubMenu[0]), NULL, { .submenu_items = sx1262SubMenu }, },
    { "2. R TX", 0, ITEM_TYPE_SUBMENU, sizeof(sx1262SubMenu)/sizeof(sx1262SubMenu[0]), NULL, { .submenu_items = sx1262SubMenu }, },
    { "3. IR Settings", 0, ITEM_TYPE_SUBMENU, sizeof(irda_SubMenu)/sizeof(irda_SubMenu[0]), NULL, { .submenu_items = irda_SubMenu }, },
};

static MenuItem_t clockSubMenu[] = {
    { " Date", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_date }, 1, 31, 1, 1 },
    { " Month", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_month }, 1, 12, 1, 1 },
    { " Day", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_day }, 1, 7, 1, 1 },
    { " Hour", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_hour }, 0, 23, 1, 1 },
    { " Minute", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_minute }, 0, 59, 1, 1 },
    { " Second", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_second }, 0, 59, 1, 1 },
    { " Year", 0, ITEM_TYPE_VALUE, 0, Clock_ValueChanged_Callback, { .ptr_value = &clock_year }, 0, 99, 1, 1 },
    { " NTP Auto Sync", 0, ITEM_TYPE_VALUE, 0, Clock_NTP_Update_Callback, { .ptr_value = &ntpSyncEnabled }, 0, 1, 1, 0 },
    { " Sync Now", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = Clock_SyncNow_Callback } },
};

// --- Подменю настроек ---
static MenuItem_t settingsSubMenu[] = {
    { " Clock", 0, ITEM_TYPE_SUBMENU, sizeof(clockSubMenu)/sizeof(clockSubMenu[0]), NULL, { .submenu_items = clockSubMenu } },
    { " RS485", 0, ITEM_TYPE_VALUE, 0, RS485_Baud_Update_Callback, { .ptr_value = &rs485BaudIndex }, 0, RS485_BAUD_MAX, 1, 1 },
    { " Bluetooth", 0, ITEM_TYPE_VALUE, 0, Bluetooth_Update_Callback, { .ptr_value = &bluetoothEnabled }, 0, 1, 1, 0 },
    { " RS485_To_Bt", 0, ITEM_TYPE_VALUE, 0, RS485ToBt_Update_Callback, { .ptr_value = &rs485toBt }, 0, 1, 1, 1 },
    { " WiFi", 0, ITEM_TYPE_VALUE, 0, WiFi_Update_Callback, { .ptr_value = &wifiEnabled }, 0, 1, 1, 0 },
    { " Buzzer", 0, ITEM_TYPE_VALUE, 0, Buzzer_Update_Callback, { .ptr_value = &buzzerOnOff }, 0, 1, 1, 0 },
    { " Vibro", 0, ITEM_TYPE_VALUE, 0, Vibro_Update_Callback, { .ptr_value = &vibroOnOff }, 0, 1, 1, 0 },
    { " LED Backlight", 0, ITEM_TYPE_VALUE, 0, Backlight_Update_Callback, { .ptr_value = &lcd_backlight_level }, 1, 10, 1, 1 },
    { " Forget WiFi", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = NULL } },
};

static MenuItem_t radioMenu[] = {
    { "1. CC1101", 0, ITEM_TYPE_SUBMENU, sizeof(cc1101Menu)/sizeof(cc1101Menu[0]), NULL, { .submenu_items = cc1101Menu }, },
    { "2. NRF24l01", 0, ITEM_TYPE_SUBMENU, sizeof(nrf24l01SubMenu)/sizeof(nrf24l01SubMenu[0]), NULL, { .submenu_items = nrf24l01SubMenu }, },
    { "3. SX1262", 0, ITEM_TYPE_SUBMENU, sizeof(sx1262SubMenu)/sizeof(sx1262SubMenu[0]), NULL, { .submenu_items = sx1262SubMenu }, },
};

/* Forward declaration for spectrum analyzer */
extern void Menu_SpectrumAnalyzer(void);

static MenuItem_t mainMenu[] = {
    { "1. Radio", 0, ITEM_TYPE_SUBMENU, sizeof(radioMenu)/sizeof(radioMenu[0]), NULL, { .submenu_items = radioMenu } },
    { "2. IR", 0, ITEM_TYPE_SUBMENU, sizeof(irda_Menu)/sizeof(irda_Menu[0]), NULL, { .submenu_items = irda_Menu } },
    { "3. Spectrum", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = Menu_SpectrumAnalyzer } },
    { "4. NC", 2, ITEM_TYPE_INFO },
    { "5. Settings", 1, ITEM_TYPE_SUBMENU, sizeof(settingsSubMenu)/sizeof(settingsSubMenu[0]), NULL, { .submenu_items = settingsSubMenu } },
    { "6. About", 0, ITEM_TYPE_INFO },
    
};

// Глобальная переменная для размера
uint8_t main_menu_count = sizeof(mainMenu) / sizeof(mainMenu[0]);

/* Флаг длинного нажатия (устанавливается в Menu_ProcessInput) */
uint8_t menu_long_press_active = 0;

/* Блокировка тача после перехода между меню (мс) */
uint32_t touch_lock_tick = 0;

// === РЕАЛИЗАЦИЯ ФУНКЦИЙ ===

void Menu_SetMainMenu(void) {
    // mainMenu определен в этом файле (пусть будет static или без static, доступ только внутри)
    // Но так как мы здесь, мы видим mainMenu
    current_menu_items = mainMenu; 
    current_menu_count = sizeof(mainMenu) / sizeof(mainMenu[0]);
}

void Menu_PushMenu(MenuItem_t* items, uint8_t count) {
    if (menu_stack_top >= MAX_MENU_DEPTH - 1) return; // Стек переполнен
    
    UIElement_t* lb = current_menu_listbox;
    if (!lb) return;
    
    // Сохраняем текущее состояние на стек
    menu_stack_top++;
    menu_stack[menu_stack_top].items = current_menu_items;
    menu_stack[menu_stack_top].count = current_menu_count;
    menu_stack[menu_stack_top].selected_index = lb->props.list_box.selected_index;
    menu_stack[menu_stack_top].scroll_offset = lb->props.list_box.scroll_offset;
    menu_stack[menu_stack_top].last_leaf_selected = lb->props.list_box.last_leaf_selected;
    
    // Переключаемся на новое подменю
    Menu_Draw(lb, items, count);
}

void Menu_PopMenu(UIElement_t* listbox) {
    if (menu_stack_top < 0) return; // Стек пуст — уже в главном меню
    
    // Восстанавливаем состояние с стека
    MenuState_t* prev_state = &menu_stack[menu_stack_top];
    menu_stack_top--;  // Очищаем элемент на стеке
    
    // Восстанавливаем selected_index с проверкой границ
    if (prev_state->selected_index >= prev_state->count) {
        prev_state->selected_index = 0;
    }
    
    // Полная перерисовка меню через Menu_Draw (корректно пересоздаст children)
    current_menu_items = prev_state->items;
    current_menu_count = prev_state->count;
    
    Menu_Draw(listbox, prev_state->items, prev_state->count);
    listbox->props.list_box.selected_index = prev_state->selected_index;
    listbox->props.list_box.scroll_offset = prev_state->scroll_offset;
    listbox->props.list_box.last_leaf_selected = prev_state->last_leaf_selected;
}

void Menu_Init(void) {
    current_menu_listbox = NULL;
    menu_stack_top = -1;  // Очищаем стек
    Menu_SetMainMenu(); // Инициализируем указатели на начало
    Clock_LoadFromDS3231();
    
    // Синхронизируем уровень подсветки с main.c
    lcd_backlight_level = LCD_Backlight_GetLevel();
}

void Menu_Draw(UIElement_t* listbox_container, MenuItem_t* items, uint8_t count) {
    if (!listbox_container || !items) return;

    // Сброс визуальных детей
    listbox_container->children_count = 0; 
    // Не вычитаем panel_rows_count — он уже обнулён в GUI_ShowMenuAdvancedMeasurementScreen
    
    // Сброс логики прокрутки (начальные значения — 0)
    listbox_container->props.list_box.scroll_offset = 0;
    listbox_container->props.list_box.selected_index = 0;
    listbox_container->props.list_box.last_leaf_selected = 0;

    // Заполнение новыми данными
    for (uint8_t i = 0; i < count; i++) {
        UI_ListBox_AddItem(listbox_container, items[i].text); 
    }

    // ОБНОВЛЕНИЕ ГЛОБАЛЬНОГО КОНТЕКСТА КРИТИЧЕСКИ ВАЖНО:
    current_menu_listbox = listbox_container; 
    current_menu_items = items;
    current_menu_count = count;

    // Обновляем текст всех элементов ListBox с текущими значениями параметров
    printf("MDraw: count=%d children_count=%d\n", count, listbox_container->children_count);
    for (uint8_t i = 0; i < count; i++) {
        printf("  [%d] type=%d children[i]=%p\n", i, items[i].type, 
               (i < listbox_container->children_count) ? (void*)listbox_container->children[i] : (void*)0);
        if (items[i].type == ITEM_TYPE_VALUE && 
            (uint8_t)i < listbox_container->children_count) {
            Update_MenuItem_Text(listbox_container->children[i], &items[i]);
        }
    }

    // Инвалидируем спрайт — следующий UI_DrawTree вызовет UI_RenderListBox для всего ListBox
    if (listbox_container->sprite) {
        Sprite_t* s = listbox_container->sprite;
        s->needs_render = true;
        s->dirty_x1 = 0; s->dirty_y1 = 0;
        s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    }
}

// Хелпер для изменения uint16_t
static void AdjustValue16(void* ptr, int min, int max, int step, int direction) {
    uint16_t val = *(uint16_t*)ptr;
    val += direction * step;
    if (val < (uint16_t)min) val = (uint16_t)max;
    if (val > (uint16_t)max) val = (uint16_t)min;
    *(uint16_t*)ptr = val;
}

// Хелпер для изменения uint8_t
static void AdjustValue8(void* ptr, int min, int max, int step, int direction) {
    uint8_t val = *(uint8_t*)ptr;
    val += direction * step;
    if (val < (uint8_t)min) val = (uint8_t)max;
    if (val > (uint8_t)max) val = (uint8_t)min;
    *(uint8_t*)ptr = val;
}

uint32_t Menu_NormalizeFrequency(uint32_t value, int8_t direction) {
    if (value < CC1101_FREQ_BAND1_MIN_FIXED) return CC1101_FREQ_BAND1_MIN_FIXED;
    if (value <= CC1101_FREQ_BAND1_MAX_FIXED) return value;
    if (value < CC1101_FREQ_BAND2_MIN_FIXED) {
        return direction > 0 ? CC1101_FREQ_BAND2_MIN_FIXED : CC1101_FREQ_BAND1_MAX_FIXED;
    }
    if (value <= CC1101_FREQ_BAND2_MAX_FIXED) return value;
    if (value < CC1101_FREQ_BAND3_MIN_FIXED) {
        return direction > 0 ? CC1101_FREQ_BAND3_MIN_FIXED : CC1101_FREQ_BAND2_MAX_FIXED;
    }
    if (value <= CC1101_FREQ_BAND3_MAX_FIXED) return value;
    return CC1101_FREQ_BAND3_MAX_FIXED;
}

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ РЕЖИМА РЕДАКТИРОВАНИЯ ===

/** Общая логика форматирования значения по метке */
static const char* Format_ValueText(const char* label, char* buf, size_t buf_size, uint32_t val) {
    if (strcmp(label, "Freq MHz") == 0 || strcmp(label, "Freq") == 0) {
        snprintf(buf, buf_size, "%s: %u.%02u", label,
                 (unsigned)(val / 100U), (unsigned)(val % 100U));
    } else if (strcmp(label, "BitRate") == 0) {
        snprintf(buf, buf_size, "%s: %u.%02u", label,
                 (unsigned)(val / 100U), (unsigned)(val % 100U));
    } else if (strcmp(label, " RS485") == 0) {
        if (val < RS485_BAUD_COUNT) {
            snprintf(buf, buf_size, "%s: %lu", label,
                     (unsigned long)rs485_baud_rates[val]);
        } else {
            snprintf(buf, buf_size, "%s: invalid", label);
        }
    } else if (strcmp(label, "Mod") == 0) {
        if (val < 7) {
            snprintf(buf, buf_size, "%s: %s", label, mod_str[val]);
        } else {
            snprintf(buf, buf_size, "%s: %lu", label, (unsigned long)val);
        }
    } else if (strcmp(label, "Power") == 0) {
        if (val < 8) {
            snprintf(buf, buf_size, "%s: %d dBm", label, power_dbm[val]);
        } else {
            snprintf(buf, buf_size, "%s: %lu", label, (unsigned long)val);
        }
    } else if (strcmp(label, "RxBw") == 0) {
        if (val < 16) {
            snprintf(buf, buf_size, "%s: %s", label, rxbw_str[val]);
        } else {
            snprintf(buf, buf_size, "%s: %lu", label, (unsigned long)val);
        }
    } else if (strcmp(label, " Bluetooth") == 0 || strcmp(label, " RS485_To_Bt") == 0 ||
        strcmp(label, " WiFi") == 0 || strcmp(label, " NTP Auto Sync") == 0 ||
        strcmp(label, " Buzzer") == 0 || strcmp(label, " Vibro") == 0) {
        snprintf(buf, buf_size, "%s: %s", label, (val ? "On" : "Off"));
    } else if (strcmp(label, "LED Backlight") == 0) {
        if (val == 0) {
            snprintf(buf, buf_size, "%s: Off", label);
        } else {
            snprintf(buf, buf_size, "%s: Lvl %lu", label, (unsigned long)val);
        }
    } else {
        snprintf(buf, buf_size, "%s: %lu", label, (unsigned long)val);
    }
    return buf;
}

/** Форматирует значение в строку с учётом типа параметра (для режима редактирования) */
static void Format_EditValue(char* buf, size_t buf_size, const char* label, uint32_t val) {
    Format_ValueText(label, buf, buf_size, val);
}

/** Обновляет текст строки редактирования (inline) */
void Menu_Update_EditDisplay(void) {
    if (edit_selected_index < 0 || !current_menu_listbox) return;
    if ((uint8_t)edit_selected_index >= current_menu_listbox->children_count) return;
    
    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[edit_selected_index];
    if (!ui_item || !edit_source_item) return;
    
    // Форматируем значение с подсветкой (скобки)
    char formatted[64];
    Format_EditValue(formatted, sizeof(formatted), edit_source_item->text, edit_temp_value);
    strncpy(ui_item->text_content, formatted, sizeof(ui_item->text_content) - 1);
    ui_item->text_content[sizeof(ui_item->text_content) - 1] = '\0';
    
    // Перерисовываем строку
    UI_RenderListBoxItem(current_menu_listbox, (uint8_t)edit_selected_index);
}

void Menu_EditMode_PreviewValue(void) {
    if (!edit_mode_active || !edit_source_item) return;
    if (strcmp(edit_source_item->text, " LED Backlight") == 0) {
        LCD_Backlight_SetLevel((uint8_t)edit_temp_value);
    }
}

/** Inline: войти в режим редактирования значения */
void Menu_EditMode_Enter(MenuItem_t* item, int selected_index) {
    if (!current_menu_listbox || selected_index < 0) return;
    if ((uint8_t)selected_index >= current_menu_listbox->children_count) return;
    
    // Сохраняем состояние
    edit_source_item = item;
    edit_selected_index = selected_index;
    edit_value_size = item->value_size;
    if (edit_value_size == 0) edit_value_size = 1;
    
    // Читаем оригинальное значение
    if (edit_value_size == 1) {
        uint8_t v;
        memcpy(&v, item->data.ptr_value, 1);
        edit_original_value = v;
    } else if (edit_value_size == 4) {
        memcpy(&edit_original_value, item->data.ptr_value, 4);
    } else {
        memcpy(&edit_original_value, item->data.ptr_value, 2);
    }
    
    edit_temp_value = edit_original_value;
    edit_step = item->value_limits.step;
    if (edit_step == 0) edit_step = 1;
    
    // Сохраняем оригинальный текст строки
    UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[selected_index];
    if (ui_item) {
        strncpy(edit_original_text, ui_item->text_content, sizeof(edit_original_text) - 1);
        edit_original_text[sizeof(edit_original_text) - 1] = '\0';
    }
    
    edit_mode_active = 1;
    
    // Обновляем текст строки на редактируемое значение
    Menu_Update_EditDisplay();
    // Нижняя панель уже существует — рисовать не нужно
}

/** Inline: выйти из режима редактирования */
void Menu_EditMode_Exit(void) {
    if (!edit_mode_active) return;
    edit_mode_active = 0;
    
    // Показываем фактическое значение после сохранения или отмены
    if (edit_selected_index >= 0 && current_menu_listbox) {
        if ((uint8_t)edit_selected_index < current_menu_listbox->children_count) {
            UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[edit_selected_index];
            if (ui_item) {
                Update_MenuItem_Text(ui_item, edit_source_item);
                UI_RenderListBoxItem(current_menu_listbox, (uint8_t)edit_selected_index);
            }
        }
    }
    edit_selected_index = -1;
    edit_source_item = NULL;
    // Нижняя панель постоянная — очищать не нужно
}

/**
 * @brief Выполняет действие для выбранного пункта меню.
 * Вызывается как из Menu_ProcessInput (по кнопкам), так и из Menu_ProcessTouch.
 */
void Menu_ExecuteSelected(UIElement_t* listbox, uint8_t selected_index) 
{
    if (!listbox || !current_menu_items) return;
    if (selected_index >= current_menu_count) return;

    MenuItem_t* item = &current_menu_items[selected_index];
    UIElement_t* ui_item = NULL;
    
    // Безопасное получение визуального элемента строки
    if ((uint8_t)selected_index < listbox->children_count) {
        ui_item = (UIElement_t*)listbox->children[selected_index];
    }

    switch (item->type) {
        case ITEM_TYPE_ACTION:
            if (item->data.action_func) {
                item->data.action_func();
            }
            break;

        case ITEM_TYPE_VALUE: {
            /* Булевы параметры (0..1) → сразу переключаем по OK */
            if (item->value_limits.min_val == 0 && item->value_limits.max_val == 1) {
                
                uint16_t old_val16;
                uint8_t  old_val8;
                
                if (item->value_size == 1) {
                    memcpy(&old_val8, item->data.ptr_value, 1);
                } else {
                    memcpy(&old_val16, item->data.ptr_value, 2);
                }
                
                uint8_t step = item->value_limits.step;
                if (menu_long_press_active) {
                    if (strcmp(item->text, "Freq MHz") == 0 || strcmp(item->text, "BitRate") == 0) {
                        step = 100;
                    } else {
                        step = step * 10;
                    }
                }
                
                if (item->value_size == 1) {
                    AdjustValue8(item->data.ptr_value, item->value_limits.min_val, item->value_limits.max_val, step, 1);
                    uint8_t new_val;
                    memcpy(&new_val, item->data.ptr_value, 1);
                    if (new_val != old_val8 && ui_item) {
                        Update_MenuItem_Text(ui_item, item);
                        UI_RenderListBoxItem(listbox, selected_index);
                        if (item->on_value_changed) item->on_value_changed();
                    }
                } else {
                    AdjustValue16(item->data.ptr_value, item->value_limits.min_val, item->value_limits.max_val, step, 1);
                    uint16_t new_val;
                    memcpy(&new_val, item->data.ptr_value, 2);
                    if (new_val != old_val16 && ui_item) {
                        Update_MenuItem_Text(ui_item, item);
                        UI_RenderListBoxItem(listbox, selected_index);
                        if (item->on_value_changed) item->on_value_changed();
                    }
                }
            } else {
                /* Не-булево → входим в inline-режим редактирования */
                Menu_EditMode_Enter(item, selected_index);
            }
            break;
        }

        case ITEM_TYPE_SUBMENU:
            if (item->data.submenu_items && item->data_count > 0) {
                listbox->touch_state.drag_last_y = -1;
                listbox->touch_state.drag_active = false;
                
                // Сохраняем текущее меню на стек и переходим в подменю
                Menu_PushMenu(item->data.submenu_items, item->data_count);
                // Блокируем тач на 300мс после перехода
                touch_lock_tick = HAL_GetTick();
            }
            break;

        case ITEM_TYPE_INFO:
            // Можно вызвать showNC() здесь, если нужно поведение как у кнопки
            break;
    }
}

void Menu_ProcessInput(uint8_t key) {
    if (!current_menu_listbox || !current_menu_items) return;

    /* ===== РЕЖИМ РЕДАКТИРОВАНИЯ ЗНАЧЕНИЯ ===== */
    if (edit_mode_active && edit_source_item) {
        uint16_t adjustment_step = edit_step;
        if (menu_long_press_active &&
            (strcmp(edit_source_item->text, "Freq MHz") == 0 ||
             strcmp(edit_source_item->text, "Freq") == 0 ||
             strcmp(edit_source_item->text, "BitRate") == 0)) {
            adjustment_step = (strcmp(edit_source_item->text, "Freq MHz") == 0 ||
                               strcmp(edit_source_item->text, "Freq") == 0) ?
                              CC1101_FREQ_COARSE_STEP : CC1101_BITRATE_COARSE_STEP;
            if (menu_hold_multiplier > 1) {
                adjustment_step = (strcmp(edit_source_item->text, "Freq MHz") == 0 ||
                                   strcmp(edit_source_item->text, "Freq") == 0) ?
                                  CC1101_FREQ_ACCEL_STEP : CC1101_BITRATE_ACCEL_STEP;
            }
        }
        switch (key) {
            case KEY_UP: {
                /* Увеличиваем значение */
                if (edit_value_size == 1) {
                    uint8_t v = (uint8_t)edit_temp_value;
                    v += (uint8_t)adjustment_step;
                    if (v > (uint8_t)edit_source_item->value_limits.max_val) {
                        v = (uint8_t)edit_source_item->value_limits.min_val;
                    }
                    edit_temp_value = v;
                } else {
                    edit_temp_value += adjustment_step;
                    if (edit_temp_value > (uint32_t)edit_source_item->value_limits.max_val) {
                        edit_temp_value = (uint32_t)edit_source_item->value_limits.min_val;
                    }
                }
                if (edit_value_size == 4) {
                    edit_temp_value = Menu_NormalizeFrequency(edit_temp_value, 1);
                }
                Menu_EditMode_PreviewValue();
                Menu_Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
                break;
            }
            case KEY_DOWN: {
                /* Уменьшаем значение (через signed для корректной проверки min) */
                if (edit_value_size == 1) {
                    int8_t v = (int8_t)edit_temp_value;
                    v -= (int16_t)adjustment_step;
                    if (v < edit_source_item->value_limits.min_val) {
                        v = (int8_t)edit_source_item->value_limits.max_val;
                    }
                    edit_temp_value = (uint16_t)v;
                } else {
                    int32_t v = (int32_t)edit_temp_value - adjustment_step;
                    if (v < edit_source_item->value_limits.min_val) {
                        v = edit_source_item->value_limits.max_val;
                    }
                    edit_temp_value = (uint32_t)v;
                }
                if (edit_value_size == 4) {
                    edit_temp_value = Menu_NormalizeFrequency(edit_temp_value, -1);
                }
                Menu_EditMode_PreviewValue();
                Menu_Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
                break;
            }
            case KEY_ENTER: {
                /* Сохраняем значение */
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_temp_value;
                } else if (edit_value_size == 4) {
                    *(uint32_t*)edit_source_item->data.ptr_value = edit_temp_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_temp_value;
                }
                /* Вызываем колбэк */
                if (edit_source_item->on_value_changed) {
                    edit_source_item->on_value_changed();
                }
                Buzzer_PlayTone(1000, 50);
                Menu_EditMode_Exit();
                break;
            }
            case KEY_CANCEL: {
                /* Отменяем — восстанавливаем оригинальное значение */
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_original_value;
                } else if (edit_value_size == 4) {
                    *(uint32_t*)edit_source_item->data.ptr_value = edit_original_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_original_value;
                }
                edit_temp_value = edit_original_value;
                Menu_EditMode_PreviewValue();
                Buzzer_PlayTone(400, 50);
                Menu_EditMode_Exit();
                break;
            }
        }
        return;
    }
    /* ===== КОНЕЦ РЕЖИМА РЕДАКТИРОВАНИЯ ===== */

    UIElement_t* lb = current_menu_listbox;
    uint8_t idx = lb->props.list_box.selected_index;

    if (idx >= current_menu_count) return;

    /* Счётчик длинного нажатия для KEY_ENTER */
    static uint16_t enter_hold_counter = 0;
    
    switch (key) {
        case KEY_UP:
            enter_hold_counter = 0;
            menu_long_press_active = 0;
            if (idx > 0) { 
                lb->props.list_box.selected_index--; 
                // Синхронизируем выделение с навигацией
                lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
                // Сохраняем scroll_offset при навигации
                uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = lb->font->char_height + pad;
                uint8_t visible_items = lb->h / item_h;
                if (visible_items == 0) visible_items = 1;
                if (lb->props.list_box.selected_index < (int16_t)lb->props.list_box.scroll_offset) {
                    lb->props.list_box.scroll_offset = (uint8_t)lb->props.list_box.selected_index;
                }
                UI_RenderListBoxItem(lb, idx);
                UI_RenderListBoxItem(lb, lb->props.list_box.selected_index);
            }
            break;
        case KEY_DOWN:
            enter_hold_counter = 0;
            menu_long_press_active = 0;
            if (idx < current_menu_count - 1) { 
                lb->props.list_box.selected_index++; 
                // Синхронизируем выделение с навигацией
                lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
                // Сохраняем scroll_offset при навигации
                uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = lb->font->char_height + pad;
                uint8_t visible_items = lb->h / item_h;
                if (visible_items == 0) visible_items = 1;
                if (lb->props.list_box.selected_index >= (int16_t)(lb->props.list_box.scroll_offset + visible_items)) {
                    lb->props.list_box.scroll_offset = (uint8_t)(lb->props.list_box.selected_index - visible_items + 1);
                }
                UI_RenderListBoxItem(lb, idx);
                UI_RenderListBoxItem(lb, lb->props.list_box.selected_index);
            }
            break;
            
        case KEY_ENTER:
            enter_hold_counter++;
            /* Длинное нажатие = 5+ циклов удержания */
            if (enter_hold_counter >= 5) {
                menu_long_press_active = 1;
            }
            Menu_ExecuteSelected(lb, idx);
            break;

        case KEY_CANCEL:
            // Выход на уровень выше (или в главное меню, если стек пуст)
            Menu_PopMenu(lb);
            break;
    }
}

/**
 * @brief Обновляет текстовое представление пункта меню в ListBox
 *        Так как UI_ListBox_AddItem копирует строку в ui_item->text_content,
 *        нам нужно переформатировать её, если это тип ITEM_TYPE_VALUE.
 */
void Update_MenuItem_Text(UIElement_t* ui_item, MenuItem_t* menu_item) {
    if (!ui_item || !menu_item || menu_item->type != ITEM_TYPE_VALUE) return;

    const char* label = menu_item->text;
    
    // Безопасное чтение значения
    uint16_t val16;
    uint32_t val32;
    uint8_t  val8;
    
    if (menu_item->value_size == 1) {
        memcpy(&val8, menu_item->data.ptr_value, 1);
        val32 = val8;
    } else if (menu_item->value_size == 4) {
        memcpy(&val32, menu_item->data.ptr_value, 4);
    } else {
        memcpy(&val16, menu_item->data.ptr_value, 2);
        val32 = val16;
    }
    
    // Используем общую функцию форматирования
    char formatted[64];
    Format_ValueText(label, formatted, sizeof(formatted), val32);
    
    strncpy(ui_item->text_content, formatted, sizeof(ui_item->text_content) - 1);
    ui_item->text_content[sizeof(ui_item->text_content) - 1] = '\0';
}

void transmit(uint16_t sys, uint16_t room, uint16_t btn, uint8_t type){}
void initRfTransmitter(int is_pager){}
void enterReceiverMode(){}
void showNC(){}
void showAbout(){}
void rs485ToggleBluetoothMode(){;}

// Для настроек
//extern uint8_t rs485BaudIndex;
//extern uint8_t oledBrightness;
//int bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff;
void toggleWiFi() {
    wifiEnabled = !wifiEnabled;
    if (wifiEnabled) {
        ESP32_WiFi_Connect();
    }
    GUI_InvalidateStatusBar();
}
void manualSyncTimeWithNTP(){}
extern void clearWiFiCredentials(void);
void cc1101ApplySettingsFromMenu(void)
{
    /* Обновляем настройки из переменных меню */
    Settings_t* settings = SettingsManager_GetMutable();
    if (!settings) return;

    /* Индекс 0..6 → реальное значение модуляции CC1101 */
    static const uint8_t mod_values[] = {
        CC1101_MOD_ASK, CC1101_MOD_FSK, CC1101_MOD_2FSK,
        CC1101_MOD_GFSK, CC1101_MOD_OOK, CC1101_MOD_4FSK, CC1101_MOD_MSK
    };
    
    settings->cc1101_freq_fixed   = cc1101FreqFixed;
    settings->cc1101_bitrate_fixed = cc1101BitRateFixed;
    settings->cc1101_rxbw_index   = cc1101RxBwIndex;
    settings->cc1101_modulation   = cc1101Modulation;
    settings->cc1101_power_index  = cc1101PowerIndex;
    
    if (cc1101Modulation < 7) {
        settings->cc1101_modulation = mod_values[cc1101Modulation];
    }

    /* Сохраняем в Flash */
    markSettingDirty();
    saveAllSettings();

    /* Применяем к CC1101 hardware */
    Bridge_ApplyCC1101Settings();

    /* Перерисовываем статус-бар */
    GUI_InvalidateStatusBar();
}

/* ========================================================================
 *  RSSI Plotter Menu Entry
 * ======================================================================== */

/* Колбэк для пункта меню "RSSI Plotter" */
static void RssiPlotter_Action(void) {
    /* Запускаем экран RSSI Plotter */
    RssiPlotterScreen_Enter();
}

/* ========================================================================
 *  Spectrum Analyzer Menu Entry
 * ======================================================================== */

void Menu_SpectrumAnalyzer(void) {
    /* Запускаем спектроанализатор */
    /* В полной реализации здесь будет:
     * 1. Инициализация RfSpectrum_t
     * 2. Запуск сканирования
     * 3. Рендеринг на дисплей
     * 4. Ожидание выхода (кнопка Cancel)
     */
    
    printf("Menu_SpectrumAnalyzer: START\n");
    
    /* TODO: Реализовать полный спектроанализатор */
    /*
    RfSpectrum_t spectrum;
    RfSpectrum_Init(&spectrum, 24000, 96000, 320);
    
    while (!buttonPressed(KEY_CANCEL)) {
        RfSpectrum_Scan(&spectrum);
        RfSpectrum_FindPeak(&spectrum);
        RfSpectrum_Render(&spectrum, 10, 40, 300, 200);
        HAL_Delay(100);
    }
    
    RfSpectrum_Stop(&spectrum);
    */
    
    printf("Menu_SpectrumAnalyzer: END\n");
}