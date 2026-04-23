
/**
    ffmpeg 5.0 wasm decode.c
 */
#include "ffdecode.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdbool.h>
#include <libavutil/log.h>
#include <pthread.h>
#include <libavformat/avio.h>
#include <libyuv.h>
#ifdef __cplusplus
}
#endif

#include <thread>
#include <future>
#include <unistd.h>
#include "ffrotate.h"

#pragma mark - c/c++ interface
void set_log_level(int level) {
    av_log_set_level(level);
}
long ffcpp_decode_init(uint8_t *heapData, size_t file_len, FF_PIX_FMT pix_fmt, InitCallback initcb, DecodeCallback cb) {
    ffwasm::ffdecode *decode = new ffwasm::ffdecode();
    decode->ff_decode_init(heapData, file_len, pix_fmt, initcb, cb);
    return (long)decode;
}

int ffcpp_set_param(long handle, int rotate) {
    ffwasm::ffdecode *decode = new ffwasm::ffdecode();
    decode->ff_set_param(rotate);
    return 0;
}

int ffcpp_decode_frame(long handle, float pts) {
    ffwasm::ffdecode *decode = (ffwasm::ffdecode *)handle;
    return decode->ff_decode_frame(pts);
}

int ffcpp_hold_seek(long handle, bool seek) {
    ffwasm::ffdecode *decode = (ffwasm::ffdecode *)handle;
    return decode->ff_hold_seek(seek);
}

int ffcpp_seek_frame(long handle, float ptsMs) {
    ffwasm::ffdecode *decode = (ffwasm::ffdecode *)handle;
    return decode->ff_seek_frame(ptsMs);
}

int ffcpp_decode_free(long handle) {
    ffwasm::ffdecode *decode = (ffwasm::ffdecode *)handle;
    decode->ff_decode_free(handle);
    delete decode;
    return 0;
}

int _av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile) {
    return 0;
}

#pragma mark - decode impl

#define MIN(a, b) (a < b) ? a : b
#define KEY_FRAME_COUNT 1000
#define FF_DECODE_OK 0
#define AVIO_BUFFER_SIZE 4096
#define VIDEO_PICTURE_QUEUE_SIZE    3       // 图像帧缓存数量

static unsigned sws_flags = SWS_BICUBIC;

namespace ffwasm
{

#pragma mark - callback
/* 读取 AVPacket 回调函数 */
static int read_packet_ptr(void *opaque, uint8_t *buf, int buf_size)
{
    FFBufferData *bd = (FFBufferData *)opaque;
    buf_size = MIN((int)bd->has_size, buf_size);
    
    if (buf == NULL) {
        av_log(NULL, AV_LOG_ERROR, "no buf pass to read_packet has_size %d,%zu\n", buf_size, bd->has_size);
        return EAGAIN;
    }
    if (buf_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "1no buf_size pass to read_packet has_size %d,%zu\n", buf_size, bd->has_size);
        return EAGAIN;
    }
//    float a = SIZE_MAX;
//    printf("ptr in file:%p io.buffer ptr:%p, has_size:%zu,buf_size:%d\n", bd->ptr, buf, bd->has_size, buf_size);
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr += buf_size;
    bd->has_size -= buf_size; // left size in buffer
//    printf("after ptr in file:%p io.buffer ptr:%p, has_size:%zu,buf_size:%d\n", bd->ptr, buf, bd->has_size, buf_size);
    return buf_size;
    
}

/* seek 回调函数 */
static int64_t seek_in_buffer_ptr(void *opaque, int64_t offset, int whence)
{
    FFBufferData *bd = (FFBufferData *)opaque;
    int64_t ret = -1;

//    printf("whence=%d , offset=%lld , file_size=%zu\n", whence, offset, bd->file_size);
    switch (whence) {
    case AVSEEK_SIZE:
        ret = bd->file_size;
        break;
    case SEEK_SET:
        bd->ptr = bd->ori_ptr + offset;
        bd->has_size = bd->file_size - offset;
        ret = (int64_t)bd->ptr;
        break;
        default:
            av_log(NULL, AV_LOG_ERROR, "offset %lld, whence %d\n", offset, whence);
    }
    return ret;
}

#pragma mark - util
FILE * outfile;

static float get_videostream_durationMs(FFCodecContext *ioCodecCtx) {
    AVStream *videoStream = ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]; // 音频流、视频流、字幕流
    return videoStream->duration * av_q2d(videoStream->time_base) * 1000;
}

static int _flush_decode_buffer(FFCodecContext *ioCodecCtx) {
    avcodec_flush_buffers(ioCodecCtx->avcodec_context);

    while (true) {
        int ret = avcodec_receive_frame(ioCodecCtx->avcodec_context, ioCodecCtx->srcFrame);
        if (ret != 0) {
            break;
        }
    }
    return 0;
}
/*
AVSEEK_FLAG_BACKWARD 1 ///< seek backward seek到timestamp之前的最近关键帧
AVSEEK_FLAG_BYTE 2 ///< seeking based on position in bytes 基于字节位置的跳转
AVSEEK_FLAG_ANY 4 ///< seek to any frame, even non-keyframes 跳转到任意帧，不一定是关键帧
AVSEEK_FLAG_FRAME 8 ///< seeking based on frame number 基于帧数量的跳转
 */
static int _ff_stream_seek(FFCodecContext *ioCodecCtx, float seekMs) {
    AVFormatContext *avFormatContext = ioCodecCtx->fmt_ctx;
    if (seekMs >= ioCodecCtx->durationMs) {
        av_log(NULL, AV_LOG_ERROR, "seekToMs seek time error, seek time out of file\n");
        return -1;
    }

    if (ioCodecCtx->video_stream_index != -1) {
        // 现实时间 -> 时间戳
        AVRational timeBase = avFormatContext->streams[ioCodecCtx->video_stream_index]->time_base;
        float seekSeconds_timestamp = seekMs / 1000 * AV_TIME_BASE;
        int64_t video_seek_timestamp = av_rescale_q((int64_t)seekSeconds_timestamp, AV_TIME_BASE_Q, timeBase);
        int ret = av_seek_frame(avFormatContext, ioCodecCtx->video_stream_index, video_seek_timestamp, AVSEEK_FLAG_BACKWARD);
        if (ret != 0) {
            av_log(NULL, AV_LOG_ERROR, "ERROR: seekToMs av_seek_frame error msg: %s\n", av_err2str(ret));
            return ret;
        } else {
            ff_log("seek to: %f, total duration: %f", seekMs, ioCodecCtx->durationMs);
        }
    }
    _flush_decode_buffer(ioCodecCtx);
    return 0;
}

static int _ff_parser_keyframes(FFCodecContext *ioCodecCtx) {
    int keyframeCount = KEY_FRAME_COUNT;
    ioCodecCtx->keyFrameList = (float *)malloc(keyframeCount * sizeof(float));
    AVPacket packet;
    AVFormatContext *avFormatCtx = ioCodecCtx->fmt_ctx;
    int video_stream_index = ioCodecCtx->video_stream_index;
    int frameIndex = 0;
    int index = 0;
    while (true) {
        int ret = av_read_frame(avFormatCtx, &packet);
        if (ret != 0) {
            break;
        }
        if (packet.stream_index == ioCodecCtx->video_stream_index) {
            bool isKeyFrame = packet.flags & AV_PKT_FLAG_KEY;
            int64_t pts = packet.pts;
            float ptsMs = packet.pts * av_q2d(avFormatCtx->streams[video_stream_index]->time_base) * 1000;
            if (isKeyFrame) {
                ff_log("key frame %d, index: %lld, ptsMs: %f", frameIndex, pts, ptsMs);
                ioCodecCtx->keyFrameList[index] = ptsMs;
                index++;
                if (index > keyframeCount) {
                    av_log(NULL, AV_LOG_DEBUG, "decode error: keyframeCount > %d", index);
                }
            }
            frameIndex++;
        }
        if (ioCodecCtx->video_stream_index != ioCodecCtx->avpacket->stream_index) {
            av_packet_unref(&packet);
            continue;
        }
    }
    ioCodecCtx->keyFrameCount = index;
    ff_log("keyframe count = %d, totalframe count = %d, duration = %f\n", index, frameIndex, ioCodecCtx->durationMs);
    _ff_stream_seek(ioCodecCtx, 0);
    return 0;
}

// 找当前pts依赖的I帧
static float _ff_depend_keyframe(FFCodecContext *ioCodecCtx, float newPts) {
    float newKey = -1;
    int maxIndex = (int) (ioCodecCtx->keyFrameCount - 1);
    for (int i = maxIndex; i >= 0; i--) {
        float obj = ioCodecCtx->keyFrameList[i];
        if (obj <= newPts) {
            newKey = obj;
            break;
        }
    }
    return newKey;
}

static bool _ff_is_same_gop(FFCodecContext *ioCodecCtx, float currentPts, float prevPts) {
    float currentKey = _ff_depend_keyframe(ioCodecCtx, currentPts);
    float prevKey = _ff_depend_keyframe(ioCodecCtx, prevPts);
    return currentPts >= prevPts && currentKey == prevKey;
}

int _ff_yuv_rotate(int srcRotate, AVFrame *srcFrame, AVFrame *rotateFrame) {
    if (fabs(srcRotate - 90) < 1.0) {
        ffrotate::frameRotate90(srcFrame, rotateFrame);
    } else if (fabs(srcRotate - 180) < 1.0 || fabs(srcRotate + 180) < 1.0) {
        ffrotate::frameRotate180(srcFrame, rotateFrame);
    } else if (fabs(srcRotate - 270) < 1.0 || fabs(srcRotate + 90) < 1.0) {
        ffrotate::frameRotate270(srcFrame, rotateFrame);
    } else {
        // 0 degreee do nothing
    }
    return 0;
}

#pragma mark - decode thread

void decode_event_thread(void *ctx) {

    while (true) {
        ffdecode *videoState = (ffdecode *)ctx;
//        FFVideoState *videoState = ctx->vid
    }
}

#pragma mark - interface

ffdecode::ffdecode() {
    
}

int ffdecode::ff_decode_init(uint8_t *heapData, size_t file_len, FF_PIX_FMT pix_fmt, InitCallback initcb, DecodeCallback cb) {
    if (heapData == nullptr || file_len <= 0) {
        av_log(NULL, AV_LOG_ERROR, "data or file length is zero\n");
        return -1;
    }
    
    int ret = 0;

    FFCodecContext *ioCodecCtx = (FFCodecContext *)av_mallocz(sizeof(FFCodecContext));
    if (ioCodecCtx == nullptr) {
        av_log(NULL, AV_LOG_ERROR, "malloc FFCodecContext fail\n");
        return -1;
    }
    this->videoState = (FFVideoState *)av_mallocz(sizeof(FFVideoState));
    ioBuffer = (FFBufferData *)av_mallocz(sizeof(FFBufferData));
    videoState->ioCodecCtx = ioCodecCtx;
    videoState->decodecb = cb;
    videoState->initcb = initcb;
    videoState->video_consume_pts = -1;       // 消费的pts
    videoState->video_prev_consume_pts = -1;  // 实际解码的pts
    videoState->video_decode_frame_pts = -1;  // 实际解码的pts

    
    if (pix_fmt == FF_PIX_FMT_I420) {
        videoState->pixelFormat = AV_PIX_FMT_YUV420P;
    } else if (pix_fmt == FF_PIX_FMT_RGBA) {
        videoState->pixelFormat = AV_PIX_FMT_RGBA;
    } else {
        av_log(NULL, AV_LOG_ERROR, "no surrort pixel format %d\n", pix_fmt);
        return -1;
    }

    av_log_set_level(AV_LOG_DEBUG);
    ioCodecCtx->avio_ctx_buffer_size = AVIO_BUFFER_SIZE;

    ioBuffer->ptr = heapData;
    ioBuffer->ori_ptr = heapData;
    ioBuffer->has_size = file_len;
    ioBuffer->file_size = file_len;
    ff_log("file size %zu", file_len);
    // 打开输入文件
    ioCodecCtx->fmt_ctx = avformat_alloc_context();
    if (!ioCodecCtx->fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }

    ioCodecCtx->avio_buffer = (uint8_t *)av_malloc(ioCodecCtx->avio_ctx_buffer_size);
    if (ioCodecCtx->avio_buffer == NULL) {
        av_log(NULL, AV_LOG_ERROR, "avio_ctx_buffer is NULL\n");
        return -1;
    }

    if (!ioCodecCtx->avio_buffer) {
        av_log(NULL, AV_LOG_ERROR, "error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    ioCodecCtx->avio_ctx = avio_alloc_context(ioCodecCtx->avio_buffer, ioCodecCtx->avio_ctx_buffer_size, 0, ioBuffer, &read_packet_ptr, NULL, &seek_in_buffer_ptr);
    if (!ioCodecCtx->avio_ctx) {
        av_log(NULL, AV_LOG_ERROR, "error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    ioCodecCtx->fmt_ctx->pb = ioCodecCtx->avio_ctx;
    if ((ret = avformat_open_input(&ioCodecCtx->fmt_ctx, NULL, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "can not open file %d %s \n", ret, av_err2str(ret));
        return ret;
    }

    ret = avformat_find_stream_info(ioCodecCtx->fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_find_stream_info file %d \n", ret);
        return ret;
    }

    ioCodecCtx->avcodec_context = avcodec_alloc_context3(NULL);
    ioCodecCtx->video_stream_index = av_find_best_stream(ioCodecCtx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ret = avcodec_parameters_to_context(ioCodecCtx->avcodec_context, ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codecpar);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "error code %d \n", ret);
        return ret;
    }
    const AVCodec *codec = avcodec_find_decoder(ioCodecCtx->avcodec_context->codec_id);
    if ((ret = avcodec_open2(ioCodecCtx->avcodec_context, codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open codec faile %d \n", ret);
        return ret;
    }

    float durationMs = get_videostream_durationMs(videoState->ioCodecCtx);
    ioCodecCtx->durationMs = durationMs;
    
    ioCodecCtx->avpacket = av_packet_alloc();
    ioCodecCtx->srcFrame = av_frame_alloc();
//    bufferList.push_back(av_frame_alloc());
//    bufferList.push_back(av_frame_alloc());
    
    {
        //yuv420p -> yuv420p 裁剪
        enum AVPixelFormat srcPixFmt = ioCodecCtx->avcodec_context->pix_fmt;
        enum AVPixelFormat dstPixFmt = ioCodecCtx->avcodec_context->pix_fmt;
        int srcWidth = ioCodecCtx->avcodec_context->width;
        int srcHeight = ioCodecCtx->avcodec_context->height;
        int dstWidth = ioCodecCtx->avcodec_context->width;
        int dstHeight = ioCodecCtx->avcodec_context->height;

        //裁剪
        ioCodecCtx->swsFrame = av_frame_alloc();
        ioCodecCtx->swsFrame->format = dstPixFmt;
        ioCodecCtx->swsFrame->width = dstWidth;
        ioCodecCtx->swsFrame->height = dstHeight;
        
        int memsize = av_image_alloc(ioCodecCtx->swsFrame->data, ioCodecCtx->swsFrame->linesize, dstWidth, dstHeight, dstPixFmt, 1);
        if (memsize <= 0) {
            av_log(NULL, AV_LOG_ERROR, "sws convert fail %d\n", memsize);
            return -1;
        }
        ioCodecCtx->sws_context = sws_getContext(srcWidth, srcHeight, srcPixFmt,
                                                dstWidth, dstHeight, dstPixFmt,
                                                sws_flags, NULL, NULL, NULL);
        ff_log("width = %d, height = %d", ioCodecCtx->avcodec_context->width, ioCodecCtx->avcodec_context->height);
        
        //旋转
        ioCodecCtx->rotateFrame = av_frame_alloc();
        ioCodecCtx->rotateFrame->format = ioCodecCtx->avcodec_context->pix_fmt;
        ioCodecCtx->rotateFrame->width = dstWidth;
        ioCodecCtx->rotateFrame->height = dstHeight;
        memsize = av_image_alloc(ioCodecCtx->rotateFrame->data, ioCodecCtx->rotateFrame->linesize, dstWidth, dstHeight, dstPixFmt, 1);
        if (memsize <= 0) {
            av_log(NULL, AV_LOG_ERROR, "sws convert fail %d\n", memsize);
            return -1;
        }
    }
        
    //开启多线程解码
    ioCodecCtx->avcodec_context->thread_count = 8;
    //丢弃哪些帧不解码
//    ioCodecCtx->avcodec_context->skip_frame = AVDISCARD_DEFAULT;
//    ioCodecCtx->avcodec_context->skip_loop_filter = AVDISCARD_DEFAULT;
//    ioCodecCtx->avcodec_context->skip_idct = AVDISCARD_DEFAULT;

    //丢弃哪些帧不解码
    AVStream *video_stream = ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index];
    ioCodecCtx->video_stream = video_stream;
//    video_stream->discard = AVDISCARD_NONREF;
    int srcRotate = enable_rotate ? ffrotate::getRotateAngle(video_stream) : 0;
    ff_log("enable_rotate %d, rotate: %d", enable_rotate, srcRotate);
    ioCodecCtx->rotate = srcRotate;
    // 0 水平 不需要转
    // 90 270 交换宽高，转成竖的
    // 180 水平反向，宽高不用交换，但需要转正
    if (srcRotate == 90 || srcRotate == 270 || srcRotate == 180) {
        if (fabs(srcRotate - 90) < 1.0) {
            int temp = ioCodecCtx->avcodec_context->width;
            ioCodecCtx->dstWidth = ioCodecCtx->avcodec_context->height;
            ioCodecCtx->dstHeight = temp;
        } else if (fabs(srcRotate - 180) < 1.0 || fabs(srcRotate + 180) < 1.0) {
            ioCodecCtx->dstWidth = ioCodecCtx->avcodec_context->width;
            ioCodecCtx->dstHeight = ioCodecCtx->avcodec_context->height;
        } else if (fabs(srcRotate - 270) < 1.0 || fabs(srcRotate + 90) < 1.0) {
            int temp = ioCodecCtx->avcodec_context->width;
            ioCodecCtx->dstWidth = ioCodecCtx->avcodec_context->height;
            ioCodecCtx->dstHeight = temp;
        } else {
            // 0 degreee do nothing
        }
    } else {
        ioCodecCtx->dstWidth = ioCodecCtx->avcodec_context->width;
        ioCodecCtx->dstHeight = ioCodecCtx->avcodec_context->height;
    }
        
    {
        //yuv420p -> rgba
        enum AVPixelFormat srcPixFmt = ioCodecCtx->avcodec_context->pix_fmt;
        enum AVPixelFormat dstPixFmt = videoState->pixelFormat;
        int srcWidth = ioCodecCtx->dstWidth;
        int srcHeight = ioCodecCtx->dstHeight;
        int dstWidth = ioCodecCtx->dstWidth;
        int dstHeight = ioCodecCtx->dstHeight;

        ioCodecCtx->rgbaFrame = av_frame_alloc();
        ioCodecCtx->rgbaFrame->format = dstPixFmt;
        float memsize = av_image_alloc(ioCodecCtx->rgbaFrame->data, ioCodecCtx->rgbaFrame->linesize, dstWidth, dstHeight, dstPixFmt, 1);
        if (memsize <= 0) {
            av_log(NULL, AV_LOG_ERROR, "sws convert fail %f\n", memsize);
            return -1;
        }
        ioCodecCtx->rgbaFrame->width = dstWidth;
        ioCodecCtx->rgbaFrame->height = dstHeight;
        ioCodecCtx->rgbaContext = sws_getContext(srcWidth, srcHeight, srcPixFmt,
                                                dstWidth, dstHeight, dstPixFmt,
                                                sws_flags, NULL, NULL, NULL);

        ff_log("width = %d, height = %d", ioCodecCtx->avcodec_context->width, ioCodecCtx->avcodec_context->height);
    }
    
    float avg_frame_rate = av_q2d(ioCodecCtx->video_stream->avg_frame_rate);
//    float r_frame_rate = av_q2d(ioCodecCtx->video_stream->r_frame_rate);

    ioCodecCtx->frame_rate = 1000 / avg_frame_rate;

    float max_b = 6;
    videoState->threshold = ceil(ioCodecCtx->frame_rate / 2);
    videoState->seek_threshold = max_b * avg_frame_rate;
    
    //解析关键帧
    _ff_parser_keyframes(ioCodecCtx);
    
    if (this->videoState->initcb) {
        VideoInfo info = {
            ioCodecCtx->avcodec_context->width,
            ioCodecCtx->avcodec_context->height,
            static_cast<float>(ioCodecCtx->durationMs),
            avg_frame_rate
        };
        this->videoState->initcb(info);
    }
    
//    auto future = std::async(std::launch::async, decode_event_thread, this);
//    auto f = std::async([] {
//        std::cout << "任务2-开始\n";
////        this_thread::sleep_for(std::chrono::seconds(2));
//        std::cout << "任务2-结束\n";
//    });
//    this->task_stop = true;
//    std::thread([this]() {
//        while (true) {
//        
//        }
//    }).detach();
//
//    auto getNum = std::async(std::launch::async, decode_event, 2);
//    auto getRet = getNum.get();
//    std::cout << getRet << std::endl; // 4

    videoState->event = DECODE_EVENT_INIT;
    return 0;
}

int ffdecode::ff_set_param(int rotate) {
    if (rotate < 0 || rotate > 360) {
        av_log(NULL, AV_LOG_ERROR, "ff_set_param rotate error %d\n", rotate);
        return -1;
    }
    return 0;
}

int ffdecode::ff_decode_frame(float consume_pts) {
    videoState->event = DECODE_EVENT_DECODE;
    this->task_stop = false;
    AVFrame *outFrame = NULL;
    int ret = ff_decode_frame_unit(videoState, consume_pts, &outFrame);
    if (this->videoState->decodecb && outFrame) {
        if (videoState->ioCodecCtx->rotate > 0 || videoState->pixelFormat != AV_PIX_FMT_YUV420P) {
            AVFrame *dstFrame = nullptr;
            ret = _ff_swsscale_frame(videoState, outFrame, &dstFrame);
            if (ret == 0 && dstFrame) {
                this->videoState->decodecb(dstFrame, consume_pts);
            }
        } else {
            if (ret == 0 && outFrame) {
                this->videoState->decodecb(outFrame, consume_pts);
            }
        }
    }
    return ret;
}

int ffdecode::ff_hold_seek(bool seek) {
    if (seek) {
        videoState->event = DECODE_EVENT_WILL_SEEK;
    } else {
        videoState->event = DECODE_EVENT_DID_SEEK;
    }

    videoState->seeking = seek;
    return 0;
}

int ffdecode::ff_seek_frame(float consume_pts) {
    videoState->event = DECODE_EVENT_SEEK;
    task_stop = true;
    videoState->ioCodecCtx->packet_eof = false;
    videoState->ioCodecCtx->decode_eof = false;
    videoState->ioCodecCtx->error_exit = false;
    videoState->video_decode_frame_pts = -1;
    //取消所有任务
    this->task_stop = false;
    AVFrame *outFrame = NULL;
    int ret = ff_decode_frame_unit(videoState, consume_pts, &outFrame);
    if (this->videoState->decodecb && outFrame) {
        if (videoState->ioCodecCtx->rotate > 0 || videoState->pixelFormat != AV_PIX_FMT_YUV420P) {
            AVFrame *dstFrame = nullptr;
            ret = _ff_swsscale_frame(videoState, outFrame, &dstFrame);
            if (ret == 0 && dstFrame) {
                this->videoState->decodecb(dstFrame, consume_pts);
            }
        } else {
            if (ret == 0 && outFrame) {
                this->videoState->decodecb(outFrame, consume_pts);
            }
        }
    }
    return ret;
}

int ffdecode::ff_decode_free(long handle) {
    videoState->event = DECODE_EVENT_STOP;
    FFCodecContext *ioCodecCtx = videoState->ioCodecCtx;

//    if (ioCodecCtx->avio_buffer) {
//        av_free(ioCodecCtx->avio_buffer);
//    }

    if (ioCodecCtx->avio_ctx) {
//        avio_close(ioCodecCtx->avio_ctx);
        avio_context_free(&ioCodecCtx->avio_ctx);
    }

    if (ioCodecCtx->srcFrame) {
        av_frame_free(&ioCodecCtx->srcFrame);
    }
        
    if (ioCodecCtx->avpacket) {
        av_packet_free(&ioCodecCtx->avpacket);
    }

    if (ioCodecCtx->swsFrame) {
        av_freep(&ioCodecCtx->swsFrame->data[0]);
        av_frame_free(&ioCodecCtx->swsFrame);
    }
    
    if (ioCodecCtx->sws_context) {
        sws_freeContext(ioCodecCtx->sws_context);
    }

    if (ioCodecCtx->rotateFrame) {
        av_frame_free(&ioCodecCtx->rotateFrame);
    }
    if (ioCodecCtx->rgbaContext) {
        sws_freeContext(ioCodecCtx->rgbaContext);
    }
    
    if (ioCodecCtx->avcodec_context) {
        avcodec_close(ioCodecCtx->avcodec_context);
        avcodec_free_context(&ioCodecCtx->avcodec_context);
    }

    if (ioCodecCtx->fmt_ctx) {
//        avformat_close_input(&ioCodecCtx->fmt_ctx);
        avformat_free_context(ioCodecCtx->fmt_ctx);
    }
    
    if (ioCodecCtx) {
        free(ioCodecCtx);
    }
    if (videoState) {
        free(videoState);
    }
    if (ioBuffer) {
        free(ioBuffer);
    }
    
    this->thread_stop = true;
    free(videoState);
    videoState = NULL;

    return 0;
}

//AVFrame * ffdecode::getFrontBuffer() {
//    return _hasBuffer ? bufferList[1 - _currentBackBufferIndex] : nullptr;
//
//}
//AVFrame * ffdecode::getBackBuffer() {
//    return _hasDestroy ? nullptr : bufferList[_currentBackBufferIndex];
//}
//
//void ffdecode::swapBuffer() {
//    _currentBackBufferIndex = 1 - _currentBackBufferIndex;
//    _hasBuffer = true;
//}

int ffdecode::ff_decode_frame_unit(FFVideoState *videoState, float consume_pts, AVFrame **outFrame) {
    if (videoState == NULL || consume_pts < 0 || outFrame == NULL || consume_pts > videoState->ioCodecCtx->durationMs) {
        av_log(NULL, AV_LOG_ERROR, "decode param error\n");
        return -1;
    }
    struct timespec begin, end;
    clock_gettime(CLOCK_REALTIME, &begin);
        
    videoState->running = true;
//    printf("running start\n");
    ff_log("ff_decode_frame_unit consume_pts %f", consume_pts);
    FFCodecContext *ioCodecCtx = videoState->ioCodecCtx;
    int ret = 0;
    if (ioCodecCtx->error_exit) {
        av_log(NULL, AV_LOG_ERROR, "ERROR: decode error exit!\n");
        return 1;
    }
    if (ioCodecCtx->decode_eof) {
        av_log(NULL, AV_LOG_ERROR, "All packet has been decode\n");
        return 1;
    }
    
    if (ioCodecCtx->packet_eof) {
//        av_log(NULL, AV_LOG_ERROR, "all frame has been packet_eof\n");
//        return -1;
    }
    
    videoState->video_consume_pts = consume_pts;
    //先找缓存
    if (videoState->video_decode_frame_pts >= 0 &&
        (videoState->video_decode_frame_pts >= consume_pts || fabsf(videoState->video_decode_frame_pts - consume_pts) < fabsf(videoState->threshold))) {
        ff_log("hit cache consume_pts %f cache pts %f", consume_pts, videoState->video_decode_frame_pts);
        *outFrame = ioCodecCtx->outFrameP;
        return 0;
    }

    float deltaTime = consume_pts - videoState->video_prev_consume_pts;
    enum FFStrategyState strategy = STRATEGY_NONE;

    if (deltaTime > videoState->seek_threshold) {// 解码慢了
        strategy = STRATEGY_SEEK_FORWARD;
    } else if (deltaTime < -videoState->seek_threshold) {// 解码快了
        strategy = STRATEGY_SEEK_BACKWARD;
    } else {
        if (fabs(deltaTime) < videoState->threshold) {  // 半帧以内
            strategy = STRATEGY_STOP;
        } else if (deltaTime > videoState->threshold) {  // 向前解码
            strategy = STRATEGY_ACCELERATE_FORWARD;
        } else if (deltaTime < -videoState->threshold) {  // 向后解码
            strategy = STRATEGY_ACCELERATE_BACKWARD;
        }
    }

    bool seek_flag = strategy == STRATEGY_SEEK_FORWARD || strategy == STRATEGY_SEEK_BACKWARD;
    bool decode_flag = !seek_flag && strategy != STRATEGY_STOP;

    if (consume_pts == 0) {
        decode_flag = true;
    }

    if (!seek_flag && !decode_flag) {
        ff_log("no need seek_flag decode_flag");
        return 0;
    }
    
    //向前seek
    //向后seek
    //半帧/一帧
    //帧不存在, 如何处理
    //滑动时只显示关键帧, 松手时精准seek
    if (seek_flag) {
//        bool inSameGop = ff_is_same_gop(ioCodecCtx, consume_pts, videoState->video_prev_consume_pts);
//        if (!inSameGop) {
        float key_pts = _ff_depend_keyframe(ioCodecCtx, consume_pts);
        ff_log("hit seek keyframe %f to %f gap %f", key_pts, consume_pts, consume_pts - key_pts);
        _ff_stream_seek(ioCodecCtx, consume_pts);
//        }
    } else if (decode_flag) {

    }
    
    bool find_flag = false;
    bool stop_flag = false;
    //读取packet和读取frame要分开两个函数, 因为packet和frame不是一一对应的关系, 当packet读取结束时, 还可以继续去读取缓存里的frame
    while (true) {
        if (stop_flag || ioCodecCtx->decode_eof || ioCodecCtx->error_exit) {
            ff_log("ERROR: stop_flag = %d, decode_eof = %d, error_exit = %d", stop_flag, ioCodecCtx->decode_eof, ioCodecCtx->error_exit);
            break;
        }
        ret = _ff_send_video_packet(ioCodecCtx, consume_pts, videoState->event == DECODE_EVENT_SEEK);
        if (ret != AVERROR_EOF && ret != AVERROR(EAGAIN) && ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "ff_decode_video_packet fail %s\n", av_err2str(ret));
            ioCodecCtx->error_exit = true;
            break;
        }
        // 循环不断从解码器读数据，直到没有数据可读。
        while (true) {
            ret = _ff_receive_video_frame(ioCodecCtx, ioCodecCtx->srcFrame, &ioCodecCtx->outFrameP);
            if (ret == FF_DECODE_OK) {
                float ptsMs = ioCodecCtx->srcFrame->pts * av_q2d(ioCodecCtx->video_stream->time_base) * 1000;
                float dtsMs = ioCodecCtx->srcFrame->pkt_dts * av_q2d(ioCodecCtx->video_stream->time_base) * 1000;
                ff_log("decode %c 帧 consume_pts: %f frame pts: %f, diff: %f, interval: %f", av_get_picture_type_char(ioCodecCtx->srcFrame->pict_type), consume_pts, ptsMs, consume_pts - ptsMs, videoState->threshold);

                videoState->video_decode_frame_pts = ptsMs;
                if (fabsf(ptsMs - consume_pts) < fabsf(videoState->threshold)) {
                    *outFrame = ioCodecCtx->outFrameP;
                    stop_flag = true;
                    find_flag = true;
                    break;
                } else {
                    if (videoState->video_decode_frame_pts > consume_pts) {
                        *outFrame = ioCodecCtx->outFrameP;
                        stop_flag = true;
                        break;
                    }
                }
            } else {
                if (AVERROR(EAGAIN) == ret) {
                    av_log(NULL, AV_LOG_ERROR, "ERROR: decode frame AVERROR(EAGAIN) consume_pts %f\n", consume_pts);
                    break;
                } else if (AVERROR_EOF == ret) {
                    av_log(NULL, AV_LOG_ERROR, "ERROR: decode frame AVERROR_EOF consume_pts %f\n", consume_pts);
                    ioCodecCtx->decode_eof = true;
                    break;
                } else {
                    av_log(NULL, AV_LOG_ERROR, "ERROR: decode error consume_pts %f msg: %s\n", consume_pts, av_err2str(ret));
                    ioCodecCtx->error_exit = true;
                    break;
                }
            }
        }
    }
    
    if (find_flag) {
        float ptsMs = ioCodecCtx->srcFrame->pts * av_q2d(ioCodecCtx->video_stream->time_base) * 1000;
        ff_log("hit decode success consume pts %f, frame pts: %f, diff: %f", consume_pts, ptsMs, consume_pts - ptsMs);
    } else {
        if (ret == FF_DECODE_OK) {
            ff_log("decode success, but no consume pts %f", consume_pts);
        } else {
            ff_log("not find consume pts %f", consume_pts);
        }
    }
    videoState->video_prev_consume_pts = consume_pts;

    // Stop measuring time and calculate the elapsed time
    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds*1e-9;
    ff_log("cost time measured: %.3f ms", elapsed * 1000);
    videoState->running = false;
//    printf("running end\n");

    return 0;
}

//是否是seek事件, 是手动的触发seek时间, decode事件下的seek操作不执行丢帧策略
int ffdecode::_ff_send_video_packet(FFCodecContext *ioCodecCtx, float consume_pts, bool ff_decode_event) {
    if (ioCodecCtx->packet_eof) {
        return AVERROR_EOF;
    }
    while (true) {
        int ret = av_read_frame(ioCodecCtx->fmt_ctx, ioCodecCtx->avpacket);
        if (ret == FF_DECODE_OK) {
            if (ioCodecCtx->video_stream_index != ioCodecCtx->avpacket->stream_index) {
                av_packet_unref(ioCodecCtx->avpacket);
                continue;
            }
            float ptsMs = ioCodecCtx->avpacket->pts * av_q2d(ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->time_base) * 1000;
            float dtsMs = ioCodecCtx->avpacket->dts * av_q2d(ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->time_base) * 1000;

            bool isKeyFrame = ioCodecCtx->avpacket->flags & AV_PKT_FLAG_KEY;
            ff_log("read packet keyframe %d, ptsMs: %f, dtsMs %f", isKeyFrame, ptsMs, dtsMs);
            //sps pps 软解码不支持annex模式, 转成startcode会导致软解无法解码
//            if (!ioCodecCtx->mPacketIsHadSpsPps) {
//                AVPacket new_packet;
//                av_init_packet(&new_packet);
//                if (ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codecpar->codec_id == AV_CODEC_ID_H264) {
//                    ioCodecCtx->mBitFilterContext = av_bitstream_filter_init("h264_mp4toannexb");
//                    if (ioCodecCtx->mBitFilterContext == NULL) {
//                        printf("cannot open the h264_mp4toannexb");
//                    }
//                } else if (ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codecpar->codec_id == AV_CODEC_ID_HEVC) {
//                    ioCodecCtx->mBitFilterContext = av_bitstream_filter_init("hevc_mp4toannexb");
//                    if (ioCodecCtx->mBitFilterContext == NULL) {
//                        printf("cannot open the hevc_mp4toannexb");
//                    }
//                }
//                if (NULL != ioCodecCtx->mBitFilterContext) {
//                    av_bitstream_filter_filter(ioCodecCtx->mBitFilterContext, ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codec, NULL, &new_packet.data, &new_packet.size, ioCodecCtx->avpacket->data, ioCodecCtx->avpacket->size, 0);
//                } else {
//                    ioCodecCtx->mIsStreamNotSupport = true;
//                    return -3;
//                }
//
//                ioCodecCtx->avcodec_context = ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codec;
//                av_packet_unref(&new_packet);
//                ioCodecCtx->mPacketIsHadSpsPps = true;
//                ioCodecCtx->mBitFilterContext = NULL;
//            }

            if (ff_decode_event) {
                int nal_unit_type = (ioCodecCtx->avpacket->data[4] & 0x1F);
                int nal_ref_idc = (ioCodecCtx->avpacket->data[4] & 0x60);
                
                if ((nal_ref_idc == 0 || nal_ref_idc == 1) && fabs(consume_pts - ptsMs) > videoState->seek_threshold) {
                    ff_log("== nal_unit_type %d, ==nal_ref_idc %d", nal_unit_type, nal_ref_idc);
                    continue;
                }
            }

            avcodec_send_packet(ioCodecCtx->avcodec_context, ioCodecCtx->avpacket);
            av_packet_unref(ioCodecCtx->avpacket);

            return ret;
        } else {
            if (ret == AVERROR_EOF) {
                // 读取完文件，这时候 pkt 的 data 跟 size 应该是 null
                avcodec_send_packet(ioCodecCtx->avcodec_context, NULL);
                ioCodecCtx->packet_eof = true;
                return ret;
            } else if (ret == AVERROR(EAGAIN)) {
//                continue; //这里需要缓存, 尝试再次发送
                av_log(NULL, AV_LOG_ERROR, "ff_decode_video_packet ret %d, %s\n", ret, av_err2str(ret));
                return ret; //验证下是否需要特殊处理???
            } else {
                av_log(NULL, AV_LOG_ERROR, "ERROR: read error code %d, %s \n", ret, av_err2str(ret));
                return ret;
            }
        }
    }
    return FF_DECODE_OK;
}

int ffdecode::_ff_receive_video_frame(FFCodecContext *ioCodecCtx, AVFrame *srcFrame, AVFrame **dstFrame) {
    int ret = FF_DECODE_OK;
    ret = avcodec_receive_frame(ioCodecCtx->avcodec_context, srcFrame);
    printf("avcodec_receive_frame ret %d\n", ret);
    /* 释放 frame 里面的YUV数据，
     * 由于 avcodec_receive_frame 函数里面会调用 av_frame_unref，所以下面的代码可以注释。
     * 所以我们不需要 手动 unref 这个 AVFrame
     * */
    // av_frame_unref(frame);
    if (ret == 0) {
        if (ioCodecCtx->avcodec_context->width != srcFrame->linesize[0]) {
            int height = sws_scale(ioCodecCtx->sws_context,
                                   (const uint8_t *const *)srcFrame->data, srcFrame->linesize,
                                   0, srcFrame->height,
                                   ioCodecCtx->swsFrame->data,
                                   ioCodecCtx->swsFrame->linesize);
            if (height <= 0) {
                av_log(NULL, AV_LOG_ERROR, "decoderPacket sws_scale error, height is %d\n", height);
                return -1;
            }
            *dstFrame = ioCodecCtx->swsFrame;
        } else {
            *dstFrame = srcFrame;
        }
        return ret;
    } else {
        return ret;
    }
//    else if (AVERROR(EAGAIN) == ret) {
//        // 提示 EAGAIN 代表 解码器 需要 更多的 AVPacket
//        // 跳出 第一层 for，让 解码器拿到更多的 AVPacket
//        return ret;;
//    } else if (AVERROR_EOF == ret) {
//        //提示 AVERROR_EOF 代表之前已经往 解码器发送了一个 data 跟 size 都是 NULL 的 AVPacket
//        return ret;;
//    } else {
//        printf("other fail \n");
//        return ret;
//    }
}

int ffdecode::_ff_swsscale_frame(FFVideoState *videoState, AVFrame *srcFrame, AVFrame **dstFrame) {
    AVFrame *frameP = srcFrame;
    FFCodecContext *ioCodecCtx = videoState->ioCodecCtx;
    if (ioCodecCtx->rotate == 90 || ioCodecCtx->rotate == 270 || ioCodecCtx->rotate == 180) {
        _ff_yuv_rotate(ioCodecCtx->rotate, srcFrame, ioCodecCtx->rotateFrame);
        frameP = ioCodecCtx->rotateFrame;
    }

    AVPixelFormat pixFmt = (AVPixelFormat)srcFrame->format;
    //统一处理, 转成rgba
    if (videoState->pixelFormat == AV_PIX_FMT_RGBA) {
        int height = sws_scale(ioCodecCtx->rgbaContext,
                               (const uint8_t *const *)frameP->data, frameP->linesize,
                               0, frameP->height,
                               ioCodecCtx->rgbaFrame->data,
                               ioCodecCtx->rgbaFrame->linesize);
        if (height <= 0) {
            av_log(NULL, AV_LOG_ERROR, "decoderPacket sws_scale error, height is %d\n", height);
            return -1;
        }
        frameP = ioCodecCtx->rgbaFrame;
    }
    
    // libyuv::I420ToRGBA(NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0, 0);
//    libyuv::I420
    /*
     I420ToRGBA(const uint8_t* src_y,
                    int src_stride_y,
                    const uint8_t* src_u,
                    int src_stride_u,
                    const uint8_t* src_v,
                    int src_stride_v,
                    uint8_t* dst_rgba,
                    int dst_stride_rgba,
                    int width,
                    int height);

     */
    *dstFrame = frameP;
    return 0;
}

ffdecode::~ffdecode() {
    
}

#pragma mark - test func
int ffdecode::av_io_decode_test(uint8_t *heapData, size_t file_len, const char *outputFile) {
    FFCodecContext *ioCodecCtx = (FFCodecContext *)av_mallocz(sizeof(FFCodecContext));
    videoState = (FFVideoState *)av_mallocz(sizeof(FFVideoState));
    ioBuffer = (FFBufferData *)av_mallocz(sizeof(FFBufferData));
    videoState->ioCodecCtx = ioCodecCtx;
    int ret = 0;
    int err;
    ioCodecCtx->avio_ctx_buffer_size = 4096;

    ioBuffer->ptr = heapData;
    ioBuffer->ori_ptr = heapData;
    ioBuffer->has_size = file_len;
    ioBuffer->file_size = file_len;

    // 打开输入文件
    ioCodecCtx->fmt_ctx = avformat_alloc_context();
    if (!ioCodecCtx->fmt_ctx) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }

    ioCodecCtx->avio_buffer = (uint8_t *)av_malloc(ioCodecCtx->avio_ctx_buffer_size);
    printf("avio_ctx_buffer is %p \n", ioCodecCtx->avio_buffer);
    if (!ioCodecCtx->avio_buffer) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    ioCodecCtx->avio_ctx = avio_alloc_context(ioCodecCtx->avio_buffer, ioCodecCtx->avio_ctx_buffer_size, 0, ioBuffer, &read_packet_ptr, NULL, &seek_in_buffer_ptr);
    if (!ioCodecCtx->avio_ctx) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    ioCodecCtx->fmt_ctx->pb = ioCodecCtx->avio_ctx;

    //avformat_open_input 会去读很多帧数据进行检测
    if ((err = avformat_open_input(&ioCodecCtx->fmt_ctx, NULL, NULL, NULL)) < 0) {
        printf("can not open file %d \n", err);
        return err;
    }

    ret = avformat_find_stream_info(ioCodecCtx->fmt_ctx, NULL);
    if (ret < 0) {
        printf("avformat_find_stream_info file %d \n", ret);
        return ret;
    }

    // 打开解码器
    ioCodecCtx->avcodec_context = avcodec_alloc_context3(NULL);
    ioCodecCtx->video_stream_index = av_find_best_stream(ioCodecCtx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ret = avcodec_parameters_to_context(ioCodecCtx->avcodec_context, ioCodecCtx->fmt_ctx->streams[ioCodecCtx->video_stream_index]->codecpar);
    if (ret < 0) {
        printf("error code %d \n", ret);
        return ret;
    }
    const AVCodec *codec = avcodec_find_decoder(ioCodecCtx->avcodec_context->codec_id);
    if ((ret = avcodec_open2(ioCodecCtx->avcodec_context, codec, NULL)) < 0) {
        printf("open codec faile %d \n", ret);
        return ret;
    }

    outfile = fopen(outputFile, "wb");
    if (!outfile) {
        return -1;
    }

    ioCodecCtx->avpacket = av_packet_alloc();
    ioCodecCtx->srcFrame = av_frame_alloc();

    enum AVPixelFormat srcPixFmt = ioCodecCtx->avcodec_context->pix_fmt;
    enum AVPixelFormat dstPixFmt = AV_PIX_FMT_YUV420P;
    int dstWidth = ioCodecCtx->avcodec_context->width;
    int dstHeight = ioCodecCtx->avcodec_context->height;
    ioCodecCtx->swsFrame = av_frame_alloc();
    ioCodecCtx->swsFrame->format = dstPixFmt;
    ret = av_image_alloc(ioCodecCtx->swsFrame->data, ioCodecCtx->swsFrame->linesize, dstWidth, dstHeight, dstPixFmt, 1);

    int srcWidth = dstWidth;
    int srcHeight = dstHeight;
    // 输出frame的参数
    // int outWidth1 = inWidth >> 4 << 4;
    // int outHeight1 = inHeight >> 4 << 4;

    ioCodecCtx->swsFrame->width = dstWidth;
    ioCodecCtx->swsFrame->height = dstHeight;
    ioCodecCtx->sws_context = sws_getContext(srcWidth, srcHeight, srcPixFmt,
                                dstWidth, dstHeight, dstPixFmt,
                                SWS_FAST_BILINEAR, NULL, NULL, NULL);

    printf("width = %d, height = %d\n", ioCodecCtx->swsFrame->width, ioCodecCtx->swsFrame->height);

    int read_end = 0;
    while (true) {
        if (1 == read_end) {
            break;
        }

        ret = av_read_frame(ioCodecCtx->fmt_ctx, ioCodecCtx->avpacket);
        // 跳过不处理音频包
        if (ioCodecCtx->video_stream_index != ioCodecCtx->avpacket->stream_index) {
            av_packet_unref(ioCodecCtx->avpacket);
            continue;
        }

        if (AVERROR_EOF == ret) {
            // 读取完文件，这时候 pkt 的 data 跟 size 应该是 null
            avcodec_send_packet(ioCodecCtx->avcodec_context, NULL);
        } else {
            if (0 != ret) {
                printf("read error code %d, %s \n", ret, av_err2str(ret));
                return ENOMEM;
            } else {
            retry:
                if (avcodec_send_packet(ioCodecCtx->avcodec_context, ioCodecCtx->avpacket) == AVERROR(EAGAIN)) {
                    printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    // 这里可以考虑休眠 0.1 秒，返回 EAGAIN 通常是 ffmpeg 的内部 api 有bug
                    goto retry;
                }
                // 释放 pkt 里面的编码数据
                av_packet_unref(ioCodecCtx->avpacket);
            }
        }

        // 循环不断从解码器读数据，直到没有数据可读。
        while (true) {
            // 读取 AVFrame
            ret = avcodec_receive_frame(ioCodecCtx->avcodec_context, ioCodecCtx->srcFrame);
            /* 释放 frame 里面的YUV数据，
             * 由于 avcodec_receive_frame 函数里面会调用 av_frame_unref，所以下面的代码可以注释。
             * 所以我们不需要 手动 unref 这个 AVFrame
             * */
            // av_frame_unref(frame);

            if (AVERROR(EAGAIN) == ret) {
                // 提示 EAGAIN 代表 解码器 需要 更多的 AVPacket
                // 跳出 第一层 for，让 解码器拿到更多的 AVPacket
                break;
            } else if (AVERROR_EOF == ret) {
                /* 提示 AVERROR_EOF 代表之前已经往 解码器发送了一个 data 跟 size 都是 NULL 的 AVPacket
                 * 发送 NULL 的 AVPacket 是提示解码器把所有的缓存帧全都刷出来。
                 * 通常只有在 读完输入文件才会发送 NULL 的 AVPacket，或者需要用现有的解码器解码另一个的视频流才会这么干。
                 * 往编码器发送 null 的 AVFrame，让编码器把剩下的数据刷出来。
                 */

                // 跳出 第二层 for，文件已经解码完毕。
                read_end = 1;
                break;
            } else if (ret >= 0) {
                // 往编码器发送 AVFrame，然后不断读取 AVPacket

                int height = sws_scale(ioCodecCtx->sws_context,
                                       (const uint8_t *const *)ioCodecCtx->srcFrame->data, ioCodecCtx->srcFrame->linesize,
                                       0, ioCodecCtx->srcFrame->height,
                                       ioCodecCtx->swsFrame->data,
                                       ioCodecCtx->swsFrame->linesize);
                if (height <= 0) {
//                    MOGIC_CHECK_ERROR(true, ret, "FFVideoDecoderSWFF::decoderPacket sws_scale error, height is zer");
                    return height;
                }

#ifdef __APPLE__
//                enum AVPixelFormat pixelForamt = (AVPixelFormat)ioCodecCtx->srcFrame->format;
                int y_size = ioCodecCtx->swsFrame->width * ioCodecCtx->swsFrame->height;
                int u_size = y_size / 4;
                int v_size = y_size / 4;

                fwrite(ioCodecCtx->swsFrame->data[0], 1, y_size, outfile);//Y
                fwrite(ioCodecCtx->swsFrame->data[1], 1, u_size ,outfile);//U:宽高均是Y的一半
                fwrite(ioCodecCtx->swsFrame->data[2], 1, v_size, outfile);//V:宽高均是Y的一半
#endif
            } else {
                printf("other fail \n");
                return ret;
            }
        }
    }

    ff_decode_free(0);
    printf("done \n");
    return 0;
}

} // namespace ffwasm
