#ifndef LCD_ST7789_H
#define LCD_ST7789_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct{
    int (*config)(void* self, int16_t left, int16_t top, int16_t right, int16_t bottom);
    int (*output)(void* self, uint8_t* data, uint32_t size, uint8_t append);
    int (*clean)(void** self);
    void *priv;
} LCD_ST7789_DRI;


LCD_ST7789_DRI* lcd_st7789_init(void);


#ifdef __cplusplus
}
#endif


#endif // LCD_ST7789_H
