#include "menu.h"
#include "gui.h"
#include "st7796.h"
#include "lcd_backlight.h"
#include "buzzer.h"
#include <string.h>
#include <stdio.h>

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
extern int bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff, vibroOnOff;

uint8_t lcd_backlight_level = 5; // Локальная копия для меню (0-10)
extern void toggleWiFi();
extern void manualSyncTimeWithNTP();
extern void clearWiFiCredentials();

// Для перерисовки статус-бара
extern void GUI_InvalidateStatusBar(void);

/* Мост к CC1101 hardware */
#include "radio_config_bridge.h"
void cc1101ApplySettingsFromMenu(void);

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

// External reference to panel_rows counter from gui.c
extern uint8_t panel_rows_count;

// Сохранённое состояние меню (из gui.c)
extern uint8_t saved_menu_scroll_offset;
extern int16_t saved_menu_selected_index;

// === СТЕК НАВИГАЦИИ ===
MenuState_t menu_stack[MAX_MENU_DEPTH];
int menu_stack_top = -1;  // -1 = стек пуст (находимся в главном меню)

// === РЕЖИМ РЕДАКТИРОВАНИЯ ЗНАЧЕНИЙ (INLINE) ===
static uint8_t  edit_mode_active = 0;
static uint16_t edit_temp_value = 0;      // Временное значение для редактирования
static uint8_t  edit_value_size = 0;      // 1=uint8_t, 2=uint16_t
static uint16_t edit_original_value = 0;  // Оригинальное значение (для отмены)
static uint8_t  edit_step = 1;            // Шаг изменения
static MenuItem_t* edit_source_item = NULL; // Исходный пункт меню
static int8_t   edit_selected_index = -1;  // Индекс редактируемой строки в ListBox
static char     edit_original_text[64];   // Оригинальный текст строки

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

// --- Подменю передачи ---
static MenuItem_t buttonSubMenu[] = {
    { "Sys:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &sys }, 1, 32, 1, 0 },
    { "Room:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &room }, 1, 32, 1, 0 },
    { "Btn:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &btn }, 1, 9, 1, 0 },
    { "Send", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = Action_Send_Key } },
};

static MenuItem_t pagerSubMenu[] = {
    { "Sys:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &sys }, 1, 32, 1, 0 },
    { "Room:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &room }, 1, 32, 1, 0 },
    { "Btn:", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &btn }, 1, 9, 1, 0 },
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
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyFreq, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, 10, 2 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyBitrate, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, 10, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyRxBw, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyMod, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, Cc1101_AutoApplyPower, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } },
};

// --- Меню CC1101 ---
static MenuItem_t cc1101Menu[] = {
    { "1. GetCall", 0, ITEM_TYPE_SUBMENU, sizeof(getCallMenu)/sizeof(getCallMenu[0]), NULL, { .submenu_items = getCallMenu } },
    { "2. RSSI Plotter", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = NULL } },
    { "3. Settings CC1101", 0, ITEM_TYPE_SUBMENU, sizeof(cc1101SubMenu)/sizeof(cc1101SubMenu[0]), NULL, { .submenu_items = cc1101SubMenu }, },
};

static MenuItem_t nrf24l01SubMenu[] = {
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, 10, 2 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, 10, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } }
};

static MenuItem_t sx1262SubMenu[] = {
    { "Freq MHz", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, 10, 2 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, 10, 2 },
    { "RxBw", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101RxBwIndex }, 0, 15, 1, 1 },
    { "Mod", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101Modulation }, 0, 6, 1, 1 },
    { "Power", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101PowerIndex }, 0, 7, 1, 1 },
    { "Apply", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = cc1101ApplySettingsFromMenu } },
};


static MenuItem_t irda_SubMenu[] = {
    { "Freq", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101FreqFixed }, 30000, 92800, 10, 2 },
    { "BitRate", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &cc1101BitRateFixed }, 120, 60000, 10, 2 },
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

// --- Подменю настроек ---
static MenuItem_t settingsSubMenu[] = {
    { " RS485", 0, ITEM_TYPE_VALUE, 0, NULL, { .ptr_value = &rs485BaudIndex }, 0, 7, 1, 0 },
    { " Bluetooth", 0, ITEM_TYPE_VALUE, 0, Bluetooth_Update_Callback, { .ptr_value = &bluetoothEnabled }, 0, 1, 1, 0 },
    { " RS485_To_Bt", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = rs485ToggleBluetoothMode } },
    { " WiFi", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = toggleWiFi } },
    { " NTP Auto Sync", 0, ITEM_TYPE_VALUE, 0, NTP_Update_Callback, { .ptr_value = &ntpSyncEnabled }, 0, 1, 1, 0 },
    { " Sync Now", 0, ITEM_TYPE_ACTION, 0, NULL, { .action_func = manualSyncTimeWithNTP } },
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

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ РЕЖИМА РЕДАКТИРОВАНИЯ ===

/** Форматирует значение в строку с учётом типа параметра */
static void Format_EditValue(char* buf, size_t buf_size, const char* label, uint16_t val) {
    if (strcmp(label, "Freq MHz") == 0) {
        snprintf(buf, buf_size, "%s: %.2f MHz", label, val / 100.0f);
    } else if (strcmp(label, "BitRate") == 0) {
        snprintf(buf, buf_size, "%s: %.2f kbps", label, val / 100.0f);
    } else if (strcmp(label, "Mod") == 0) {
        static const char* mod_str[] = { "ASK", "FSK", "2FSK", "GFSK", "OOK", "4FSK", "MSK" };
        if (val < 7) {
            snprintf(buf, buf_size, "%s: %s", label, mod_str[val]);
        } else {
            snprintf(buf, buf_size, "%s: %d", label, val);
        }
    } else if (strcmp(label, "Power") == 0) {
        static const int8_t power_dbm[] = { -30, -20, -15, -10, -3, 0, 5, 10 };
        if (val < 8) {
            snprintf(buf, buf_size, "%s: %d dBm", label, power_dbm[val]);
        } else {
            snprintf(buf, buf_size, "%s: %d", label, val);
        }
    } else if (strcmp(label, "RxBw") == 0) {
        static const char* rxbw_str[] = {
            "58k", "68k", "81k", "102k", "116k", "135k", "162k", "203k",
            "232k", "270k", "325k", "406k", "464k", "541k", "650k", "812k"
        };
        if (val < 16) {
            snprintf(buf, buf_size, "%s: %s", label, rxbw_str[val]);
        } else {
            snprintf(buf, buf_size, "%s: %d", label, val);
        }
    } else {
        snprintf(buf, buf_size, "%s: %d", label, val);
    }
}

/** Обновляет текст строки редактирования (inline) */
static void Update_EditDisplay(void) {
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

/** Inline: войти в режим редактирования значения */
static void EditMode_Enter(MenuItem_t* item, int selected_index) {
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
    Update_EditDisplay();
    // Нижняя панель уже существует — рисовать не нужно
}

/** Inline: выйти из режима редактирования */
static void EditMode_Exit(void) {
    if (!edit_mode_active) return;
    edit_mode_active = 0;
    
    // Восстанавливаем оригинальный текст строки
    if (edit_selected_index >= 0 && current_menu_listbox) {
        if ((uint8_t)edit_selected_index < current_menu_listbox->children_count) {
            UIElement_t* ui_item = (UIElement_t*)current_menu_listbox->children[edit_selected_index];
            if (ui_item) {
                strncpy(ui_item->text_content, edit_original_text, sizeof(ui_item->text_content) - 1);
                ui_item->text_content[sizeof(ui_item->text_content) - 1] = '\0';
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
static void Menu_ExecuteSelected(UIElement_t* listbox, uint8_t selected_index) 
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
                EditMode_Enter(item, selected_index);
            }
            break;
        }

        case ITEM_TYPE_SUBMENU:
            if (item->data.submenu_items && item->data_count > 0) {
                listbox->touch_state.drag_last_y = -1;
                listbox->touch_state.drag_active = false;
                
                // Сохраняем текущее меню на стек и переходим в подменю
                Menu_PushMenu(item->data.submenu_items, item->data_count);
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
        switch (key) {
            case KEY_UP: {
                /* Увеличиваем значение */
                if (edit_value_size == 1) {
                    uint8_t v = (uint8_t)edit_temp_value;
                    v += edit_step;
                    if (v > (uint8_t)edit_source_item->value_limits.max_val) {
                        v = (uint8_t)edit_source_item->value_limits.min_val;
                    }
                    edit_temp_value = v;
                } else {
                    edit_temp_value += edit_step;
                    if (edit_temp_value > (uint16_t)edit_source_item->value_limits.max_val) {
                        edit_temp_value = (uint16_t)edit_source_item->value_limits.min_val;
                    }
                }
                Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
                break;
            }
            case KEY_DOWN: {
                /* Уменьшаем значение (через signed для корректной проверки min) */
                if (edit_value_size == 1) {
                    int8_t v = (int8_t)edit_temp_value;
                    v -= edit_step;
                    if (v < edit_source_item->value_limits.min_val) {
                        v = (int8_t)edit_source_item->value_limits.max_val;
                    }
                    edit_temp_value = (uint16_t)v;
                } else {
                    int16_t v = (int16_t)edit_temp_value;
                    v -= edit_step;
                    if (v < edit_source_item->value_limits.min_val) {
                        v = (int16_t)edit_source_item->value_limits.max_val;
                    }
                    edit_temp_value = (uint16_t)v;
                }
                Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
                break;
            }
            case KEY_ENTER: {
                /* Сохраняем значение */
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_temp_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_temp_value;
                }
                /* Вызываем колбэк */
                if (edit_source_item->on_value_changed) {
                    edit_source_item->on_value_changed();
                }
                Buzzer_PlayTone(1000, 50);
                EditMode_Exit();
                break;
            }
            case KEY_CANCEL: {
                /* Отменяем — восстанавливаем оригинальное значение */
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_original_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_original_value;
                }
                Buzzer_PlayTone(400, 50);
                EditMode_Exit();
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
       char new_text[64];
       
       // Безопасное чтение: копируем в локальную переменную
       uint16_t val16;
       uint8_t  val8;
       
       if (menu_item->value_size == 1) {
           memcpy(&val8, menu_item->data.ptr_value, 1);
       } else {
           memcpy(&val16, menu_item->data.ptr_value, 2);
       }
       
       int val = menu_item->value_size == 1 ? (int)val8 : (int)val16;
       
       // Отладка: выводим все ITEM_TYPE_VALUE
       printf("UMIT: '%s' size=%d val=%d ptr=%p sprite=%p\n", label, menu_item->value_size, val, menu_item->data.ptr_value, ui_item->sprite);

      // Форматирование для CC1101 Freq (fixed-point MHz × 100)
      if (strcmp(label, "Freq MHz") == 0) {
          float freq_mhz = val / 100.0f;
          snprintf(new_text, sizeof(new_text), "%s: %.2f", label, freq_mhz);
      }
      // Форматирование для CC1101 BitRate (fixed-point kbps × 100)
      else if (strcmp(label, "BitRate") == 0) {
          float br_kbps = val / 100.0f;
          snprintf(new_text, sizeof(new_text), "%s: %.2f", label, br_kbps);
      }
       // Форматирование для Modulation (индекс 0..6 → название)
       else if (strcmp(label, "Mod") == 0) {
           static const char* mod_str[] = {
               "ASK", "FSK", "2FSK", "GFSK", "OOK", "4FSK", "MSK"
           };
           if (val >= 0 && val < 7) {
               snprintf(new_text, sizeof(new_text), "%s: %s", label, mod_str[val]);
           } else {
               snprintf(new_text, sizeof(new_text), "%s: %d", label, val);
           }
       }
      // Форматирование для Power (cc1101PowerIndex — uint8_t, читаем 1 байт)
      else if (strcmp(label, "Power") == 0) {
          static const int8_t power_dbm[] = { -30, -20, -15, -10, -3, 0, 5, 10 };
          if (val < 8) {
              snprintf(new_text, sizeof(new_text), "%s: %d dBm", label, power_dbm[val]);
          } else {
              snprintf(new_text, sizeof(new_text), "%s: %d", label, val);
          }
      }
      // Форматирование для RxBw (индекс 0..15 → kHz из таблицы ESP32)
      else if (strcmp(label, "RxBw") == 0) {
          static const char* rxbw_str[] = {
              "58k", "68k", "81k", "102k", "116k", "135k", "162k", "203k",
              "232k", "270k", "325k", "406k", "464k", "541k", "650k", "812k"
          };
          if (val < 16) {
              snprintf(new_text, sizeof(new_text), "%s: %s", label, rxbw_str[val]);
          } else {
              snprintf(new_text, sizeof(new_text), "%s: %d", label, val);
          }
      }
      // Улучшенное форматирование для разных типов настроек
      else if (strcmp(label, "BT") == 0 || strcmp(label, "WiFi") == 0 || 
          strcmp(label, "NTP Auto") == 0 || strcmp(label, "Buzzer") == 0) {
          
          snprintf(new_text, sizeof(new_text), "%s: %s", label, (val ? "On" : "Off"));
      } else if (strcmp(label, "OLED Light") == 0) {
          snprintf(new_text, sizeof(new_text), "%s: Lvl %d", label, val);
      } else if (strcmp(label, "LED Backlight") == 0) {
          if (val == 0) {
              snprintf(new_text, sizeof(new_text), "%s: Off", label);
          } else {
              snprintf(new_text, sizeof(new_text), "%s: Lvl %d", label, val);
          }
      } else {
          // Универсальный вариант для Sys, Room, Freq и т.д.
          snprintf(new_text, sizeof(new_text), "%s: %d", label, val);
      }

      strncpy(ui_item->text_content, new_text, sizeof(ui_item->text_content) - 1);
      ui_item->text_content[sizeof(ui_item->text_content) - 1] = '\0';
  }

void Menu_ProcessTouch(uint16_t tx, uint16_t ty) {
    if (!current_menu_listbox || !current_menu_items) return;
    UIElement_t* lb = current_menu_listbox;

    if (lb->touch_state.drag_active) return;

    /* ===== НИЖНЯЯ ПАНЕЛЬ КНОПОК (y >= 450) ===== */
    if (ty >= 450 && ty < 480) {
        // Grid 4 колонки по 25%: Cancel(0-79) | Up(80-159) | Down(160-239) | Enter(240-319)
        if (tx < 80) {
            // Cancel — выход из подменю
            Menu_PopMenu(lb);
            Buzzer_PlayTone(400, 50);
            } else if (tx < 160) {
            // Up — вверх по меню
            if (lb->props.list_box.selected_index > 0) {
                uint8_t old_idx = lb->props.list_box.selected_index;
                lb->props.list_box.selected_index--;
                lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
                // Scroll up
                uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = lb->font->char_height + pad;
                uint8_t visible_items = lb->h / item_h;
                if (visible_items == 0) visible_items = 1;
                if (lb->props.list_box.selected_index < (int16_t)lb->props.list_box.scroll_offset) {
                    lb->props.list_box.scroll_offset = (uint8_t)lb->props.list_box.selected_index;
                }
                UI_RenderListBoxItem(lb, old_idx);
                UI_RenderListBoxItem(lb, (uint8_t)lb->props.list_box.selected_index);
            }
            Buzzer_PlayTone(800, 30);
            } else if (tx < 240) {
            // Down — вниз по меню
            if (lb->props.list_box.selected_index < (int16_t)current_menu_count - 1) {
                uint8_t old_idx = lb->props.list_box.selected_index;
                lb->props.list_box.selected_index++;
                lb->props.list_box.last_leaf_selected = lb->props.list_box.selected_index;
                // Scroll down
                uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
                uint16_t item_h = lb->font->char_height + pad;
                uint8_t visible_items = lb->h / item_h;
                if (visible_items == 0) visible_items = 1;
                if (lb->props.list_box.selected_index >= (int16_t)(lb->props.list_box.scroll_offset + visible_items)) {
                    lb->props.list_box.scroll_offset = (uint8_t)(lb->props.list_box.selected_index - visible_items + 1);
                }
                UI_RenderListBoxItem(lb, old_idx);
                UI_RenderListBoxItem(lb, (uint8_t)lb->props.list_box.selected_index);
            }
            Buzzer_PlayTone(800, 30);
        } else {
            // Enter — выбрать/открыть пункт
            uint8_t idx = (uint8_t)lb->props.list_box.selected_index;
            if (idx < current_menu_count) {
                Menu_ExecuteSelected(lb, idx);
                Buzzer_PlayTone(1000, 50);
            }
        }
        return;
    }
    /* ===== КОНЕЦ НИЖНЕЙ ПАНЕЛИ ===== */

    /* ===== РЕЖИМ РЕДАКТИРОВАНИЯ ЗНАЧЕНИЯ (TOUCH) ===== */
    if (edit_mode_active && edit_source_item) {
        // В режиме редактирования нижняя панель работает как обычно:
        // Cancel(0-79)/Up(80-159)=увеличить/Down(160-239)=уменьшить/Enter(240-319)=сохранить
        if (ty >= 450 && ty < 480) {
            if (tx < 80) {
                // Cancel — отмена
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_original_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_original_value;
                }
                EditMode_Exit();
                Buzzer_PlayTone(400, 50);
        } else if (tx < 160) {
                // Up — увеличить
                if (edit_value_size == 1) {
                    int8_t v = (int8_t)edit_temp_value;
                    v += edit_step;
                    if (v > edit_source_item->value_limits.max_val) v = edit_source_item->value_limits.min_val;
                    edit_temp_value = (uint16_t)v;
                } else {
                    int16_t v = (int16_t)edit_temp_value;
                    v += edit_step;
                    if (v > edit_source_item->value_limits.max_val) v = edit_source_item->value_limits.min_val;
                    edit_temp_value = (uint16_t)v;
                }
                Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
        } else if (tx < 240) {
                // Down — уменьшить
                if (edit_value_size == 1) {
                    int8_t v = (int8_t)edit_temp_value;
                    v -= edit_step;
                    if (v < edit_source_item->value_limits.min_val) v = edit_source_item->value_limits.max_val;
                    edit_temp_value = (uint16_t)v;
                } else {
                    int16_t v = (int16_t)edit_temp_value;
                    v -= edit_step;
                    if (v < edit_source_item->value_limits.min_val) v = edit_source_item->value_limits.max_val;
                    edit_temp_value = (uint16_t)v;
                }
                Update_EditDisplay();
                Buzzer_PlayTone(800, 30);
            } else {
                // Enter — сохранить
                if (edit_value_size == 1) {
                    *(uint8_t*)edit_source_item->data.ptr_value = (uint8_t)edit_temp_value;
                } else {
                    *(uint16_t*)edit_source_item->data.ptr_value = edit_temp_value;
                }
                if (edit_source_item->on_value_changed) {
                    edit_source_item->on_value_changed();
                }
                EditMode_Exit();
                Buzzer_PlayTone(1000, 50);
            }
        }
        return;
    }

    MenuItem_t* item = NULL;
    UIElement_t* ui_item = NULL;
    int8_t selected = -1;

    // Для VALUE/ACTION — не меняем selected_index (навигация), только last_leaf_selected (выделение)
    // UI_ListBox_ProcessTouch меняет selected_index — нам это не нужно для листовых пунктов
    int16_t local_y = ty - lb->y;
    uint16_t font_h = (lb->font != NULL) ? lb->font->char_height : font_arial_9_struct.char_height;
    uint8_t pad = (lb->props.list_box.item_padding > 0) ? lb->props.list_box.item_padding : MENU_LISTBOX_ITEM_PADDING;
    uint16_t item_h = font_h + pad;
    int8_t target = lb->props.list_box.scroll_offset + (int8_t)(local_y / item_h);
    
    if (target >= 0 && (uint8_t)target < lb->children_count && 
        target >= 0 && (uint8_t)target < current_menu_count) {
        selected = target;
        item = &current_menu_items[selected];
        ui_item = (UIElement_t*)lb->children[selected];
    }
    
    if (selected < 0) return;

    switch (item->type) {
        case ITEM_TYPE_VALUE:
            if (ui_item) Update_MenuItem_Text(ui_item, item);
            // Выделение листа без смены навигации
            if (lb->props.list_box.last_leaf_selected != selected) {
                int8_t old = lb->props.list_box.last_leaf_selected;
                lb->props.list_box.last_leaf_selected = selected;
                if (old >= 0) UI_RenderListBoxItem(lb, (uint8_t)old);
                UI_RenderListBoxItem(lb, selected);
            }
            Menu_ExecuteSelected(lb, (uint8_t)selected);
            break;
            
        case ITEM_TYPE_ACTION:
            // Выделение листа без смены навигации
            if (lb->props.list_box.last_leaf_selected != selected) {
                int8_t old = lb->props.list_box.last_leaf_selected;
                lb->props.list_box.last_leaf_selected = selected;
                if (old >= 0) UI_RenderListBoxItem(lb, (uint8_t)old);
                UI_RenderListBoxItem(lb, selected);
            }
            Menu_ExecuteSelected(lb, (uint8_t)selected);
            break;
            
        case ITEM_TYPE_SUBMENU:
            // SUBMENU не выделяем — переходим в подменю
            Menu_ExecuteSelected(lb, (uint8_t)selected);
            break;
            
        default:
            break;
    }
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