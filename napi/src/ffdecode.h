#ifndef FF_DECODE_H_
#define FF_DECODE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

// Enable verbose decoder logs by defining FFWASM_ENABLE_LOG.
#ifdef FFWASM_ENABLE_LOG
#define ff_log(format, ...) std::printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define ff_log(format, ...) ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct VideoInfo {
	int width;
	int height;
	int durationMs;
	float fps;
} VideoInfo;

typedef void (*DecodeCallback)(AVFrame* frame, float ptsMs);
typedef void (*InitCallback)(VideoInfo info);

void set_log_level(int level);

// pix_fmt: 1 = yuv420p, 2 = rgba
long ffcpp_decode_init(uint8_t* heapData, size_t file_len, int pix_fmt, const char* outputFile, InitCallback initcb, DecodeCallback cb);
int ffcpp_decode_frame(long handle, float pts, AVFrame** outFrame);
int ffcpp_hold_seek(long handle, bool seek);
int ffcpp_seek_frame(long handle, float ptsMs, AVFrame** outFrame);
int ffcpp_decode_free(long handle);
int _av_io_decode_test(uint8_t* heapData, size_t file_len, const char* outputFile);

#ifdef __cplusplus
}
#endif

namespace ffwasm {

struct FFBufferData {
	uint8_t* ptr = nullptr;
	uint8_t* ori_ptr = nullptr;
	size_t has_size = 0;
	size_t file_size = 0;
};

struct FFCodecContext {
	AVFormatContext* fmt_ctx = nullptr;
	AVIOContext* avio_ctx = nullptr;
	uint8_t* avio_buffer = nullptr;
	AVCodecContext* avcodec_context = nullptr;
	int rotateWidth = 0;
	int rotateHeight = 0;
	AVPacket* avpacket = nullptr;
	AVFrame* srcFrame = nullptr;
	int avio_ctx_buffer_size = 0;

	AVFrame* swsFrame = nullptr;
	SwsContext* sws_context = nullptr;
	int video_stream_index = -1;
	AVStream* video_stream = nullptr;
	float* keyFrameList = nullptr;
	int keyFrameCount = 0;
	bool packet_eof = false;
	bool decode_eof = false;
	float frame_rate = 0.0f;
	bool error_exit = false;
	float durationMs = 0.0f;

	int rotate = 0;
	AVFrame* rotateFrame = nullptr;
	SwsContext* swsContext = nullptr;
};

enum FFStrategyState {
	STRATEGY_NONE = 0,
	STRATEGY_ACCELERATE_FORWARD = 1,
	STRATEGY_STOP = 2,
	STRATEGY_SEEK_FORWARD = 3,
	STRATEGY_SEEK_BACKWARD = 4,
	STRATEGY_ACCELERATE_BACKWARD = 6
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

struct FFVideoState {
	float video_consume_pts = 0.0f;
	float video_prev_consume_pts = 0.0f;
	float video_decode_frame_pts = 0.0f;
	FFStrategyState strategy = STRATEGY_NONE;

	float threshold = 0.0f;
	float seek_threshold = 0.0f;

	FFCodecContext* ioCodecCtx = nullptr;
	DecodeCallback decodecb = nullptr;
	InitCallback initcb = nullptr;

	bool running = false;
	AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
	bool seek = false;
	FF_DECODE_EVENT event = DECODE_EVENT_NONE;
};

class ffdecode {
private:
	FFVideoState* videoState = nullptr;
	FFBufferData* ioBuffer = nullptr;
	std::atomic<bool> task_stop = false;
	std::atomic<bool> thread_stop = false;
	std::atomic<int32_t> consume_pts = -1;

public:
	ffdecode();
	~ffdecode();

	int ff_decode_init(uint8_t* heapData, size_t file_len, int pix_fmt, const char* outputFile, InitCallback initcb, DecodeCallback cb);
	int ff_decode_frame(float pts, AVFrame** outFrame);
	int ff_hold_seek(bool seek);
	int ff_seek_frame(float ptsMs, AVFrame** outFrame);
	int ff_decode_free(long handle);
	int av_io_decode_test(uint8_t* heapData, size_t file_len, const char* outputFile);
};

}  // namespace ffwasm

#endif  // FF_DECODE_H_
