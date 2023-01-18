
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
     

void JS_VideoDecodeCallback(unsigned char *y,
                         unsigned char *u,
                         unsigned char *v,
                         int line1,
                         int line2,
                         int line3,
                         int width,
                         int height,
                         long pts) {
#ifdef __APPLE__
//        enum AVPixelFormat pixelForamt = (AVPixelFormat)frame->format;
     int y_size = width * height;
     int u_size = y_size / 4;
     int v_size = y_size / 4;

     fwrite(y, 1, y_size, outFile);//Y
     fwrite(u, 1, u_size ,outFile);//U:宽高均是Y的一半
     fwrite(v, 1, v_size, outFile);//V:宽高均是Y的一半
 
//    fwrite(frame->data[0], 1, frame->width * frame->height * 4, outFile);
#else
#endif

}

void JS_VideoInfoCallback(int width,
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
     printf("%d %d %d %f\n", info.width, info.height, info.durationMs, info.fps);
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

    printf("data = %s, length = %zu, callback = %ld\n", data, length, jscallback);
    
    pDecodeCtx = (JSDecodeContext *)malloc(sizeof(JSDecodeContext));
    if (!pDecodeCtx) {
        return -1;
    }
    pDecodeCtx->js_infoCallback = NULL;
    pDecodeCtx->js_decodeCallback = NULL;
    
    if (jscallback) {
        pDecodeCtx->js_infoCallback = (JSVideoInfoCallback)jscallback;
        long ret = ffcpp_decode_init(data, length, 1, "/Users/jason/Jason/mogic/ffwasm/res/1920_1080.yuv", InitCallbackFunc, DecodeCallbackFunc);
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
        AVFrame *frame = NULL;
        long ret = ffcpp_decode_frame(handle, ptsMs, &frame);
    //    if (frame != NULL) {
    //        DecodeCallbackFunc(frame, ptsMs);
    //    }

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
        AVFrame *frame = NULL;
        return ffcpp_seek_frame(handle, ptsMs, &frame);
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

#include <chrono>
#include <thread>

using namespace std::chrono;
#define TEST1
int main(int argc, const char * argv[]) {

//    for (int i =0; i < 5; i++) {
//        int temp = i;
//        std::this_thread::sleep_for(std::chrono::seconds(1));
//        std::thread([](int value){
//            std::thread::id tid = std::this_thread::get_id();
//            std::cout << " tid=" << tid<< " " << value << std::endl;
//        }, temp).detach();
//
//    }

    set_log_level(16);
    outFile = fopen("/Users/jason/Jason/mogic/ffwasm/res/1920_1080.yuv", "wb");
    size_t length = 0;
    const char *path = "/Users/jason/Jason/mogic/ffwasm/res/test.mp4";
    uint8_t *data = read_file_memory(path, &length);

#ifdef TEST
    _av_io_decode_test(data, length, "/Users/jason/Jason/mogic/ffwasm/res/1920_1080.yuv");
#else
    long handle = ffwasm_decode_open(data, length, (long)JS_VideoInfoCallback);
    
    float pts = 0;
    while (pts <= _durationMs) {
        long ret = ffwasm_decode_frame(handle, pts, (long)JS_VideoDecodeCallback);
        ff_log("index %f\n", pts);
        if (ret < 0) {
            ff_log("decode error\n");
            break;
        }
        pts += 30;
//        usleep(30 * 1000);
    }
    
    
//        time_point<system_clock> start1 = system_clock::now();
//
//        struct timespec begin, end;
//        clock_gettime(CLOCK_REALTIME, &begin);
//        usleep(30 * 1000);
//
//        clock_gettime(CLOCK_REALTIME, &end);
//        long seconds = end.tv_sec - begin.tv_sec;
//        long nanoseconds = end.tv_nsec - begin.tv_nsec;
//        double elapsed = seconds + nanoseconds*1e-9;
//        printf("cost Result Time measured: %.3f ms.\n", elapsed * 1000);
//
//        time_point<system_clock> end1 = system_clock::now();
//        std::chrono::duration<double> elapsed1 = end1 - start1;
//        std::cout << "cost time: " << elapsed1.count() * 1000 << "\n";
    
//    sleep(10);

    ffwasm_decode_free(handle);
    
    fclose(outFile);
    
    
#endif
    free(data);
    printf("%s\n", "hello world");
    
    // insert code here...
    printf("Hello, World!\n");
    return 0;
}

//brew info ffmpeg
//https://zhuanlan.zhihu.com/p/509741316
