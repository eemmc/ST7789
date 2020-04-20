#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "st7789.h"
#include "slicer.h"


#define LCD_WIDTH  320
#define LCD_HEIGHT 240

typedef struct {
    LCD_ST7789_DRI *driver;
    Slicer         *slicer;
    int        scale_width;
    int       scale_height;
} Memory;


int display_frame(void *pointer, uint8_t *buffer, int linesize){
    fprintf(stderr, "display_frame ready.\n");

    Memory *refs = (Memory*)pointer;

    int i;
    int ret = 0;

    ret |= refs->driver->output(refs->driver, NULL, 0, 0);

    uint8_t *p = buffer;
    for(i = 0; i <240; i++){
        ret |= refs->driver->output(refs->driver, p, 640, 1);
        p += linesize;
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

    if(refs->slicer->init(refs->slicer, argv[1]) != 0){
        fprintf(stderr, "Slicer init Failed!\n");
        goto END;
    }

    double scale;
    if(refs->slicer->width > refs->slicer->height){
        scale = LCD_WIDTH / (double)refs->slicer->width;
        refs->scale_width  = LCD_WIDTH;
        refs->scale_height = (int)(refs->slicer->height * scale);
    }else{
        scale = LCD_HEIGHT / (double)refs->slicer->height;
        refs->scale_width  = (int)(refs->slicer->width * scale);
        refs->scale_height = LCD_HEIGHT;
    }



    snprintf(refs->slicer->command, sizeof(refs->slicer->command),
             "scale=%d:%d", 320, 240);

    if(refs->driver->config(refs->driver, 0, 0,320, 240) != 0){
        fprintf(stderr, "LCD config Failed\n");
        goto END;
    }


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
