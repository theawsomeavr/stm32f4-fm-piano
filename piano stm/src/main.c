#include "stm32f4xx.h"
#include "usb_device.h"
#include <SWO_debug.h>
#include <graphics.h>
#include <led_player.h>
#include <audio.h>

void SystemClock_Config(void);

int main(void){
    HAL_Init();
    SystemClock_Config();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOAEN;
    GPIOC->MODER = (GPIOC->MODER & ~GPIO_MODER_MODER13) | (0b01 << GPIO_MODER_MODER13_Pos);

    MX_USB_DEVICE_Init();
    led_player_init();
    audio_init();
    graphics_init();

    while(1){
        audio_update();
        led_player_update();
        graphics_update();
    }
}

void USB_MIDI_dout(MIDIMesg *data){
    switch(data->cmd & 0xf0){
        case 0x90:
            if(data->arg2 == 0){
                audio_stop_note(data->cmd & 0x0f, data->arg1);
                led_player_note_off(data->cmd & 0x0f, data->arg1);
            }
            else {
                audio_play_note(data->cmd & 0x0f, data->arg1, data->arg2);
                led_player_note_on(data->cmd & 0x0f, data->arg1, data->arg2);
            }
            break;
        case 0x80:
            audio_stop_note(data->cmd & 0x0f, data->arg1);
            led_player_note_off(data->cmd & 0x0f, data->arg1);
            break;
        case 0xB0:
            audio_handle_cc(data->cmd & 0x0f, data->arg1, data->arg2);
            led_player_handle_cc(data->cmd & 0x0f, data->arg1, data->arg2);
            break;
    }
}

//Unpack the 7 bit values
void MIDI_sysex_decode(uint8_t *buf, uint16_t *cursor, uint8_t data){
    for(uint8_t i = 0; i != 7; i++){
        uint8_t *dest = buf + (*cursor) / 8;
        uint8_t bit_idx = (*cursor) & 0x07;
        uint8_t bit = (data >> i) & 0x01;
        *dest |= bit << bit_idx;
        (*cursor)++;
    }
}

void USB_MIDI_sysex(uint8_t *data, uint16_t size){
    uint8_t buf[16];
    uint8_t channel_op, slider;
    uint16_t cursor = 0;

    data++;
    size--;

    if(size < 4)return;
    if(*(uint16_t *)data != *(uint16_t *)"TH")return;
    memset(buf, 0, 16);
    data += 2;
    size -= 2;
    channel_op = data[0];
    slider = data[1];
    data += 2;
    size -= 2;

    for(uint8_t i = 0; i != size; i++){
        if(data[i] == 0xF7)break;
        MIDI_sysex_decode(buf, &cursor, data[i]);
    }

    uint8_t target = channel_op & 0x01;
    uint8_t channel = channel_op >> 1;

    if(channel == 9){
        switch(slider){
            case 0:
                audio_update_perc((PercSetting)slider, *(float *)buf);
                break;
        }
        return;
    }
    switch(slider){
        case 0 ... 5:
            audio_update_patch(channel, target, (AudioSetting)slider, *(float *)buf);
            break;
        case 6:
            audio_update_patch(channel, target, (AudioSetting)slider, *(int *)buf);
            break;
    }
}
