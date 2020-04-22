#include "st7789.h"
#include "bcm2835.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define LCD_ST7789_WIDTH  240
#define LCD_ST7789_HEIGHT 320

#define LCD_ST7789_GPIO_SPI_PIN_RES   24
#define LCD_ST7789_GPIO_SPI_PIN_DC    25


typedef struct {
    uint8_t region[8];
} LCD_ST7798_MT;


int lcd_st7789_write_command(uint8_t cmd){
    bcm2835_gpio_clr(LCD_ST7789_GPIO_SPI_PIN_DC);
    bcm2835_spi_writenb((const char*)&cmd, 1);
    return 0;
}

int lcd_st7789_write_data(uint8_t *data, uint32_t size){
    bcm2835_gpio_set(LCD_ST7789_GPIO_SPI_PIN_DC);
    bcm2835_spi_writenb((const char*)data, size);
    return 0;
}

int lcd_st7789_write_unwrap(uint8_t *data, uint32_t size){
    bcm2835_spi_writenb((const char*)data, size);
    return 0;
}

int lcd_st7789_clear(void){
    int ret = 0;
    {
        uint8_t region[8] ={
            0x00, 0x00, 0x00, 0xF0,
            0x00, 0x00, 0x01, 0x40
        };

        ret |= lcd_st7789_write_command(0x2A);
        ret |= lcd_st7789_write_data(region, 4);
        ret |= lcd_st7789_write_command(0x2B);
        ret |= lcd_st7789_write_data(region + 4, 4);
        ret |= lcd_st7789_write_command(0x2C);
    }

    {
        int i;
        uint32_t linesize = LCD_ST7789_WIDTH * 2;
        uint8_t *blanks = (uint8_t*)malloc(linesize);
        memset(blanks, 0, linesize);
        for(i = 0; i < LCD_ST7789_HEIGHT; i++){
            ret |= lcd_st7789_write_data(blanks, linesize);
        }
        free(blanks);
    }

    if(ret != 0) {
        perror("[Error] - lcd st7789 clear Failed");
        return -1;
    }

    return 0;
}

/**[显示重置]*/
int lcd_st7789_reset(){
    int ret = 0;
    uint8_t param[16];

    {// 重置引脚
        bcm2835_gpio_set(LCD_ST7789_GPIO_SPI_PIN_RES);
        usleep(100000);
        bcm2835_gpio_clr(LCD_ST7789_GPIO_SPI_PIN_RES);
        usleep(100000);
        bcm2835_gpio_set(LCD_ST7789_GPIO_SPI_PIN_RES);
        usleep(100000);
    }


    {//设置显示扫描方向 默认值
        ret |= lcd_st7789_write_command(0x36);
        param[0] = 0x00;
        ret |= lcd_st7789_write_data(param, 1);
    }
    {//设置像素格式rgb 16bit/pixel 65K
        ret |= lcd_st7789_write_command(0x3A);
        param[0] = 0x05;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置门控制? 默认值
        ret |= lcd_st7789_write_command(0xB2);
        param[0] = 0x0C;
        param[1] = 0x0C;
        param[2] = 0x00;
        param[3] = 0x33;
        param[4] = 0x33;
        ret |= lcd_st7789_write_data(param, 5);
    }

    {//设置闸门控制? （高位: 13.26, 低位: -10.43)
        ret |= lcd_st7789_write_command(0xB7);
        param[0] = 0x35;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置VCOM? (0x37对应值为 1.475)
        ret |= lcd_st7789_write_command(0xBB);
        param[0] = 0x19;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置LCM控制? 默认值
        ret |= lcd_st7789_write_command(0xC0);
        param[0] = 0x2C;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//cmd 0xC2 0x01 0xFF 设置VDV和VRH开启 默认值
        ret |= lcd_st7789_write_command(0xC2);
        param[0] = 0x01;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置VRH (0x12对应值为 4.45+( vcom+vcom offset+vdv))
        ret |= lcd_st7789_write_command(0xC3);
        param[0] = 0x12;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置VDV (0x20对应值为 0)
        ret |= lcd_st7789_write_command(0xC4);
        param[0] = 0x20;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置刷新率 默认值 (0x0F对应值为 60Hz)
        ret |= lcd_st7789_write_command(0xC6);
        param[0] = 0x0F;
        ret |= lcd_st7789_write_data(param, 1);
    }

    {//设置电源控制? 默认值
        ret |= lcd_st7789_write_command(0xD0);
        param[0] = 0xA4;
        param[1] = 0xA1;
        ret |= lcd_st7789_write_data(param, 2);
    }

    {//设置正电压伽马控制？ (有默认值)
        ret |= lcd_st7789_write_command(0xE0);
        param[0]  = 0xD0;
        param[1]  = 0x04;
        param[2]  = 0x0D;
        param[3]  = 0x11;
        param[4]  = 0x13;
        param[5]  = 0x2B;
        param[6]  = 0x3F;
        param[7]  = 0x54;
        param[8]  = 0x4C;
        param[9]  = 0x18;
        param[10] = 0x0D;
        param[11] = 0x0B;
        param[12] = 0x1F;
        param[13] = 0x23;
        ret |= lcd_st7789_write_data(param, 14);
    }

    {//设置负电压伽马控制? (有默认值)
        ret |= lcd_st7789_write_command(0xE1);
        param[0]  = 0xD0;
        param[1]  = 0x04;
        param[2]  = 0x0C;
        param[3]  = 0x11;
        param[4]  = 0x13;
        param[5]  = 0x2C;
        param[6]  = 0x3F;
        param[7]  = 0x44;
        param[8]  = 0x51;
        param[9]  = 0x2F;
        param[10] = 0x1F;
        param[11] = 0x1F;
        param[12] = 0x20;
        param[13] = 0x23;
        ret |= lcd_st7789_write_data(param, 14);
    }

    {//cmd 0x21 设置显示反色打开
        ret |= lcd_st7789_write_command(0x21);
    }

    {//cmd 0x11 唤醒
        ret |= lcd_st7789_write_command(0x11);
    }

    {//cmd 0x29 设置显示打开
        ret |= lcd_st7789_write_command(0x29);
    }

    {// 清空像素
        ret |= lcd_st7789_clear();
    }

    if(ret != 0) {
        perror("[Error] - Problem perpare spi dev");
        return -1;
    }

    return 0;
}


int lcd_st7789_config(void *self, int16_t left, int16_t top, int16_t right, int16_t bottom){
    LCD_ST7789_DRI *drive = (LCD_ST7789_DRI*)self;
    LCD_ST7798_MT *mem = (LCD_ST7798_MT*)drive->priv;

    if(bcm2835_init() == 0){
        fprintf(stderr, "bcm2835_init failed. \n");
        return -1;
    }else{
        bcm2835_gpio_fsel(LCD_ST7789_GPIO_SPI_PIN_RES, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_fsel(LCD_ST7789_GPIO_SPI_PIN_DC,  BCM2835_GPIO_FSEL_OUTP);
    }
    if(bcm2835_spi_begin() == 0){
        printf("bcm2835_spi_begin failed.\n");
        return -1;
    }else{
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
        bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_8);
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, 0);
    }

    mem->region[0]  = (left >> 8) & 0xFF;
    mem->region[1]  = left & 0xFF;
    mem->region[2]  = (right >> 8) & 0xFF;
    mem->region[3]  = right & 0xFF;
    mem->region[4]  = (top >> 8) & 0xFF;
    mem->region[5]  = top & 0xFF;
    mem->region[6]  = (bottom >> 8) & 0xFF;
    mem->region[7]  = bottom & 0xFF;

    if(lcd_st7789_reset() != 0){
        fprintf(stderr, "LCD_ST7789 reset Failed\n");
        return -1;
    }
    return 0;
}


int lcd_st7789_output(void* self, uint8_t* data, uint32_t size, uint8_t append){
    LCD_ST7789_DRI *drive = (LCD_ST7789_DRI*)self;
    LCD_ST7798_MT *mem = (LCD_ST7798_MT*)drive->priv;

    int ret = 0;

    if(append == 0x00){
        ret |= lcd_st7789_write_command(0x2A);
        ret |= lcd_st7789_write_data(mem->region, 4);
        ret |= lcd_st7789_write_command(0x2B);
        ret |= lcd_st7789_write_data(mem->region + 4, 4);
        ret |= lcd_st7789_write_command(0x2C);
    }

    if(size > 0){
        ret |= lcd_st7789_write_data(data, size);
    }

    if(ret != 0){
        fprintf(stderr, "LCD_ST7789 output Failed\n");
        return -1;
    }

    return 0;
}

int lcd_st7789_clean(void** self){
    LCD_ST7789_DRI *drive = (LCD_ST7789_DRI*)(*self);
    LCD_ST7798_MT *mem = (LCD_ST7798_MT*)drive->priv;

    {//关闭 gpio 使能
        bcm2835_gpio_fsel(LCD_ST7789_GPIO_SPI_PIN_RES, BCM2835_GPIO_FSEL_INPT);
        bcm2835_gpio_fsel(LCD_ST7789_GPIO_SPI_PIN_DC,  BCM2835_GPIO_FSEL_INPT);
        bcm2835_spi_end();
        bcm2835_close();
    }

    free(mem);
    free(drive);

    *self = NULL;

    return 0;
}


LCD_ST7789_DRI* lcd_st7789_init(void){
    LCD_ST7789_DRI *drive = NULL;

    drive = (LCD_ST7789_DRI*)malloc(sizeof (LCD_ST7789_DRI));
    memset(drive, 0, sizeof (LCD_ST7789_DRI));

    drive->priv = (LCD_ST7798_MT*)malloc(sizeof (LCD_ST7798_MT));
    memset(drive->priv, 0, sizeof (LCD_ST7798_MT));

    drive->config = &lcd_st7789_config;
    drive->output = &lcd_st7789_output;
    drive->clean  = &lcd_st7789_clean;

    return drive;
}
