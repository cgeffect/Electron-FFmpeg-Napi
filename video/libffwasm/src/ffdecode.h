
#ifndef ff_decode_h
#define ff_decode_h

#ifdef __APPLE__
#define __FFWASM__
#else
#endif

#ifdef __FFWASM__
#define ff_log(format,...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define ff_log(format,...)
#endif

#define AV_LOG_ERROR    16
#define AV_LOG_WARNING  24
#define AV_LOG_INFO     32
#define AV_LOG_VERBOSE  40
#define AV_LOG_DEBUG    48

#pragma mark - c call c++ interface

#ifdef __cplusplus
extern "C" {
#endif
 
#include <stdio.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <stdbool.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

enum FF_PIX_FMT {
    FF_PIX_FMT_I420 = 1,
    FF_PIX_FMT_RGBA
};
typedef struct VideoInfo {
    int width;
    int height;
    float durationMs;
    float fps;
} VideoInfo;

typedef void (*DecodeCallback)(AVFrame *frame, float PtsMs);
typedef void (*InitCallback)(VideoInfo info);

void set_log_level(int level);

// pix_fmt 1 = yuv, 2 = rgba
long ffcpp_decode_init(uint8_t *heapData, size_t file_len, FF_PIX_FMT pix_fmt, InitCallback initcb, DecodeCallback cb);

int ffcpp_set_param(long handle, int rotate);

int ffcpp_decode_frame(long handle, float pts);

int ffcpp_hold_seek(long handle, bool seek);

int ffcpp_seek_frame(long handle, float ptsMs);

int ffcpp_decode_free(long handle);

int _av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile);

#ifdef __cplusplus
}
#endif

#pragma mark - cpp
//https://dawnarc.com/2019/07/c-error-templates-must-have-c-linkage/
#ifdef __cplusplus
extern "C++" {
#endif

#include <thread>
#include <iostream>
#include <atomic>
#include <future>
#include <vector>
#include <memory>
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
    int dstWidth, dstHeight;
    AVPacket *avpacket;
    int avio_ctx_buffer_size;
    
    AVFrame *swsFrame;

    struct SwsContext *sws_context;
    int video_stream_index;
    AVStream *video_stream;
    float *keyFrameList;
    int keyFrameCount;
    bool packet_eof;
    bool decode_eof;
    float frame_rate;
    bool error_exit;
    float durationMs;
    
    //旋转
    int rotate;
    AVFrame *rgbaFrame = nullptr;
    AVFrame *rotateFrame = nullptr;
    SwsContext *rgbaContext = nullptr;

    AVFrame *srcFrame = nullptr;
    AVFrame *outFrameP = nullptr;

    
    bool mPacketIsHadSpsPps;
    // AVBitStreamFilterContext *mBitFilterContext{nullptr};
    bool mIsStreamNotSupport;
} FFCodecContext;

enum FFStrategyState {
    STRATEGY_NONE = 0,
    STRATEGY_ACCELERATE_FORWARD = 1, // 向前加速解码
    STRATEGY_STOP = 2,               // 停止解码
    STRATEGY_SEEK_FORWARD = 3,       // 正向seek
    STRATEGY_SEEK_BACKWARD = 4,      // 逆向seek
    STRATEGY_ACCELERATE_BACKWARD = 6 // 向后加速解码
};

enum FF_DECODE_EVENT {
    DECODE_EVENT_NONE = 0,
    DECODE_EVENT_INIT,
    DECODE_EVENT_DECODE,
    DECODE_EVENT_WILL_SEEK,
    DECODE_EVENT_SEEK,
    DECODE_EVENT_DID_SEEK,
    DECODE_EVENT_STOP,
};

typedef struct FFVideoState {
    float video_consume_pts;       // 消费的pts
    float video_prev_consume_pts;  // 实际解码的pts
    float video_decode_frame_pts;  // 实际解码的pts
    enum FFStrategyState strategy;
    
    float threshold;           // 解码落后阈值，按一帧时间算
    float seek_threshold;           // seek 阈值

    FFCodecContext *ioCodecCtx;

    DecodeCallback decodecb;
    InitCallback initcb;

    bool running;
    
    enum AVPixelFormat pixelFormat;
    bool seeking = false;
    
    FF_DECODE_EVENT event;
    
} FFVideoState;

class ffdecode
{
private:
    FFVideoState *videoState = NULL;
    FFBufferData *ioBuffer = NULL;
    std::atomic<bool> task_stop = false;
    std::atomic<bool> thread_stop = false;
//    std::atomic<int32_t> consume_pts = -1;
    
    std::shared_ptr<std::future<int>> future;
//    std::future<int> future;
//    AVFrame *getFrontBuffer();
//    AVFrame *getBackBuffer();
//    void swapBuffer();
//    std::atomic<int> _currentBackBufferIndex;
//    std::atomic<bool> _hasBuffer = false;
//    std::vector<AVFrame *> bufferList;
//    bool _hasDestroy;
    
    int _ff_receive_video_frame(FFCodecContext *ioCodecCtx, AVFrame *srcFrame, AVFrame **dstFrame);
    int _ff_send_video_packet(FFCodecContext *ioCodecCtx, float consume_pts, bool ff_decode_event);
    int _ff_swsscale_frame(FFVideoState *videoState, AVFrame *srcFrame, AVFrame **dstFrame);

    bool enable_rotate = false;
    bool mPacketIsHadSpsPps = false;
    
public:
    ffdecode(/* args */);
    ~ffdecode();
    
    int ff_decode_init(uint8_t *heapData, size_t file_len, FF_PIX_FMT pix_fmt, InitCallback initcb, DecodeCallback cb);

    int ff_set_param(int rotate);
    
    int ff_decode_frame(float pts);

    int ff_hold_seek(bool seek);

    int ff_seek_frame(float ptsMs);

    int ff_decode_free(long handle);
    
    int av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile);
    
    int ff_decode_frame_unit(FFVideoState *videoState, float consume_pts, AVFrame **outFrame);

};

} // namespace ffwasm

#ifdef __cplusplus
}
#endif

#endif /* ff_decode_h */
