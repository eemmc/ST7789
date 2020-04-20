#ifndef FFMPEG_SLICER_H
#define FFMPEG_SLICER_H


#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*SlicerCallback)(void *refs, uint8_t *buffer, int linesize);

typedef struct {
    int(*init)(void *self, const char *filename);
    int(*loop)(void *self, SlicerCallback callback, void *refs);
    int(*free)(void *self);

    char command[128];
    int width;
    int height;

    void *priv;
} Slicer;

Slicer * slicer_new(void);

#ifdef __cplusplus
}
#endif

#endif // FFMPEG_SLICER_H
