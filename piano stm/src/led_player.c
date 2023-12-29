#include "stm32f4xx_hal.h"
#include <led_player.h>
#include <leds.h>

#define MAX_NOTES 4

typedef struct {
    uint8_t channel;
    uint8_t note;
    uint8_t vel;
    uint32_t tick;
}MIDINote;

typedef struct {
    MIDINote notes[MAX_NOTES];
    void (*lighter)(float *colors, MIDINote *led);
}LedNote;

//Cool piecewise function values
static const float damp[] = { 0.0f, 0.05595f, 0.11080000000000001f, 0.16455f, 0.2172f, 0.26875000000000004f, 0.31920000000000004f, 0.36855000000000004f, 0.4168f, 0.4639500000000001f, 0.51f, 0.55495f, 0.5988f, 0.6415500000000001f, 0.6832000000000001f, 0.72375f, 0.7632f, 0.8015500000000001f, 0.8388000000000001f, 0.8749500000000001f, 0.9100000000000001f, 0.9439500000000002f, 0.9768000000000001f, 1.00855f, 1.0392000000000001f, 1.06875f, 1.0972f, 1.1245500000000002f, 1.1508000000000003f, 1.17595f, 1.2000000000000002f, 1.22295f, 1.2448000000000001f, 1.2655500000000002f, 1.2852000000000001f, 1.30375f, 1.3212000000000002f, 1.33755f, 1.3528000000000002f, 1.3669499999999999f, 1.3800000000000001f, 1.39195f, 1.4028000000000005f, 1.4125500000000002f, 1.4212000000000002f, 1.4287500000000004f, 1.4352f, 1.44055f, 1.4448000000000003f, 1.44795f, 1.4500000000000002f, 1.4509500000000004f, 1.4508f, 1.4495500000000001f, 1.4472000000000005f, 1.44375f, 1.4392000000000003f, 1.43355f, 1.4268000000000003f, 1.4189500000000002f, 1.4100000000000001f, 1.3999500000000005f, 1.3887999999999998f, 1.3765500000000004f, 1.3632f, 1.34875f, 1.3332000000000002f, 1.3165499999999999f, 1.2988f, 1.2799500000000004f, 1.2600000000000002f, 1.23895f, 1.2168000000000005f, 1.1935500000000006f, 1.1692f, 1.1437500000000007f, 1.1172000000000004f, 1.0895500000000005f, 1.0607999999999995f, 1.0309500000000003f, 1.0f, 0.9697473999999999f, 0.9419392000000002f, 0.9164998000000002f, 0.8933536000000002f, 0.8724250000000001f, 0.8536384000000001f, 0.8369182000000001f, 0.8221888f, 0.8093746f, 0.7984f, 0.7891894f, 0.7816672f, 0.7757577999999999f, 0.7713856f, 0.768475f, 0.7669504f, 0.7667362f, 0.7677568f, 0.7699365999999999f, 0.7731999999999999f, 0.7774714f, 0.7826752f, 0.7887357999999999f, 0.7955776000000001f, 0.803125f, 0.8113024f, 0.8200341999999999f, 0.8292448f, 0.8388586000000002f, 0.8487999999999998f, 0.8589934f, 0.8693632000000002f, 0.8798337999999998f, 0.8903295999999998f, 0.9007749999999997f, 0.9110943999999999f, 0.9212121999999998f, 0.9310527999999996f, 0.9405405999999998f, 0.9495999999999998f, 0.9581554000000001f, 0.9661312000000002f, 0.9734517999999999f, 0.9800416000000001f, 0.9858250000000002f, 0.9907263999999998f, 0.9946701999999998f, 0.9975808000000004f, 0.9993826000000001f, };
static const uint8_t channel_colors[16][3] = {
    {255, 0, 0},
    {255, 174, 0},
    {153, 255, 0},
    {0, 255, 17},
    {0, 255, 242},
    {0, 110, 255},
    {13, 0, 255},
    {153, 0, 255},
    {255, 0, 230},
    {0, 0, 0},   //channel 10
    {255, 0, 132},
    {251, 255, 0},
    {185, 177, 250},
    {132, 255, 130},
    {255, 130, 130},
    {255, 255, 255},
};

static const uint8_t led_mapping[LED_COUNT] = {
    21, 22, 20, 23, 19, 18, 24, 17, 25, 16, 26, 15, 
    14, 27, 13, 28, 12, 11, 29, 10, 30, 9, 31,8,
    7, 32, 6, 33, 5, 4, 34, 3, 35, 2, 0, 1
};

static LedNote led_keys[LED_COUNT];

static inline uint8_t get_index(int note){
    note -= 60;
    while(note < 0){
        note += LED_COUNT;
    }
    return note % LED_COUNT;
} 

static void led_player_silence_chan(uint8_t channel){
    for(uint8_t i = 0; i != LED_COUNT; i++){
        for(uint8_t j = 0; j != MAX_NOTES; j++){
            if(led_keys[i].notes[j].note == 255)continue;
            if(led_keys[i].notes[j].channel != channel)continue;
            led_keys[i].notes[j].vel = 0;
        }
    }
}

static float amount(uint32_t tick){
    tick *= 4.5;
    if(tick >= sizeof(damp) / 4)return 1;
    return damp[tick];
}

//I'm way too lazy to implement code for faulty leds
#if 0
static void rb_lighter(float *colors, MIDINote *led){
    float brightness = amount(led->tick);
    colors[0] += brightness;
}

static void rg_lighter(float *colors, MIDINote *led){
    float brightness = amount(led->tick);
    colors[0] += brightness;
}

static void r_lighter(float *colors, MIDINote *led){
    float brightness = amount(led->tick);
    colors[0] += brightness;
}
#endif

static void rgb_lighter(float *colors, MIDINote *led){
    float brightness = amount(led->tick);
    const uint8_t *ch_colors = channel_colors[led->channel];

    colors[0] += ch_colors[0] * brightness * (1 / 255.0f);
    colors[1] += ch_colors[1] * brightness * (1 / 255.0f);
    colors[2] += ch_colors[2] * brightness * (1 / 255.0f);
}

void led_player_init(){
    leds_init();
    for(uint8_t i = 0; i != LED_COUNT; i++){
        for(uint8_t j = 0; j != MAX_NOTES; j++){
            led_keys[i].notes[j].note = 255;
        }

        led_keys[i].lighter = rgb_lighter;
    }
}

static void led_player_tick(){
    for(uint8_t i = 0; i != LED_COUNT; i++){
        MIDINote *notes = led_keys[i].notes;
        void (*call)(float *, MIDINote *) = led_keys[i].lighter;
        float rgb[3] = {0, 0, 0};

        for(uint8_t j = 0; j != MAX_NOTES; j++){
            if(notes[j].note == 255)continue;
            call(rgb, notes + j);
            if(notes[j].vel == 0)notes[j].note = 255;
            notes[j].tick++;
        }

        rgb[0] *= 0.25f;
        rgb[1] *= 0.25f;
        rgb[2] *= 0.25f;
        if(rgb[0] > 1.0f)rgb[0] = 1.0f;
        if(rgb[1] > 1.0f)rgb[1] = 1.0f;
        if(rgb[2] > 1.0f)rgb[2] = 1.0f;

        LedColor c = {200 * rgb[0], 200 * rgb[1], 200 * rgb[2]};
        leds_set(c, led_mapping[i]);
    }

    leds_show();
}

void led_player_update(){
    static uint32_t prev_time = 0;
    uint32_t time = HAL_GetTick();
    if(time - prev_time > (1000 / 50)){
        led_player_tick();
        prev_time = time;
    }
}

void led_player_note_on(uint8_t channel, uint8_t note, uint8_t vel){
    if(channel == 0x09)return;
    MIDINote *notes = led_keys[get_index(note)].notes;
    for(uint8_t i = 0; i != MAX_NOTES; i++){
        if(notes[i].note != 255)continue;
        notes[i].tick = 0;
        notes[i].note = note;
        notes[i].channel = channel;
        notes[i].vel = vel;
        break;
    }
}

void led_player_note_off(uint8_t channel, uint8_t note){
    if(channel == 0x09)return;
    MIDINote *notes = led_keys[get_index(note)].notes;
    for(uint8_t i = 0; i != MAX_NOTES; i++){
        if(notes[i].note == 255)continue;
        if(notes[i].channel != channel || notes[i].note != note)continue;
        notes[i].vel = 0;
    }
}

void led_player_test(){
    int idx = 0;
    while(1){
        leds_clear();
        leds_set(leds_color(0.5, 0.5, 0.5), led_mapping[idx]);
        leds_show();
        HAL_Delay(500);
        idx++;
        idx %= 12;//LED_COUNT;
    }
}

void led_player_handle_cc(uint8_t channel, uint8_t cmd, uint8_t data){
    switch(cmd){
        //Mute sounds
        case 120:
        case 123:
            led_player_silence_chan(channel);
            break;
    }
}
