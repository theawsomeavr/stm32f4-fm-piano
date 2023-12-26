#include "SWO_debug.h"
//This also includes core_cmX.h which contains the ITM_SendChar function
#include <stm32f4xx.h>
#include <stdio.h>

//#define USE_SERIAL
uint32_t _pow10(uint8_t e) {
    uint32_t r = 1;
    for (uint8_t i = 0; i != e; i++)r *= 10;
    return r;
}
uint32_t _pow16(uint8_t e) {
    uint32_t r = 1;
    for (uint8_t i = 0; i != e; i++)r *= 0x10;
    return r;
}
uint32_t _pow2(uint8_t e) {
    uint32_t r = 1;
    r <<= e;
    return r;
}

void SWO_init(){
    #ifdef USE_SERIAL
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
        GPIOA->CRH = (GPIOA ->CRH & ~GPIO_CRH_CNF9_0) | GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_0;
        USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
        USART1->BRR=(39<<4);
        #else
        //RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
        //AFIO->MAPR = (AFIO->MAPR & ~(AFIO_MAPR_SWJ_CFG_0 | AFIO_MAPR_SWJ_CFG_1 | AFIO_MAPR_SWJ_CFG_2)) | AFIO_MAPR_SWJ_CFG_1;
    #endif
}

void SWO_send_ln(){
    SWO_send_byte('\n');    
}

void SWO_send_byte(uint8_t b){
    #ifdef USE_SERIAL
        while(!(USART1->SR & USART_SR_TXE));
        USART1->DR = b;
        #else
        ITM_SendChar(b); 
    #endif
}

void SWO_send_str(char * str){
    while (*str) {
        SWO_send_byte(*str++);
    }
}

void SWO_send_str_ln(char * str){
    while (*str) {
        SWO_send_byte(*str++);
    }
    SWO_send_byte('\n');
}

void display_dec(int32_t value) {    
    char str[20];
    sprintf(str,"%d", (int)value);
    SWO_send_str(str);
}

void display_hex(uint32_t value) {
    uint8_t digits = 1;
    for(uint8_t i = 0; i != 8; i++) {
        if (_pow16(digits + 1) > value)break;
        digits++;
    }
    
    for (int8_t i = digits; i != -1; i--) {
        uint8_t digit = (value / _pow16(i)) & 0xF;
        SWO_send_byte(digit + ((digit < 10) ? '0' : 'A' - 10));
    }
}

void display_bin(uint32_t value) {
    uint8_t digits = 31;
    for(; digits != 0; digits--) {
        if (_pow2(digits) & value)break;
    }
    
    for (int8_t i = digits; i != -1; i--) {
        uint8_t digit = (value / _pow2(i)) & 0x01;
        SWO_send_byte(digit + '0');
    }
}

void SWO_send_int(int32_t value, uint8_t base){
    switch (base) {
        case DEC:
        display_dec(value);
        break;
        
        case HEX:
        SWO_send_byte('0');
        SWO_send_byte('x');
        display_hex(value);
        break;
        
        case BIN:
        SWO_send_byte('0');
        SWO_send_byte('b');
        display_bin(value);
        break;
    }
}

void SWO_send_float(float v){
    char str[20];
    sprintf(str,"%d.%03u", (int)v, (int) ((v - (int)v) * 1000) );
    SWO_send_str(str);
}            

void SWO_send_hex(const void *data, uint8_t bytes){
    SWO_send_str("[");
    for(uint8_t i = 0; i != bytes; i++){
        SWO_send_int(*((uint8_t *)data + i), HEX);
        if(i != bytes - 1)SWO_send_str(", ");
    }
    SWO_send_str("]\n");
}
