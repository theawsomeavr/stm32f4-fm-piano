#include "tables.h"
#include <audio.h>
#include <drum.h>
#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <string.h>

#define AUDIO_CHANS 16
#define PERC_CHANS 4
#define AUDIO_BUF_SIZE 512
#define AUDIO_SET_ADSR(n, op, ...) {\
    typedef struct {\
        float A_max, A, D, S, R;\
    }ADSRLocal;\
    ADSRLocal l = __VA_ARGS__;\
    memcpy(&audio_voices[n].op.A_max, &l, sizeof(ADSRLocal));\
}

typedef enum {
    Idle,
    Attack,
    Decay,
    AfterDecay,
    Release
}ADSRState;

typedef struct {
    float volume;
    float A_max;
    float A,D,S,R;
    uint32_t factor;
    const float *wave;
    ADSRState state;
}VoiceOp;

typedef struct {
    uint32_t ev_tick;
    uint8_t playing, note, channel;
    VoiceOp fm, carrier;
    uint32_t acc, inc;
}AudioVoice;

typedef struct {
    uint32_t ev_tick;
    uint8_t note;
    const uint8_t *sample;
    uint32_t size, acc, inc;
    float volume;
}PercVoice;

typedef struct {
    float A_max;
    float A,D,S,R;
    uint32_t factor;
    const float *wave;
}ChannelOp;

typedef struct {
    //TODO: implement
    float volume, pitch;
    ChannelOp fm, carrier;
}ChannelSettings;

static AudioVoice audio_voices[AUDIO_CHANS];
static PercVoice perc_voices[PERC_CHANS];
static float global_perc_vol;
static ChannelSettings channel_settings[16];

static const float sine_fact = 5726.623061f;//45812.98449;
static uint32_t audio_buf[AUDIO_BUF_SIZE];
static uint8_t audio_target_buf;

static inline uint32_t audio_get_sample(){
    float final_sample = 0;

    for(uint32_t i = 0; i != AUDIO_CHANS; i++){
        AudioVoice *v = audio_voices + i;
        //make them sync
        uint16_t fm_idx = ((uint32_t)(v->acc * v->fm.factor) >> 17) & 0xfff;
        uint16_t carrier_idx = ((uint32_t)(v->acc * v->carrier.factor) >> 17);

        float float_fm_sample = v->fm.wave[fm_idx] * v->fm.volume;
        float float_sample = v->carrier.wave[(carrier_idx + (int)(float_fm_sample * 6144)) & 0xfff];

        v->acc += v->inc;
        final_sample += float_sample * v->carrier.volume;
    }

    for(uint8_t i = 0; i != PERC_CHANS; i++){
        PercVoice *v = perc_voices + i;
        uint32_t idx = v->acc >> 8;
        if(idx > v->size)continue;
        v->acc += v->inc;
        final_sample += ((int)v->sample[idx] - 127) * (1 / 32.0f) * v->volume;
    }

    //Volume can range between -8 -> +8
    int32_t dynamic_sample = final_sample * 767.0f;
    return compressor_table[dynamic_sample + 12288];
}

static void audio_fill(){
    uint32_t *ptr = audio_buf + (AUDIO_BUF_SIZE / 2) * audio_target_buf;
    for(uint32_t i = 0; i != AUDIO_BUF_SIZE / 2; i++){
        ptr[i] = audio_get_sample();
    }

    audio_target_buf = !audio_target_buf;
}

static void audio_process_adsr(VoiceOp *op){
    float next_volume = op->volume;
    switch(op->state){
        //TODO: Implement volume mixing
        case Idle: return;
        case Attack:
            next_volume += op->A;
            if(next_volume >= op->A_max){
                op->state = Decay;
                next_volume = op->A_max;
            }
            break;
        case Decay:
            next_volume -= op->D;
            if(next_volume <= op->S){
                op->state = AfterDecay;
                next_volume = op->S;
            }
            break;
        case AfterDecay:
            next_volume = op->S;
            break;
        case Release:
            next_volume -= op->R;
            if(next_volume <= 0.0f){
                op->state = Idle;
                next_volume = 0.0f;
            }
            break;
    }
    op->volume = next_volume;
}

static void audio_tick_adsr(){
    for(uint8_t i = 0; i != AUDIO_CHANS; i++){
        audio_process_adsr(&audio_voices[i].fm);
        audio_process_adsr(&audio_voices[i].carrier);
        if(audio_voices[i].playing){
            if(audio_voices[i].carrier.volume == 0.0f){
                audio_voices[i].playing = 0;
            }
        }
    }
}

static void audio_start_voice(AudioVoice *v, uint8_t channel, uint8_t note, uint8_t volume){
    float freq = freq_table[note];
    ChannelSettings *c = channel_settings + channel;

    //Load the channel "instrument"
    memcpy(&v->fm.A_max, &c->fm, sizeof(ChannelOp));
    memcpy(&v->carrier.A_max, &c->carrier, sizeof(ChannelOp));

    v->ev_tick = HAL_GetTick();
    v->channel = channel;
    v->note = note;
    v->carrier.state = Attack;
    v->fm.state = Attack;
    v->playing = 1;

    v->inc = freq * sine_fact;
}

static void audio_start_perc(PercVoice *v, uint8_t note, uint8_t volume){
    ChannelSettings *c = channel_settings + 9;
    v->ev_tick = HAL_GetTick();
    v->note = note;
    v->inc = 0;
    v->acc = 0;

#define SAMPLE(s) \
    v->sample = s; \
    v->size = sizeof(s) - 1;
    
    switch(note){
        case 35: case 36:
            SAMPLE(kick);
            break;
        case 38: case 40:
            SAMPLE(snare);
            break;
        case 42: case 44:
            SAMPLE(closed_hihat);
            break;
        case 37:
            //SAMPLE(stick);
            break;
        case 46:
            SAMPLE(open_hi_hat);
            break;
        case 49: case 52: case 55: case 57:
            SAMPLE(crash);
            break;
        case 51: case 59:
            SAMPLE(ride);
            break;
        case 50:
            SAMPLE(tomm);
            break;
        case 48:
            SAMPLE(tomm);
            break;
        case 47:
            SAMPLE(tomm);
            break;
        case 45:
            SAMPLE(tomm);
            break;
        case 43:
            SAMPLE(tomm);
            break;
        case 41:
            SAMPLE(tomm);
            break;
        default:
            v->sample = safe_sample;
            v->size = 0;

#if 0
      if (mididata == 50) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		percussion_increment[a]=pecussion_constant*1.5;
		LEDS_volume[high_tomm]=255;
        LEDS_color_offset[high_tomm] += color_offset;
      }
	  else if (mididata == 48) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		percussion_increment[a]=pecussion_constant*1.2;
		LEDS_volume[highmid_tomm]=255;
         LEDS_color_offset[highmid_tomm] += color_offset;
      }
	  else if (mididata == 47) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		LEDS_volume[highlow_tomm]=255;
         LEDS_color_offset[highlow_tomm] += color_offset;
      }
	  else if (mididata == 45) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		percussion_increment[a]=pecussion_constant*0.8;
		LEDS_volume[lowmid_tomm]=255;
        LEDS_color_offset[lowmid_tomm] += color_offset;
      }
	  else if (mididata == 43) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		percussion_increment[a]=pecussion_constant*0.6;
		LEDS_volume[low_tomm]=255;
        LEDS_color_offset[low_tomm] += color_offset;
      }
	  else if (mididata == 41) {
        set_percussion(a,tomm,sizeof(tomm),mididata,velocity);
		percussion_increment[a]=pecussion_constant*0.4;
		LEDS_volume[low_tomm]=255;
        LEDS_color_offset[low_tomm] += color_offset;
      }

#endif
    }
    v->inc = 0.512f * 380;
    v->volume = (volume / 127.0f) * global_perc_vol * c->volume;
}

static void audio_play_perc(uint8_t note, uint8_t vel){
    PercVoice *v, *last_v = perc_voices, *same_note = NULL;
    uint32_t max_lifetime = 0;
    uint32_t cur_tick = HAL_GetTick();

    for(uint8_t i = 0; i != PERC_CHANS; i++){
        v = perc_voices + i;
        //If the sample is done in the current channel, use it
        if((v->acc >> 8) > v->size)goto assign;

        if(v->note == note)same_note = v;

        uint32_t lifetime = cur_tick - v->ev_tick;
        if(lifetime >= max_lifetime){
            last_v = v;
            max_lifetime = lifetime;
        }
    }

    v = last_v;
    if(same_note)v = same_note;
assign:
    //Play the note
    audio_start_perc(v, note, vel);
}

static void audio_stop_voice(AudioVoice *v){
    v->carrier.state = Release;
    v->fm.state = Release;
}

void audio_init(){
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    //PA1 to alternate function
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER1) | (0b10 << GPIO_MODER_MODER1_Pos);
    //AF01 for PA1 (TIM2 2)
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL1) | (1 << GPIO_AFRL_AFSEL1_Pos);

    TIM2->CCMR1 = TIM_CCMR1_OC2PE | (0b110 << TIM_CCMR1_OC2M_Pos);
    TIM2->CCER = TIM_CCER_CC2E;
    TIM2->PSC = 0;
    TIM2->ARR = 2047;
    TIM2->CR1 = TIM_CR1_ARPE;
    TIM2->DIER = TIM_DIER_UDE;

    //Channel 3 (TIM2 UP); 16 bit size; Mem -> Perif
    DMA1_Stream1->CR = (3 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC |
                       DMA_SxCR_CIRC |
                       (0b10 << DMA_SxCR_MSIZE_Pos) |
                       (0b10 << DMA_SxCR_PSIZE_Pos) |
                       (0b10 << DMA_SxCR_PL_Pos) |
                       (0b01 << DMA_SxCR_DIR_Pos);

    DMA1_Stream1->NDTR = AUDIO_BUF_SIZE;
    DMA1_Stream1->M0AR = (uint32_t)audio_buf;
    DMA1_Stream1->PAR  = (uint32_t)&TIM2->CCR2;
    DMA1_Stream1->CR |= DMA_SxCR_HTIE | DMA_SxCR_TCIE;

    //Quirks of using sram and optimizations
    audio_target_buf = 0;
    global_perc_vol = 1;
    memset(audio_voices, 0, sizeof(audio_voices));
    memset(perc_voices, 0, sizeof(perc_voices));
    for(uint8_t i = 0; i != AUDIO_CHANS; i++){
        audio_voices[i].carrier.wave = sine_table;
        audio_voices[i].fm.wave = sine_table;
    }

    for(uint8_t i = 0; i != PERC_CHANS; i++){
        perc_voices[i].sample = safe_sample;
        perc_voices[i].inc = 10000;
    }

    for(uint8_t i = 0; i != 16; i++){
        channel_settings[i].carrier.wave = sine_table;
        channel_settings[i].fm.wave = sine_table;
        channel_settings[i].volume = 1;
        channel_settings[i].pitch = 0;
    }

    audio_fill();
    audio_fill();
    NVIC_EnableIRQ(DMA1_Stream1_IRQn);

    DMA1_Stream1->CR |= DMA_SxCR_EN;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void DMA1_Stream1_IRQHandler(){
    if(DMA1->LISR & DMA_LISR_HTIF1){
        DMA1->LIFCR |= DMA_LIFCR_CHTIF1;
        audio_fill();
    }
    if(DMA1->LISR & DMA_LISR_TCIF1){
        DMA1->LIFCR |= DMA_LIFCR_CTCIF1;
        audio_fill();
    }
}

#if 0
void DEBUG_start_note(uint8_t channel, uint8_t note){
    audio_voices[channel].carrier.inc = freq_table[note + 12] * sine_fact;
    audio_voices[channel].fm.inc = freq_table[note + 12] * 2 * sine_fact;
    audio_voices[channel].carrier.volume = 1;
}

void DEBUG_stop_note(uint8_t channel){
    audio_voices[channel].carrier.inc = 0;
    audio_voices[channel].fm.inc = 0;
    audio_voices[channel].carrier.volume = 0;
}

void DEBUG_delay(uint16_t ticks){ HAL_Delay(ticks); }

void DEBUG_play_miditones(const uint8_t *score){
    while(1){
        uint8_t cmd = *score & 0xf0;
        uint8_t chan = *score & 0x0f;
        score++;
        switch(cmd){
            case 0xf0:
                return;
            case 0x90:
                DEBUG_start_note(chan, *score++);
                break;
            case 0x80:
                DEBUG_stop_note(chan);
                break;
            default:
                DEBUG_delay(*(score - 1) << 8 | *score);
                score++;
        }
    }
}
#endif

void audio_update(){
    static uint32_t prev_tick;
    uint32_t cur_tick = HAL_GetTick();
    if((cur_tick - prev_tick) > (1000 / 500)){
        audio_tick_adsr();
        prev_tick = cur_tick;
    }
}

void audio_play_note(uint8_t channel, uint8_t note, uint8_t vel){
    //Ignore MIDI channel 10 (percussion)
    if(channel == 9){
        audio_play_perc(note, vel);
        return;
    }
    AudioVoice *v, *last_v_busy = audio_voices, *last_v_releasing = NULL;
    uint32_t max_lifetime_busy = 0;
    uint32_t max_lifetime_releasing = 0;
    uint32_t cur_tick = HAL_GetTick();

    for(uint8_t i = 0; i != AUDIO_CHANS; i++){
        v = audio_voices + i;
        //If the voice is not playing anything, use it
        if(!v->playing)goto assign;

        //Get the voice that has been playing for the most amount of time (oldest)
        //of the ones that are silencing and those which are not done
        uint32_t lifetime = cur_tick - v->ev_tick;
        if(v->carrier.state == Release){
            if(lifetime >= max_lifetime_releasing){
                last_v_releasing = v;
                max_lifetime_releasing = lifetime;
            }
        }
        else {
            if(lifetime >= max_lifetime_busy){
                last_v_busy = v;
                max_lifetime_busy = lifetime;
            }
        }
    }

    //If there is a voice that is in the process of dying use that for playing the new note
    //else use the oldest busy voice
    if(last_v_releasing)v = last_v_releasing;
    else v = last_v_busy;
assign:
    //Play the note
    audio_start_voice(v, channel, note, vel);
}

void audio_stop_note(uint8_t channel, uint8_t note){
    if(channel == 9)return;
    for(uint8_t i = 0; i != AUDIO_CHANS; i++){
        AudioVoice *v = audio_voices + i;
        if(!v->playing)continue;

        //Search for the voice that is playing this note on the specified channel
        //If there is a match, release it.
        if(v->note == note && v->channel == channel){
            audio_stop_voice(v);
        }
    }
}

//Correction function for linearising the ADSR knobs
static inline float knob_curve(float t){
    return 0.002f/(1.4f*t + 0.0001f);
}

static void audio_update_voices(uint8_t channel, uint8_t op, uint32_t offset, const void *data){
    for(int i = 0; i != AUDIO_CHANS; i++){
        AudioVoice *v = audio_voices + i;
        if(!v->playing || v->channel != channel)continue;
        
        if(op)memcpy((uint8_t *)&v->fm + offset, data, 4);
        else memcpy((uint8_t *)&v->carrier + offset, data, 4);
    }
}

void audio_update_patch(uint8_t channel, uint8_t _op, AudioSetting setting, float v){
    ChannelSettings *c = channel_settings + channel;
    ChannelOp *op = &c->carrier;
    if(_op)op = &c->fm;

    switch(setting){
        case A_attack_max:
            op->A_max = v;
            break;
        case A_attack:
            op->A = knob_curve(v);
            break;
        case A_decay:
            op->D = knob_curve(v);
            break;
        case A_sustain:
            op->S = v;
            audio_update_voices(channel, _op, offsetof(VoiceOp, S), &op->S);
            break;
        case A_release:
            op->R = knob_curve(v);
            break;
        case A_multi:
            op->factor = v * 2;
            audio_update_voices(channel, _op, offsetof(VoiceOp, factor), &op->factor);
            break;
        case A_wave:
            switch((int)v){
                case 0:
                    op->wave = sine_table;
                    break;
                case 1:
                    if(_op)op->wave = abs_sine_table;
                    else op->wave = abs_carrier_sine_table;
                    break;
                case 2:
                    if(_op)op->wave = half_sine_table;
                    else op->wave = half_carrier_sine_table;
                    break;
                case 3:
                    if(_op)op->wave = saw_sine_table;
                    else op->wave = saw_carrier_sine_table;
                    break;
            }
            audio_update_voices(channel, _op, offsetof(VoiceOp, wave), &op->wave);
            break;
    }
}

void audio_update_perc(PercSetting setting, float v){
    switch(setting){
        case A_volume:
            global_perc_vol = v;
            break;
    }
}

void audio_handle_cc(uint8_t channel, uint8_t cmd, uint8_t value){
    ChannelSettings *c = channel_settings + channel;
    switch(cmd){
        //Mute sounds
        case 120:
        case 123:
            for(uint8_t i = 0; i != AUDIO_CHANS; i++){
                AudioVoice *v = audio_voices + i;
                if(v->channel == channel && v->playing)audio_stop_voice(v);
            }
            break;

        case 7:
            c->volume = value / 127.0f;
            break;
    }
}

float audio_get_sample_vol(){
    int acc = 0;
    for(uint16_t i = 0; i != AUDIO_BUF_SIZE / 2; i++){
        acc += abs((int)audio_buf[i << 1] - 1024);
    }

    return acc * (1.0f / (float)(AUDIO_BUF_SIZE / 2) / 256.0f);
}
