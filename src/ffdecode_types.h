#ifndef FF_DECODE_TYPES_H_
#define FF_DECODE_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ffdecode_api.h"

namespace ffwasm {

class FFBufferData {
public:
	FFBufferData() = default;

	void Reset(uint8_t* data, size_t fileSize) {
		ptr_ = data;
		ori_ptr_ = data;
		has_size_ = fileSize;
		file_size_ = fileSize;
	}

	int Read(uint8_t* out, int requestSize) {
		const int readSize = requestSize < static_cast<int>(has_size_) ? requestSize : static_cast<int>(has_size_);
		if (!out) {
			return -1;
		}
		if (readSize <= 0) {
			return 0;
		}
		std::memcpy(out, ptr_, static_cast<size_t>(readSize));
		ptr_ += readSize;
		has_size_ -= static_cast<size_t>(readSize);
		return readSize;
	}

	int64_t SeekSet(int64_t offset) {
		ptr_ = ori_ptr_ + offset;
		has_size_ = file_size_ - static_cast<size_t>(offset);
		return reinterpret_cast<int64_t>(ptr_);
	}

	size_t RemainingSize() const {
		return has_size_;
	}

	size_t FileSize() const {
		return file_size_;
	}

private:
	uint8_t* ptr_ = nullptr;
	uint8_t* ori_ptr_ = nullptr;
	size_t has_size_ = 0;
	size_t file_size_ = 0;
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
	AVFrame* displayFrame = nullptr;
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
	float video_decode_frame_pts = -1.0f;
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

}  // namespace ffwasm

#endif  // FF_DECODE_TYPES_H_
