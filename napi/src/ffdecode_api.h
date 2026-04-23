#ifndef FF_DECODE_API_H_
#define FF_DECODE_API_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>

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

#endif  // FF_DECODE_API_H_
