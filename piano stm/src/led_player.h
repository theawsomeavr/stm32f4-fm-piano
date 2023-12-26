#ifndef LED_PLAYER_H
#define LED_PLAYER_H

#include <stdint.h>

void led_player_init();
void led_player_update();
void led_player_note_on(uint8_t channel, uint8_t note, uint8_t vel);
void led_player_note_off(uint8_t channel, uint8_t note);
void led_player_handle_cc(uint8_t channel, uint8_t cmd, uint8_t data);

#endif
