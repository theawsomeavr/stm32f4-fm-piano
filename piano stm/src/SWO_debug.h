#ifndef SWO_DEBUG_H
#define SWO_DEBUG_H
#include <inttypes.h>

#define DEC 0
#define BIN 1
#define HEX 2

void SWO_init();
void SWO_send_ln();
void SWO_send_byte(uint8_t b);
void SWO_send_int(int32_t value, uint8_t base);
void SWO_send_float(float v);
void SWO_send_str(char *str);
void SWO_send_str_ln(char *str);
void SWO_send_hex(const void *data, uint8_t bytes);

#endif
