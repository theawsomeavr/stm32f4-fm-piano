#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>
#define LED_COUNT 36

typedef struct {
    uint8_t r, g, b;
}LedColor;

LedColor leds_hue(float hue);
LedColor leds_color(float r, float g, float b);

void leds_init();
void leds_clear();
void leds_set(LedColor color, uint8_t led);
void leds_show();

#endif
