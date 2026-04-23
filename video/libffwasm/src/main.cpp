
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
     
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include "ffdecode.h"

//#include "ffthread.h"

FILE *outFile;
float _durationMs;
/* 把整个文件的内容全部读进去内存 */
uint8_t *read_file_memory(const char *path, size_t *length)
{
    FILE *pfile;
    uint8_t *data;

    pfile = fopen(path, "rb");
    if (pfile == NULL) {
        return NULL;
    }
    fseek(pfile, 0, SEEK_END);
    *length = ftell(pfile);
    data = (uint8_t *)malloc((*length) * sizeof(uint8_t));
    rewind(pfile);
    *length = fread(data, 1, *length, pfile);
    fclose(pfile);
    pfile = NULL;
    return data;
}

/*
 手动触发暂停播放, 要等seek成功之后才能继续播放
 */
#pragma mark - js callback
typedef void (*JSVideoDecodeCallback)(unsigned char *y,
                                      unsigned char *u,
                                      unsigned char *v,
                                      int line1,
                                      int line2,
                                      int line3,
                                      int width,
                                      int height,
                                      long pts);

typedef void (*JSVideoInfoCallback)(int width,
                                 int height,
                                 int durationMs,
                                 float fps);
     

void JS_VideoDecodeFunc(unsigned char *y,
                        unsigned char *u,
                        unsigned char *v,
                        int line1,
                        int line2,
                        int line3,
                        int width,
                        int height,
                        long pts) {
#ifdef __APPLE__
    printf("width %d, height %d, line1 %d line2 %d pts %ld\n", width, height, line1, line2, pts);
    if (u != NULL) {
        int y_size = width * height;
        int u_size = y_size / 4;
        int v_size = y_size / 4;

        fwrite(y, 1, y_size, outFile);//Y
        fwrite(u, 1, u_size ,outFile);//U:宽高均是Y的一半
        fwrite(v, 1, v_size, outFile);//V:宽高均是Y的一半
    } else {
        fwrite(y, 1, width * height * 4, outFile);
    }
#else
#endif
}

void JS_VideoInfoFunc(int width,
                      int height,
                      int durationMs,
                      float fps) {
  _durationMs = durationMs;
}

typedef struct JSDecodeContext {
    JSVideoDecodeCallback js_decodeCallback;
    JSVideoInfoCallback js_infoCallback;
} JSDecodeContext;

JSDecodeContext *pDecodeCtx = NULL;

#pragma mark - c callback
void InitCallbackFunc(VideoInfo info) {
    printf("%d %d %f %f\n", info.width, info.height, info.durationMs, info.fps);
    if (pDecodeCtx->js_infoCallback) {
        pDecodeCtx->js_infoCallback(info.width, info.height, info.durationMs, info.fps);
    }
}

void DecodeCallbackFunc(AVFrame *frame, float ptsMs) {
    if (pDecodeCtx->js_decodeCallback) {
        pDecodeCtx->js_decodeCallback(frame->data[0], frame->data[1], frame->data[2], frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->width, frame->height, ptsMs);
    }
}

#pragma mark - js interface
long ffwasm_decode_open(uint8_t *data, size_t length, long jscallback) {
//    const char *config = avcodec_configuration();
//    printf("open_decode === \n%s\n", config);
    pDecodeCtx = (JSDecodeContext *)malloc(sizeof(JSDecodeContext));
    if (!pDecodeCtx) {
        return -1;
    }
    pDecodeCtx->js_infoCallback = NULL;
    pDecodeCtx->js_decodeCallback = NULL;
    
    if (jscallback) {
        pDecodeCtx->js_infoCallback = (JSVideoInfoCallback)jscallback;
        long ret = ffcpp_decode_init(data, length, FF_PIX_FMT_I420, InitCallbackFunc, DecodeCallbackFunc);
        return ret;
    } else {
        printf("decode error: no register callback\n");
        return -1;
    }
    return 0;
}

long ffwasm_decode_frame(long handle, float ptsMs, long jscallback) {
    if (jscallback) {
        pDecodeCtx->js_decodeCallback = (JSVideoDecodeCallback)jscallback;
        long ret = ffcpp_decode_frame(handle, ptsMs);
        return ret;
    } else {
        return -1;
    }
}

int ffwasm_hold_seek(long handle, bool seek) {
    return ffcpp_hold_seek(handle, seek);
}

int ffwasm_seek_frame(long handle, float ptsMs, long jscallback) {
    if (jscallback) {
        pDecodeCtx->js_decodeCallback = (JSVideoDecodeCallback)jscallback;
        return ffcpp_seek_frame(handle, ptsMs);
    } else {
        return -1;
    }
}

long ffwasm_decode_free(long handle) {
    return ffcpp_decode_free(handle);
}

#ifdef __cplusplus
 }
#endif

int main(int argc, const char * argv[]) {

    set_log_level(16);
    outFile = fopen("/Users/jason/Jason/mogic/ffwasm/res/1920_1080.yuv", "wb");
    size_t length = 0;
    const char *inPath = "/Users/jason/Jason/mogic/ffwasm/video/src/assets/hevc.mp4";
    uint8_t *data = read_file_memory(inPath, &length);

    long handle = ffwasm_decode_open(data, length, (long)JS_VideoInfoFunc);
    
    float pts = 0;
    while (pts <= _durationMs) {
        long ret = ffwasm_decode_frame(handle, pts, (long)JS_VideoDecodeFunc);
        ff_log("index %f", pts);
        if (ret < 0) {
            ff_log("decode error");
            break;
        }
        pts += 30;
    }
    ffwasm_decode_free(handle);
    
    fclose(outFile);
    free(data);
    printf("Hello, World!\n");
    return 0;
}

//brew info ffmpeg
//https://zhuanlan.zhihu.com/p/509741316
