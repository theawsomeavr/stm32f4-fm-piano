#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

void oled_init();
void oled_benchmark();
void oled_draw_line(int x, int y, int x2, int y2);
void oled_draw_line_dotted(int x, int y, int x2, int y2);
void oled_show();

#endif
