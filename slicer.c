#include "slicer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>

typedef struct {
    AVFormatContext *ifmt_ctx;
    AVCodecContext *codec_ctx;
    AVFilterContext *fil_src_ctx;
    AVFilterContext *fil_swp_ctx;
    AVFilterGraph *fil_graph;
    AVRational time;
    AVPacket packet;
    AVFrame *sframe;
    AVFrame *fframe;
    int64_t last_pts;
    int64_t cur_usec;
    int stream_index;
    int decode_flush;
    int filter_flush;

    struct timeval times[4];
} SlicerMemory;

int slicer_filter_init(Slicer *slicer){
    SlicerMemory *mem = (SlicerMemory*)slicer->priv;

    int ret = 0;
    char args[512];
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *bufferswp = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs    = avfilter_inout_alloc();
    AVFilterInOut *inputs     = avfilter_inout_alloc();
    AVRational time_base = mem->ifmt_ctx->streams[mem->stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_RGB565BE, AV_PIX_FMT_NONE };

    mem->fil_graph = avfilter_graph_alloc();
    if(!outputs || !inputs || !mem->fil_graph){
        ret = AVERROR(ENOMEM);
        goto END;
    }

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             mem->codec_ctx->width, mem->codec_ctx->height,
             mem->codec_ctx->pix_fmt, time_base.num, time_base.den,
             mem->codec_ctx->sample_aspect_ratio.num,
             mem->codec_ctx->sample_aspect_ratio.den);

    if((ret = avfilter_graph_create_filter(&mem->fil_src_ctx, buffersrc, "in",
                                           args, NULL, mem->fil_graph)) < 0){
        fprintf(stderr, "Could not create buffer source\n");
        goto END;
    }

    if((ret = avfilter_graph_create_filter(&mem->fil_swp_ctx, bufferswp, "out",
                                           NULL, NULL, mem->fil_graph)) < 0){
        fprintf(stderr, "Could not create buffer sink\n");
        goto END;
    }

    if((ret = av_opt_set_int_list(mem->fil_swp_ctx, "pix_fmts", pix_fmts,
                                  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0){
        fprintf(stderr, "Could not set output pixel format\n");
        goto END;
    }

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = mem->fil_src_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name        = av_strdup("out");
    inputs->filter_ctx  = mem->fil_swp_ctx;
    inputs->pad_idx     = 0;
    inputs->next        = NULL;

    if((ret = avfilter_graph_parse_ptr(mem->fil_graph, slicer->command,
                                       &inputs, &outputs, NULL)) < 0){
        goto END;
    }

    if((ret = avfilter_graph_config(mem->fil_graph, NULL)) < 0){
        goto END;
    }

END:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return 0;
}

int slicer_display_frame(SlicerMemory *mem, SlicerCallback callback, void *refs){

    gettimeofday(&mem->times[0], NULL);

//    struct timeval stamp;
//    int64_t delay, offset;
//    if(mem->fframe->pts != AV_NOPTS_VALUE){
//        if(mem->last_pts != AV_NOPTS_VALUE){
//            delay = av_rescale_q(mem->fframe->pts - mem->last_pts,
//                                 mem->time, AV_TIME_BASE_Q);
//            if(delay > 0 && delay < 5000000){
//                gettimeofday(&stamp, NULL);
//                offset = stamp.tv_sec * 1000000 + stamp.tv_usec;
//                mem->cur_usec += delay;
//                if(mem->cur_usec > offset){
//                    usleep(mem->cur_usec - offset);
//                }
//            }
//        }else{
//            gettimeofday(&stamp, NULL);
//            mem->cur_usec = stamp.tv_sec * 1000000 + stamp.tv_usec;
//        }

//        mem->last_pts = mem->fframe->pts;
//    }

    if(callback != NULL){
        callback(refs, mem->fframe->data[0], mem->fframe->linesize[0]);
    }

    gettimeofday(&mem->times[1], NULL);

    timersub(&mem->times[0], &mem->times[3], &mem->times[2]);
    fprintf(stderr, "ffmpeg use time.: %lu.%lu\n", mem->times[2].tv_sec, mem->times[2].tv_usec);
    timersub(&mem->times[1], &mem->times[0], &mem->times[2]);
    fprintf(stderr, "lcd use time.: %lu.%lu\n", mem->times[2].tv_sec, mem->times[2].tv_usec);
    timersub(&mem->times[1], &mem->times[3], &mem->times[2]);
    fprintf(stderr, "all use time.: %lu.%lu\n", mem->times[2].tv_sec, mem->times[2].tv_usec);


    gettimeofday(&mem->times[3], NULL);
    return 0;
}

int slicer_filter_frame(SlicerMemory *mem, SlicerCallback callback, void *refs){
    int ret;
    AVFrame *frame = !mem->filter_flush ? mem->sframe : NULL;
    if((ret = av_buffersrc_add_frame_flags(mem->fil_src_ctx, frame,
                                           AV_BUFFERSRC_FLAG_KEEP_REF)) < 0){
        return ret;
    }

    while(1){
        ret = av_buffersink_get_frame(mem->fil_swp_ctx, mem->fframe);
        if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN)){
            break;
        }else if(ret < 0){
            return ret;
        }

        mem->time = mem->fil_swp_ctx->inputs[0]->time_base;
        if((ret = slicer_display_frame(mem, callback, refs)) < 0){
            return ret;
        }

        av_frame_unref(mem->fframe);
    }

    return 0;
}

int slicer_decode_frame(SlicerMemory *mem, SlicerCallback callback, void *refs){
    int ret;
    AVPacket *packet = !mem->decode_flush ? &mem->packet : NULL;
    if((ret = avcodec_send_packet(mem->codec_ctx, packet)) < 0){
        return ret;
    }

    while(ret >= 0){
        ret = avcodec_receive_frame(mem->codec_ctx, mem->sframe);
        if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN)){
            break;
        }else if(ret < 0){
            return ret;
        }

        mem->sframe->pts = mem->sframe->best_effort_timestamp;

        if((ret = slicer_filter_frame(mem, callback, refs)) < 0){
            return ret;
        }

        av_frame_unref(mem->sframe);
    }
    return 0;
}



int slicer_init(void *self, const char *filename){
    Slicer *slicer=(Slicer*)self;
    SlicerMemory *mem = (SlicerMemory*)slicer->priv;

    int ret;
    AVCodec *codec;
    AVCodecParameters *codecpar;

    if((ret = avformat_open_input(&mem->ifmt_ctx, filename, NULL, NULL)) < 0){
        fprintf(stderr, "Could not open input file\n");
        return ret;
    }

    if((ret = avformat_find_stream_info(mem->ifmt_ctx, NULL)) < 0){
        fprintf(stderr, "Could not find stream information\n");
        return ret;
    }

    if((ret = av_find_best_stream(mem->ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) < 0){
        fprintf(stderr, "Could not find a video stream in the input file\n");
        return ret;
    }

    mem->stream_index = ret;
    if(!(mem->codec_ctx = avcodec_alloc_context3(codec))){
        fprintf(stderr, "Could not allocate codec context \n");
        return AVERROR(ENOMEM);
    }

    codecpar = mem->ifmt_ctx->streams[mem->stream_index]->codecpar;
    if((ret = avcodec_parameters_to_context(mem->codec_ctx, codecpar)) < 0){
        fprintf(stderr, "Could not parameters to context\n");
        return ret;
    }

    if((ret = avcodec_open2(mem->codec_ctx, codec, NULL)) < 0){
        fprintf(stderr, "Could not open video decoder\n");
        return ret;
    }

    slicer->width  = mem->codec_ctx->width;
    slicer->height = mem->codec_ctx->height;

    return 0;
}

int slicer_loop(void *self, SlicerCallback callback, void *refs){
    Slicer *slicer = (Slicer*)self;
    SlicerMemory *mem = (SlicerMemory*)slicer->priv;

    int ret;

    mem->sframe = av_frame_alloc();
    mem->fframe = av_frame_alloc();
    if(!mem->sframe || !mem->fframe){
        fprintf(stderr, "Could not allocate buffer frame\n");
        return 1;
    }

    if((ret = slicer_filter_init(slicer)) < 0){
        return ret;
    }

    while(1){
        if((ret = av_read_frame(mem->ifmt_ctx, &mem->packet)) < 0){
            break;
        }

        if(mem->packet.stream_index == mem->stream_index){
            if((ret = slicer_decode_frame(mem, callback, refs)) < 0){
                return ret;
            }
        }

        av_packet_unref(&mem->packet);
    }

    mem->decode_flush = 1;
    if((ret = slicer_decode_frame(mem, callback, refs)) < 0){
        return ret;
    }

    mem->filter_flush = 1;
    if((ret = slicer_filter_frame(mem, callback, refs)) < 0){
        return ret;
    }

    return 0;
}

int slicer_free(void *self){
    Slicer *slicer = (Slicer*)self;
    SlicerMemory *mem = (SlicerMemory*)slicer->priv;

    avfilter_graph_free(&mem->fil_graph);
    avcodec_free_context(&mem->codec_ctx);
    avformat_close_input(&mem->ifmt_ctx);
    av_packet_unref(&mem->packet);
    av_frame_free(&mem->fframe);
    av_frame_free(&mem->sframe);

    free(mem);

    free(slicer);

    return 0;
}

Slicer * slicer_new(void){
    Slicer *slicer;
    SlicerMemory *mem;

    slicer = (Slicer*)malloc(sizeof (Slicer));
    memset(slicer, 0, sizeof (Slicer));

    mem = (SlicerMemory*)malloc(sizeof (SlicerMemory));
    memset(mem, 0, sizeof (SlicerMemory));
    mem->last_pts = AV_NOPTS_VALUE;

    slicer->init = &slicer_init;
    slicer->loop = &slicer_loop;
    slicer->free = &slicer_free;
    slicer->priv = mem;

    return slicer;
}
