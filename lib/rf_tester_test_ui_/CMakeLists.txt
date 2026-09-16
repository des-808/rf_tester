SET(SOURCES components/ui_comp_panel1.c
    components/ui_comp.c
    screens/ui_MenuMain.c
    screens/ui_Settings.c
    screens/ui_CC1101.c
    screens/ui_NRF24L01.c
    screens/ui_LORA.c
    screens/ui_RSSIPLOTTER.c
    screens/ui_ABOUT.c
    screens/ui_Screen8.c
    screens/ui_Screen9.c
    screens/ui_Screen10.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
