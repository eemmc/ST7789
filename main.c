#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "st7789.h"
#include "slicer.h"


#define LCD_WIDTH  240
#define LCD_HEIGHT 320

typedef struct {
    LCD_ST7789_DRI *driver;
    Slicer         *slicer;
    int        scale_width;
    int       scale_height;
} Memory;



int display_frame(void *pointer, uint8_t *buffer, int linesize){
    Memory *refs = (Memory*)pointer;

    int i;
    int ret = 0;

    ret |= refs->driver->output(refs->driver, NULL, 0, 0);

    uint32_t flinesize = refs->scale_height * 2;
    for(i = 0; i < refs->scale_width - 2; i++){
        ret |= refs->driver->output(refs->driver, buffer + linesize * i, flinesize, 1);
    }

    if(ret != 0){
        fprintf(stderr, "LCD display_frame Failed!\n");
        return -1;
    }

    return 0;
}


int main(int argc, char **argv) {

    if(argc != 2){
        fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
        return 1;
    }

    Memory *refs = (Memory*)malloc(sizeof (Memory));
    refs->driver = lcd_st7789_init();
    refs->slicer = slicer_new();

    //读取视频基本信息
    if(refs->slicer->init(refs->slicer, argv[1]) != 0){
        fprintf(stderr, "Slicer init Failed!\n");
        goto END;
    }

    //计算缩放比例
    double scales[3];
    scales[0] = (double)LCD_WIDTH / LCD_HEIGHT;
    scales[1] = (double)refs->slicer->height / refs->slicer->width;
    if(scales[0] >= scales[1]){
        scales[2] = LCD_HEIGHT / (double)refs->slicer->width;
        refs->scale_width  = LCD_HEIGHT;
        refs->scale_height = (int)(refs->slicer->height * scales[2]);
    }else{
        scales[2] = LCD_WIDTH / (double)refs->slicer->height;
        refs->scale_width  = (int)(refs->slicer->width * scales[2]);
        refs->scale_height = LCD_WIDTH;
    }

    //缩放后顺时针旋转90度
    snprintf(refs->slicer->command, sizeof(refs->slicer->command),
             "scale=%d:%d,transpose=clock",
             refs->scale_width, refs->scale_height);

    //计算LCD边缘偏移
    int16_t left   = (LCD_WIDTH - refs->scale_height) / 2;
    int16_t top    = (LCD_HEIGHT - refs->scale_width) / 2;
    int16_t right  = left + refs->scale_height - 1;
    int16_t bottom = top + refs->scale_width - 1;
    fprintf(stderr, "size: [%d, %d, %d, %d]\n", left, top, right, bottom);

    if(refs->driver->config(refs->driver, left, top, right, bottom) != 0){
        fprintf(stderr, "LCD config Failed\n");
        goto END;
    }

    //循环解码
    if(refs->slicer->loop(refs->slicer, &display_frame, refs) != 0){
        fprintf(stderr, "Slicer parse video Failed!\n");
        goto END;
    }

END:
    if(refs->slicer != NULL){
        refs->slicer->free(refs->slicer);
    }

    if(refs->driver != NULL){
        refs->driver->clean((void**)&refs->driver);
    }

    free(refs);

    return 0;
}
