#include "font.h"
#include "segoe_print_12_font.h"
#include "font_arial_9.h"
#include "st7796.h"
//extern uint16_t frame_buffer[]; 
// --- Глобальная переменная текущего шрифта ---
Font_t* current_font = NULL;

// --- Функция выбора шрифта ---
void lcd_set_font(const Font_t *font) {
    current_font = (Font_t*)font;
}

// --- Утилиты для работы со шрифтами ---

/* static int get_real_char_width(uint8_t relative_idx) { //вариант поиска размера в длину от начала к концу символа
    if (!current_font) return 0;

    const uint8_t* bitmap = &current_font->data[relative_idx * current_font->char_width * current_font->bytes_per_column];
    
    int cols = current_font->char_width;
*/
    /* for (int col = cols - 1; col >= 0; col--) {
        const uint8_t* col_ptr = &bitmap[col * current_font->bytes_per_column];
        // Проверяем все байты в столбце
        for (int i = 0; i < current_font->bytes_per_column; i++) {
            if (col_ptr[i] != 0) {
                return col + 1;
            }
        }
    } */
   // Проходим по столбцам СПРАВА НАЛЕВО (чтобы найти ПОСЛЕДНИ ненулевой столбец)
   /* for (int col = cols - 1; col >= 0; col--) {
        const uint8_t* col_ptr = &bitmap[col * current_font->bytes_per_column];

        // Проверяем все пиксели в этом столбце так же, как в lcd_draw_char_to_buffer
        for (int row = 0; row < current_font->char_height; row++) {
            uint8_t byte_idx = row / 8;
            uint8_t bit_idx  = row % 8;

            uint8_t col_byte;
            if (current_font->bytes_per_column == 2) {
                col_byte = col_ptr[byte_idx];
            } else if (current_font->bytes_per_column == 3) {
                col_byte = col_ptr[byte_idx];
            } else {
                continue;
            }

            // Если хоть один пиксель установлен — этот столбец нужен
            if (col_byte & (1u << bit_idx)) {
                return col + 1;  // +1, т.к. col=0 → 1 столбец, col=10 → 11 столбцов
            }
        }
    }

    return 1; // хотя бы 1 колонка
} */

static int get_real_char_width(uint8_t relative_idx) { //вариант поиска размера в длину от конца к началу символа
    if (!current_font || !current_font->data) return 0;

    const uint8_t* bitmap = &current_font->data[relative_idx * current_font->char_width * current_font->bytes_per_column];
    
    // Проходим по столбцам СПРАВА НАЛЕВО
    for (int col = current_font->char_width - 1; col >= 0; col--) {
        const uint8_t* col_ptr = &bitmap[col * current_font->bytes_per_column];

        // Проверяем каждый пиксель в этом столбце
        for (int row = 0; row < current_font->char_height; row++) {
            uint8_t byte_idx = row / 8; // 0..2 для height ≤ 24
            uint8_t bit_idx  = row % 8; // 0..7

            // Проверяем только допустимые индексы байтов
            if (byte_idx >= current_font->bytes_per_column) {
                continue; // защита от переполнения (например, height=25, bytes_per_column=2)
            }

            uint8_t col_byte = col_ptr[byte_idx]; // ← берем нужный байт

            // Проверяем нужный бит: (1 << bit_idx)
            if (col_byte & (1u << bit_idx)) {
                return col + 1;  // +1, т.к. col=0 → 1 столбец
            }
        }
    }

    return 1; // хотя бы 1 колонка (на всякий случай)
}

void lcd_draw_char(int16_t x, int16_t y, char c, uint16_t color,uint16_t bg_color,int char_real_width,Sprite_t *sprite) {
    lcd_draw_char_to_buffer(x, y, c, color, bg_color, char_real_width, sprite);
}

// Рисование одного символа — универсальная функция
void lcd_draw_char_to_buffer(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color,int char_real_width,Sprite_t *sprite) {
    if (!current_font || !sprite || !sprite->data) return;

    // ✅ Сдвигаем координаты в относительные
    int16_t rel_x = x - sprite->x;
    int16_t rel_y = y - sprite->y;

    // Проверка: выходим ли мы за пределы спрайта
    if (rel_x + char_real_width < 0 || rel_x >= sprite->w || rel_y + current_font->char_height < 0 || rel_y >= sprite->h) {
        return; // полностью вне видимой области
    }

    uint8_t idx = (uint8_t)c;

    if (idx < current_font->start_char || idx >= current_font->length + current_font->start_char) {
        return;
    }

    uint8_t relative_idx = idx - current_font->start_char;

    // 1. Очищаем фон по real_width
    /* for (int dx = 0; dx < char_real_width; dx++) {
        for (int dy = 0; dy < current_font->char_height; dy++) {
            int px = rel_x + dx;
            int py = rel_y + dy;
            if (px >= 0 && px < sprite->w && py >= 0 && py < sprite->h) {
                //frame_buffer[(uint32_t)py * ST7796_WIDTH + (uint32_t)px] = bg_color;
                sprite->data[(uint32_t)py * sprite->w + (uint32_t)px] = bg_color;
            }
        }
    } */

    // ✅ 1. Очищаем фон по FULL char_width (гарантированно, чтобы не было артефактов)
    // Используем current_font->char_width вместо char_real_width
    for (int dx = 0; dx < current_font->char_width; dx++) {
        for (int dy = 0; dy < current_font->char_height; dy++) {
            int px = rel_x + dx;
            int py = rel_y + dy;
            if (px >= 0 && px < sprite->w && py >= 0 && py < sprite->h) {
                sprite->data[(uint32_t)py * sprite->w + (uint32_t)px] = bg_color;
            }
        }
    }


    // 🔍 ТЕСТ: запишите один белый пиксель в левом верхнем углу символа
    //frame_buffer[y * ST7796_WIDTH + x] = 0xFFFF; // белый

    const int COLS = char_real_width;
    const int ROWS = current_font->char_height;

    const uint8_t* bitmap = &current_font->data[relative_idx * current_font->char_width * current_font->bytes_per_column];

    for (int col = 0; col < COLS; col++) {
        const uint8_t* col_ptr = &bitmap[col * current_font->bytes_per_column];

        for (int row = 0; row < ROWS; row++) {
            uint8_t byte_idx = row / 8;
            uint8_t bit_idx = row % 8;

            uint8_t col_byte;
            if (current_font->bytes_per_column == 2) {
                col_byte = col_ptr[byte_idx]; // ✅ [0] = нижние, [1] = верхние
            } else if (current_font->bytes_per_column == 3) {
                col_byte = col_ptr[byte_idx]; // ✅ [0] = нижние, [1] = средние, [2] = верхние
            } else {
                col_byte = 0; // не поддерживается
            }

            uint8_t bit = (col_byte >> bit_idx) & 1;

                if (bit) {
                    int px = rel_x + col;
                    int py = rel_y + row;
                    if (px >= 0 && px < sprite->w && py >= 0 && py < sprite->h) {
                        uint32_t idx = (uint32_t)py * sprite->w + (uint32_t)px;
                        //frame_buffer[idx] = color;
                        sprite->data[idx] = color;

                        //ST7796_DrawPixel(px, py, color);
                    }
                }
        }
    }
}


void lcd_print(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color,Sprite_t *sprite){
    lcd_print_to_buffer_ex (x,  y, color, str, bg_color,sprite,true);
}



// --- Новая функция: печатает строку в frame_buffer ---
void lcd_print_to_buffer(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color,Sprite_t *sprite) {
    if (!current_font || !sprite || !sprite->data) return;
    
    // ✅ SWAP байты цветов (так как RGB565 уже возвращает swapped значения)
    /*  color = ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
    bg_color = ((bg_color & 0xFF) << 8) | ((bg_color >> 8) & 0xFF);  */
    
    int16_t start_x = x;
    const char *original_str = str;

    while (*str) {
        uint8_t c = (uint8_t)*str;

        if (c == ' ') {
            int half_width = current_font->char_width / 2;
            x += half_width;
            str++;
            continue;
        }

        if (c < 0x80) {
            int relative_idx = c - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)c, color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
                str++;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
                str++;
            }
        }
        // UTF-8 (2 байта) → конвертируем в cp1251
        else if ((c & 0xE0) == 0xC0 && *(str + 1) && ((*(str + 1) & 0xC0) == 0x80)) {
            uint16_t unicode = ((c & 0x1F) << 6) | (str[1] & 0x3F);
            str += 2;

            uint8_t cp1251 = '?';
            if (unicode >= 0x0410 && unicode <= 0x044F) {
                cp1251 = (unicode <= 0x042F) ? (unicode - 0x0410 + 0xC0) : (unicode - 0x0430 + 0xE0);
            } else if (unicode == 0x0401) {
                cp1251 = 0xA8;
            } else if (unicode == 0x0451) {
                cp1251 = 0xB8;
            }

            int relative_idx = cp1251 - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)cp1251, color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
            }
        }
        // cp1251 (однобайтовый)
        else if (c >= 0xC0 && c <= 0xFF) {
            int relative_idx = c - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)c, color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width,sprite);
                x += real_width + current_font->spacing;
            }
            str++;
        }
        else {
            c = (int8_t)'?' - current_font->start_char;
            int real_width = get_real_char_width(c - current_font->start_char);
            lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width,sprite);
            x += current_font->char_width + current_font->spacing;
            str++;
        }
    }

    // Обновляем экран
        int16_t width = lcd_get_str_width(original_str);
        if (width > 0) {
            // ✅ Очищаем весь D-cache перед обновлением экрана (из-за записей в frame_buffer)
            SCB_CleanDCache_by_Addr((uint32_t*)&sprite->data[y * sprite->w + start_x], (width * current_font->char_height * 2 + 31) & ~31);
            __DSB();
            //Sprite_push(sprite, 0, 0);
            //Sprite_push(sprite, sprite->x, sprite->y);
            ST7796_PushSprite(sprite);
        }
}

// --- Новая функция: печатает строку в sprite, с опцией обновления ---
void lcd_print_to_buffer_ex(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color, Sprite_t *sprite, bool update_after_print) {
    if (!current_font || !sprite || !sprite->data) return;

    int16_t start_x = x;
// 1. Расчет локальных координат внутри спрайта
    int16_t local_start_x = x - sprite->x;
    int16_t local_y = y - sprite->y;
    int16_t local_x = local_start_x; 
    const char * original_str = str;

    // ✅ 2. Печатаем строку
    while (*str) {
        uint8_t c = (uint8_t)*str;

        if (c == ' ') {
            int half_width = current_font->char_width / 2;
            x += half_width;
            str++;
            continue;
        }

        if (c < 0x80) {
            int relative_idx = c - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)c, color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
                str++;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
                str++;
            }
        }
        else if ((c & 0xE0) == 0xC0 && *(str + 1) && ((*(str + 1) & 0xC0) == 0x80)) {
            uint16_t unicode = ((c & 0x1F) << 6) | (str[1] & 0x3F);
            str += 2;

            uint8_t cp1251 = '?';
            if (unicode >= 0x0410 && unicode <= 0x044F) {
                cp1251 = (unicode <= 0x042F) ? (unicode - 0x0410 + 0xC0) : (unicode - 0x0430 + 0xE0);
            } else if (unicode == 0x0401) {
                cp1251 = 0xA8;
            } else if (unicode == 0x0451) {
                cp1251 = 0xB8;
            }

            int relative_idx = cp1251 - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)cp1251, color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
            }
        }
        else if (c >= 0xC0 && c <= 0xFF) {
            int relative_idx = c - current_font->start_char;
            if (relative_idx >= 0 && relative_idx < current_font->length) {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, (char)c, color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
            } else {
                int real_width = get_real_char_width(relative_idx);
                lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width, sprite);
                x += real_width + current_font->spacing;
            }
            str++;
        }
        else {
            int real_width = get_real_char_width('?' - current_font->start_char);
            lcd_draw_char_to_buffer(x, y, '?', color, bg_color, real_width, sprite);
            x += current_font->char_width + current_font->spacing;
            str++;
        }
    }

    // 🔥 Добавляем ОДИН вызов ST7796_UpdateSprite по требованию
    /* if (update_after_print) {
        // Переводим начальную абсолютную координату X и Y строки 
        // в относительные координаты внутри массива спрайта
        int16_t rel_start_x = start_x - sprite->x;
        int16_t rel_start_y = y - sprite->y;

        // Если строка печатается с самого начала или частично за пределами спрайта,
        // делаем безопасную отсечку для предотвращения отрицательных индексов
        if (rel_start_x < 0) rel_start_x = 0;
        if (rel_start_y < 0) rel_start_y = 0;

         int16_t width = lcd_get_str_width(original_str);
        if (width > 0) {
            SCB_CleanDCache_by_Addr((uint32_t*)&sprite->data[y * sprite->w + start_x], (width * current_font->char_height * 2 + 31) & ~31);
            __DSB();
            //Sprite_push(sprite, sprite->x, sprite->y);
            ST7796_PushSprite(sprite);
        } 

    } */
   if (update_after_print) {
        int16_t str_width = lcd_get_str_width(original_str);
        // Проверка границ, чтобы избежать HardFault при отрисовке за пределами
        if (str_width > 0 && local_y >= 0 && local_y < sprite->h) {
            int16_t clean_x = (local_start_x < 0) ? 0 : local_start_x;
            if (clean_x < sprite->w) {
                // Использование локальных координат для расчета адреса
                uint16_t* cache_ptr = &sprite->data[(uint32_t)local_y * sprite->w + clean_x];
                uint32_t bytes_to_clean = (uint32_t)str_width * current_font->char_height * 2;
                uint32_t aligned_size = (bytes_to_clean + 31) & ~31;
                SCB_CleanDCache_by_Addr((uint32_t*)cache_ptr, aligned_size);
                __DSB();
                ST7796_PushSprite(sprite);
            }
        }
    }
}


int16_t lcd_get_str_width(const char *str) {
    if (!current_font) return 0;

    int16_t width = 0;

    while (*str) {
        uint8_t c = (uint8_t)*str;

        if (c == ' ') {
            width += current_font->char_width / 2;
            str++;
            continue;
        }

        if (c < 0x80) {
            int relative_idx = c - current_font->start_char;
            int char_width = get_real_char_width(relative_idx);
            width += char_width + current_font->spacing;
            str++;
        }
        else if ((c & 0xE0) == 0xC0 && *(str + 1) && ((*(str + 1) & 0xC0) == 0x80)) {
            uint16_t unicode = ((c & 0x1F) << 6) | (str[1] & 0x3F);
            str += 2;

            uint8_t cp1251 = '?';
            if (unicode >= 0x0410 && unicode <= 0x044F) {
                cp1251 = (unicode <= 0x042F) ? (unicode - 0x0410 + 0xC0) : (unicode - 0x0430 + 0xE0);
            } else if (unicode == 0x0401) {
                cp1251 = 0xA8;
            } else if (unicode == 0x0451) {
                cp1251 = 0xB8;
            }

            int relative_idx = cp1251 - current_font->start_char;
            int char_width = get_real_char_width(relative_idx);
            width += char_width + current_font->spacing;
        } else {
            // Защита: если это реальный конец строки (\0), то width увеличивать не нужно, выходим!
            if (*str == '\0') {
                break;
            }
            width += 1 + current_font->spacing;
            str++;
        }
    }

    return width;
}


void lcd_print_int(int16_t x, int16_t y, uint16_t color, int value, uint16_t bg_color,Sprite_t *sprite) {
    //char buf[12]; // "-2147483648\0" = 12 символов
    int len = 0;
    
    // Обработка нуля
    if (value == 0) {
        int char_real_width = get_real_char_width('0');
        lcd_draw_char_to_buffer(x, y, '0', color,bg_color,char_real_width,sprite);
        return;
    }

    // Обработка отрицательных чисел
    if (value < 0) {
        int char_real_width = get_real_char_width('-');
        lcd_draw_char_to_buffer(x, y, '-', color,bg_color,char_real_width,sprite);
        x += current_font->char_width; // или точнее: + get_char_width('-') + spacing
        value = -value;
        len++;
    }

    // Преобразование в строку (обратный порядок)
    char temp[12];
    while (value > 0) {
        temp[len++] = '0' + (value % 10);
        value /= 10;
    }

    // Вывод в прямом порядке
    for (int i = len - 1; i >= 0; i--) {
        int char_real_width = get_real_char_width(temp[i]);
        lcd_draw_char_to_buffer(x, y, temp[i], color,bg_color,char_real_width,sprite);
        x += get_real_char_width(temp[i] - current_font->start_char) + current_font->spacing;
    }
}

void lcd_print_float(int16_t x, int16_t y, uint16_t color, float value, uint8_t decimals, uint16_t bg_color,Sprite_t *sprite) {
    int32_t integer = (int32_t)value;
    int32_t fraction = (int32_t)((value - integer) * pow(10, decimals));

    lcd_print_int(x, y, color, integer,bg_color,sprite);

    if (decimals > 0) {
        int char_real_width = get_real_char_width('.');
        lcd_draw_char_to_buffer(x, y, '.', color, bg_color,char_real_width,sprite);
        x += current_font->char_width + current_font->spacing;

        // Вывод дробной части (с ведущими нулями)
        char buf[10];
        int len = 0;
        while (fraction > 0 && len < decimals) {
            buf[len++] = '0' + (fraction % 10);
            fraction /= 10;
        }
        while (len < decimals) {
            buf[len++] = '0';
        }

        // Вывод в обратном порядке
        for (int i = len - 1; i >= 0; i--) {
            int char_real_width = get_real_char_width(buf[i]);
            lcd_draw_char_to_buffer(x, y, buf[i], color, bg_color,char_real_width,sprite);
            x += get_real_char_width(buf[i] - current_font->start_char) + current_font->spacing;
        }
    }
}

void lcd_clear_line(uint16_t x,uint16_t y, uint16_t height, uint16_t bg, Sprite_t *sprite) {
    if (!sprite || !sprite->data) return;
    ST7796_FillBufferRect(sprite->data, x, y, sprite->w, height, bg);
}