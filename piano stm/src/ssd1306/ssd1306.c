#include <ssd1306.h>
#include <stdlib.h>
#include <stm32f4xx.h>
#include <string.h>

#define CS_0() GPIOA->ODR &= ~GPIO_ODR_ODR_4
#define CS_1() GPIOA->ODR |= GPIO_ODR_ODR_4
#define CMD()  GPIOA->ODR &= ~GPIO_ODR_ODR_6
#define DRAW() GPIOA->ODR |= GPIO_ODR_ODR_6

#define SPI_W(data) \
    SPI1->DR = data; \
    while(!(SPI1->SR & SPI_SR_TXE)); \
    while(SPI1->SR & SPI_SR_BSY);

static uint8_t dumb;
static uint8_t oled_framebuffer[1024];
static uint8_t framebuffer_xmit_done;

static const uint8_t setup_data[] = {
    0xA8, 0x3f,
    0xD3, 0x00,
    0x40,
    0xA1, 0xC0, //Vertical flip
    0xDA, 0x12, //Alternate row mode
    0x81, 0xDD, //Contrast/Brightness
    0xA4,
    0xA6,
    0xD5, 0x80,
    0x8D, 0x14,
    0xAF, //Display on
    0x20, 0x00,   //Horizontal data mode
    0x21, 0, 127, //0 -> 127 cols
    0x22, 0, 7    //0 -> 7 pages
};

void oled_init(){
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    //PA5 and PA6 to alternate function
    //PA3, PA4, PA6 to normal mode
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER5) | (0b10 << GPIO_MODER_MODER5_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER7) | (0b10 << GPIO_MODER_MODER7_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER3) | (0b01 << GPIO_MODER_MODER3_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER4) | (0b01 << GPIO_MODER_MODER4_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER6) | (0b01 << GPIO_MODER_MODER6_Pos);

    //AF05 for PA1 (SPI1)
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL5) | (5 << GPIO_AFRL_AFSEL5_Pos);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL7) | (5 << GPIO_AFRL_AFSEL7_Pos);

    SPI1->CR1 = (0b010 << SPI_CR1_BR_Pos) | SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM;
    SPI1->CR1 |= SPI_CR1_SPE;

    //Channel 3 (SPI1 TX); 8 bit size; Mem -> Perif
    DMA2_Stream3->CR = (3 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | (0b00 << DMA_SxCR_MSIZE_Pos) | (0b01 << DMA_SxCR_DIR_Pos);
    DMA2_Stream3->NDTR = 1024;
    DMA2_Stream3->M0AR = (uint32_t)oled_framebuffer;
    DMA2_Stream3->PAR  = (uint32_t)&SPI1->DR;
    DMA2_Stream3->CR |= DMA_SxCR_TCIE;
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);

    GPIOA->ODR &= ~GPIO_ODR_ODR_3;
    HAL_Delay(1);
    GPIOA->ODR |= GPIO_ODR_ODR_3;

    //D/C pin (High == data, Low == cmd)
    CMD();
    CS_0();

    for(uint8_t i = 0; i != sizeof(setup_data); i++){
        SPI_W(setup_data[i]);
    }

    DRAW();
    for(int i = 0; i != 1024; i++){
        oled_framebuffer[i] = 0x00;
        SPI_W(0x00);
    }

    CS_1();
    framebuffer_xmit_done = 1;
}

void oled_show(){
    while(!framebuffer_xmit_done);
    framebuffer_xmit_done = 0;
    CMD();
    CS_0();
    SPI_W(0x20);
    SPI_W(0x00);
    SPI_W(0x21);
    SPI_W(0);
    SPI_W(127);
    SPI_W(0x22);
    SPI_W(0);
    SPI_W(7);

    DRAW();
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    DMA2_Stream3->NDTR = 1024;
    DMA2_Stream3->M0AR = (uint32_t)oled_framebuffer;
    DMA2_Stream3->PAR  = (uint32_t)&SPI1->DR;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
}

void DMA2_Stream3_IRQHandler(){
    if(DMA2->LISR & DMA_LISR_TCIF3){
        DMA2->LIFCR |= DMA_LIFCR_CTCIF3;

        SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
        CS_1();
        memset(oled_framebuffer, 0, 1024);
        framebuffer_xmit_done = 1;
    }
}

static inline void oled_draw_pixel(int x, int y){
    if(x < 0 || x > 127)return;
    if(y < 0 || y > 63)return;
    oled_framebuffer[x + ((y & 0b11111000) << 4)] |= 1 << (y & 0x7);
}

// Extremely Fast Line Algorithm Var B (Multiplication)
// Copyright 2001-2, By Po-Han Lin

//Freely useable in non-commercial applications as long as credits 
// to Po-Han Lin and link to http://www.edepot.com is provided in source 
// code and can been seen in compiled executable.  Commercial 
// applications please inquire about licensing the algorithms.
//
// Lastest version at http://www.edepot.com/phl.html

// THE EXTREMELY FAST LINE ALGORITHM Variation D (Addition Fixed Point)
void oled_draw_line(int x, int y, int x2, int y2){
    uint8_t yLonger = 0;
    int shortLen = y2 - y;
    int longLen = x2 - x;
    int abs_longLen = abs(longLen);
    int abs_shortLen = abs(shortLen);

    register int start = x; 
    register int end = x2; 
    register int j; 

    if(abs_shortLen > abs_longLen){
        int swap = shortLen;
        shortLen = longLen;
        longLen = swap;
        abs_longLen = abs_shortLen;
        yLonger = 1;

        start = y;
        end = y2;
        j = x << 8;
    }
    else {
        j = y << 8;
    }

    register int L_inc = 1;
	register int S_inc = (shortLen << 8) / abs_longLen;

    if(longLen < 0)L_inc = -1;
    if(yLonger){
        for(; start != end; start+=L_inc) {
            oled_draw_pixel(j >> 8, start);
            j += S_inc;
        }
    } else {
        for(; start != end; start+=L_inc) {
            oled_draw_pixel(start, j >> 8);
            j += S_inc;
        }
    }
}

void oled_draw_line_dotted(int x, int y, int x2, int y2){
    uint8_t yLonger = 0;
    int shortLen = y2 - y;
    int longLen = x2 - x;
    int abs_longLen = abs(longLen);
    int abs_shortLen = abs(shortLen);

    register int start = x; 
    register int end = x2; 
    register int j; 

    if(abs_shortLen > abs_longLen){
        int swap = shortLen;
        shortLen = longLen;
        longLen = swap;
        abs_longLen = abs_shortLen;
        yLonger = 1;

        start = y;
        end = y2;
        j = x << 8;
    }
    else {
        j = y << 8;
    }

    register int L_inc = 1;
	register int S_inc = (shortLen << 8) / abs_longLen;

    if(longLen < 0)L_inc = -1;
    if(yLonger){
        for(; start != end; start+=L_inc) {
            if(start & 1)oled_draw_pixel(j >> 8, start);
            j += S_inc;
        }
    } else {
        for(; start != end; start+=L_inc) {
            if(start & 1)oled_draw_pixel(start, j >> 8);
            j += S_inc;
        }
    }
}
