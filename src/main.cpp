#include <stdio.h>
#include <stdlib.h>
#include "ffdecode.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

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

typedef struct JSDecodeContext {
    JSVideoDecodeCallback js_decodeCallback;
    JSVideoInfoCallback js_infoCallback;
} JSDecodeContext;

static JSDecodeContext *pDecodeCtx = NULL;

static void InitCallbackFunc(VideoInfo info) {
    if (pDecodeCtx != NULL && pDecodeCtx->js_infoCallback != NULL) {
        pDecodeCtx->js_infoCallback(info.width, info.height, info.durationMs, info.fps);
    }
}

static void DecodeCallbackFunc(AVFrame *frame, float ptsMs) {
    if (pDecodeCtx != NULL && pDecodeCtx->js_decodeCallback != NULL && frame != NULL) {
        pDecodeCtx->js_decodeCallback(frame->data[0],
                                      frame->data[1],
                                      frame->data[2],
                                      frame->linesize[0],
                                      frame->linesize[1],
                                      frame->linesize[2],
                                      frame->width,
                                      frame->height,
                                      (long)ptsMs);
    }
}

long ffwasm_decode_open(uint8_t *data, size_t length, long jsInfoCallback) {
    if (data == NULL || length == 0 || jsInfoCallback == 0) {
        printf("ffwasm_decode_open invalid arguments\n");
        return -1;
    }

    if (pDecodeCtx != NULL) {
        free(pDecodeCtx);
        pDecodeCtx = NULL;
    }

    pDecodeCtx = (JSDecodeContext *)malloc(sizeof(JSDecodeContext));
    if (!pDecodeCtx) {
        return -1;
    }

    pDecodeCtx->js_infoCallback = (JSVideoInfoCallback)jsInfoCallback;
    pDecodeCtx->js_decodeCallback = NULL;
    return ffcpp_decode_init(data, length, 1, NULL, InitCallbackFunc, DecodeCallbackFunc);
}

long ffwasm_decode_frame(long handle, float ptsMs, long jsDecodeCallback) {
    if (handle == 0 || jsDecodeCallback == 0 || pDecodeCtx == NULL) {
        return -1;
    }
    pDecodeCtx->js_decodeCallback = (JSVideoDecodeCallback)jsDecodeCallback;
    AVFrame *frame = NULL;
    return ffcpp_decode_frame(handle, ptsMs, &frame);
}

int ffwasm_hold_seek(long handle, bool seek) {
    if (handle == 0) {
        return -1;
    }
    return ffcpp_hold_seek(handle, seek);
}

int ffwasm_seek_frame(long handle, float ptsMs, long jsDecodeCallback) {
    if (handle == 0 || jsDecodeCallback == 0 || pDecodeCtx == NULL) {
        return -1;
    }
    pDecodeCtx->js_decodeCallback = (JSVideoDecodeCallback)jsDecodeCallback;
    AVFrame *frame = NULL;
    return ffcpp_seek_frame(handle, ptsMs, &frame);
}

long ffwasm_decode_free(long handle) {
    if (pDecodeCtx != NULL) {
        free(pDecodeCtx);
        pDecodeCtx = NULL;
    }
    if (handle == 0) {
        return 0;
    }
    return ffcpp_decode_free(handle);
}

#ifdef __cplusplus
}
#endif

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 0;
}
