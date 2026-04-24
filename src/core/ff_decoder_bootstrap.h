#ifndef FF_DECODER_BOOTSTRAP_H_
#define FF_DECODER_BOOTSTRAP_H_

#include "ffdecode_types.h"

namespace ffwasm {

class FFDecoderBootstrap {
public:
	FFDecoderBootstrap(FFVideoState* state, FFBufferData* buffer, uint8_t* heapData, size_t file_len, int pix_fmt, const char* outputFile, InitCallback initcb, DecodeCallback cb);
	int Run();

private:
	int SetupInput();
	int OpenCodec();
	int SetupFrames();
	void SetupRuntimeFlags();
	int SetupRotation();
	int ParseVideoMeta();
	void NotifyInit();

private:
	FFVideoState* state_ = nullptr;
	FFCodecContext* codecCtx_ = nullptr;
	FFBufferData* buffer_ = nullptr;
	uint8_t* heapData_ = nullptr;
	size_t fileLen_ = 0;
	int pixFmt_ = 1;
	const char* outputFile_ = nullptr;
	InitCallback initcb_ = nullptr;
	DecodeCallback decodecb_ = nullptr;
};

}  // namespace ffwasm

#endif  // FF_DECODER_BOOTSTRAP_H_
