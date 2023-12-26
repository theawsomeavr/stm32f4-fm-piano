#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

typedef enum {
    A_attack_max,
    A_attack,
    A_decay,
    A_sustain,
    A_release,
    A_multi,
    A_wave,
}AudioSetting;

typedef enum {
    A_volume,
}PercSetting;

void audio_init();
void audio_update();
void audio_test();
void audio_play_note(uint8_t channel, uint8_t note, uint8_t vel);
void audio_stop_note(uint8_t channel, uint8_t note);
void audio_update_patch(uint8_t channel, uint8_t op, AudioSetting setting, float v);
void audio_update_perc(PercSetting setting, float v);
void audio_handle_cc(uint8_t channel, uint8_t cmd, uint8_t value);
float audio_get_sample_vol();

#endif
