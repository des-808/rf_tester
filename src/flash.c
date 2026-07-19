#include <w25qxx.h>
#include <string.h>
//#include "fonts.h"
#include <flash.h>

//#define FONT_BIN_ADDRESS   0x000000  // Начальный адрес в Flash
//#define FONT_SIZE          14112     // Размер segoe_print_12.bin

// Прочитать шрифт в RAM
    uint8_t font_buf[14112];

void SPI_Write_Font(const uint8_t* font_data) {
    uint8_t status;

    // Сначала стираем блок (минимум — сектор 4 КБ)
    // Для W25Q80/16/32 можно стереть весь чип или один/несколько секторов.
    // Удобнее всего — стереть чип один раз при прошивке.
    // Но если нужно обновить — стираем по секторам.

    // Пример: стираем секторы, которые покрывают 0x000000–0x003FFF (16 КБ)
    W25Qx_Erase_Block(0x000000);     // стираем 1-й сектор (4KB)
    W25Qx_Erase_Block(0x001000);     // 2-й сектор
    W25Qx_Erase_Block(0x002000);     // 3-й сектор
    W25Qx_Erase_Block(0x003000);     // 4-й сектор

    // Теперь пишем по частям (страницами по 256 байт)
    uint32_t addr = 0;
    uint32_t remaining = FONT_SIZE;
    while (remaining > 0) {
        uint32_t chunk_size = (remaining > 256) ? 256 : remaining;
        status = W25Qx_Write((uint8_t*)font_data + addr, addr, chunk_size);
        if (status != W25Qx_OK) {
            // error handler
            return;
        }
        addr += chunk_size;
        remaining -= chunk_size;
    }

    //printf("Font written to Flash.\n");
}

void SPI_Read_Font(uint8_t* buffer, uint32_t offset, uint32_t size) {
    uint8_t status = W25Qx_Read(buffer, offset, size);
    if (status != W25Qx_OK) {
        // обработать ошибку
    }
}


void init_flash() {
    // Инициализация Flash
    if (W25Qx_Init() != W25Qx_OK) {
        // ошибка
        while (1);
    }

    // Если нужно — записать шрифт (один раз, например, по кнопке)
    // SPI_Write_Font(segoe_print_12_data);  // если segoe_print_12_data[] есть в ROM

    // Прочитать шрифт в RAM
    uint8_t font_buf[14112];
    SPI_Read_Font(font_buf, FONT_BIN_ADDRESS, FONT_SIZE);

    // Теперь используем font_buf как массив битов (нужно будет в своей функции отрисовки текста)

}