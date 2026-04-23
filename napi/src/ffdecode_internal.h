#ifndef FF_DECODE_INTERNAL_H_
#define FF_DECODE_INTERNAL_H_

#include "ffdecode_types.h"

namespace ffwasm {

extern FILE* outfile;
extern unsigned sws_flags;

int read_packet_ptr(void* opaque, uint8_t* buf, int buf_size);
int64_t seek_in_buffer_ptr(void* opaque, int64_t offset, int whence);

float get_videostream_durationMs(FFCodecContext* ioCodecCtx);
int _ff_parser_keyframes(FFCodecContext* ioCodecCtx);

}  // namespace ffwasm

#endif  // FF_DECODE_INTERNAL_H_
