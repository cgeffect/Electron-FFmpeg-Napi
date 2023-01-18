
#ifndef ff_decode_h
#define ff_decode_h

//#define __FFDEBUG__
#ifdef __FFDEBUG__
#define ff_log(format,...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define ff_log(format,...)
#endif

#ifdef __cplusplus
extern "C" {
#endif
 
#include <stdio.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <stdbool.h>
//#include <pthread.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

typedef struct VideoInfo {
    int width;
    int height;
    int durationMs;
    float fps;
} VideoInfo;

typedef void (*DecodeCallback)(AVFrame *frame, float PtsMs);
typedef void (*InitCallback)(VideoInfo info);

#define AV_LOG_ERROR    16
#define AV_LOG_WARNING  24
#define AV_LOG_INFO     32
#define AV_LOG_VERBOSE  40
#define AV_LOG_DEBUG    48

void set_log_level(int level);

// pix_fmt 1 = yuv, 2 = rgba
long ffcpp_decode_init(uint8_t *heapData, size_t file_len, int pix_fmt, const char *outputFile, InitCallback initcb, DecodeCallback cb);

int ffcpp_decode_frame(long handle, float pts, AVFrame **outFrame);

int ffcpp_hold_seek(long handle, bool seek);

int ffcpp_seek_frame(long handle, float ptsMs, AVFrame **outFrame);

int ffcpp_decode_free(long handle);

int _av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile);

#ifdef __cplusplus
}
#endif

//#include <thread>
#include <iostream>
#include <atomic>
//#include <deque>
//#include <condition_variable>
//#include <future>

namespace ffwasm
{

typedef struct FFBufferData {
    uint8_t *ptr;     // 指向buffer数据中 "还没被io上下文消耗的位置"
    uint8_t *ori_ptr; // 也是指向buffer数据的指针,之所以定义ori_ptr,是用在自定义seek函数中
    size_t has_size;      // 视频buffer还没被消耗部分的大小,随着不断消耗,越来越小
    size_t file_size; // 原始视频buffer的大小,也是用在自定义seek函数中
} FFBufferData;

typedef struct FFCodecContext {
    AVFormatContext *fmt_ctx;
    AVIOContext *avio_ctx;
    uint8_t *avio_buffer;
    AVCodecContext *avcodec_context;
    AVPacket *avpacket;
    AVFrame *srcFrame;
    int avio_ctx_buffer_size;
    
    AVFrame *swsFrame;
    struct SwsContext *swsContext;
    int video_stream_index;
    AVStream *video_stream;
    float *keyFrameList;
    int keyFrameCount;
    bool packet_eof;
    bool decode_eof;
    float frameRate;
    bool error_exit;
    
} FFCodecContext;

enum FFStrategyState {
    STRATEGY_NONE = 0,
    STRATEGY_ACCELERATE_FORWARD = 1, // 向前加速解码
    STRATEGY_STOP = 2,               // 停止解码
    STRATEGY_SEEK_FORWARD = 3,       // 正向seek
    STRATEGY_SEEK_BACKWARD = 4,      // 逆向seek
    STRATEGY_ACCELERATE_BACKWARD = 6 // 向后加速解码
};

typedef struct FFVideoState {
    float video_consume_pts;       // 消费的pts
    float video_prev_consume_pts;  // 实际解码的pts
    float video_decode_frame_pts;  // 实际解码的pts
    int        seek_req;           // 标识一次seek请求
    int        seek_flags;         // seek标志，诸如AVSEEK_FLAG_BYTE等
    int64_t        seek_pos;       // 请求seek的目标位置(当前位置+增量)
    int64_t        seek_rel;       // 本次seek的位置增量, >0 向前seek, <0向后seek
    enum FFStrategyState strategy;
    
    float timeThreshold;           // 解码落后阈值，按一帧时间算
    float seekThreshold;           // seek 阈值

    FFCodecContext *ioCodecCtx;

    DecodeCallback decodecb;
    InitCallback initcb;

    bool running;
    
    enum AVPixelFormat pixelFormat;
    bool seek = false;
} FFVideoState;

class ffdecode
{
private:
    FFVideoState *videoState = NULL;
    FFBufferData *ioBuffer = NULL;
    std::atomic<bool> task_stop = false;
    std::atomic<bool> thread_stop = false;
    std::atomic<int32_t> consume_pts = -1;
//    std::mutex locker = {};
//    std::condition_variable noEmpty = {};

public:
    ffdecode(/* args */);
    ~ffdecode();
    
    int ff_decode_init(uint8_t *heapData, size_t file_len, int pix_fmt, const char *outputFile, InitCallback initcb, DecodeCallback cb);

    int ff_decode_frame(float pts, AVFrame **outFrame);

    int ff_hold_seek(bool seek);

    int ff_seek_frame(float ptsMs, AVFrame **outFrame);

    int ff_decode_free(long handle);
    
    int av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile);
};

} // namespace ffwasm

#endif /* ff_decode_h */
