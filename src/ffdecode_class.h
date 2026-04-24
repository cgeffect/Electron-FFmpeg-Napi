#ifndef FF_DECODE_CLASS_H_
#define FF_DECODE_CLASS_H_

#include <atomic>
#include <cstdint>

#include "ffdecode_types.h"

namespace ffwasm {

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

#endif  // FF_DECODE_CLASS_H_
