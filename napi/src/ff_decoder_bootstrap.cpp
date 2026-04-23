#include "ff_decoder_bootstrap.h"

#include "ffdecode_internal.h"
#include "ffrotate.h"
extern "C" {
#include <libavutil/imgutils.h>
}

namespace ffwasm {
namespace {
constexpr int kAvioBufferSize = 4096;
}

FFDecoderBootstrap::FFDecoderBootstrap(FFVideoState* state, FFBufferData* buffer, uint8_t* heapData, size_t file_len, int pix_fmt, const char* outputFile, InitCallback initcb, DecodeCallback cb)
	: state_(state), codecCtx_(state ? state->ioCodecCtx : nullptr), buffer_(buffer), heapData_(heapData), fileLen_(file_len), pixFmt_(pix_fmt), outputFile_(outputFile), initcb_(initcb), decodecb_(cb) {}

int FFDecoderBootstrap::Run() {
	if (!state_ || !codecCtx_ || !buffer_) {
		return AVERROR(EINVAL);
	}
	state_->decodecb = decodecb_;
	state_->initcb = initcb_;
	state_->pixelFormat = (pixFmt_ == 2) ? AV_PIX_FMT_RGBA : AV_PIX_FMT_YUV420P;

	av_log_set_level(AV_LOG_WARNING);
	codecCtx_->avio_ctx_buffer_size = kAvioBufferSize;
	buffer_->Reset(heapData_, fileLen_);
	ff_log("file size %zu", fileLen_);

	int ret = SetupInput();
	if (ret < 0) return ret;
	ret = OpenCodec();
	if (ret < 0) return ret;
	ret = SetupFrames();
	if (ret < 0) return ret;
	SetupRuntimeFlags();
	ret = SetupRotation();
	if (ret < 0) return ret;
	ret = ParseVideoMeta();
	if (ret < 0) return ret;
	NotifyInit();
	state_->event = DECODE_EVENT_INIT;
	return 0;
}

int FFDecoderBootstrap::SetupInput() {
	codecCtx_->fmt_ctx = avformat_alloc_context();
	if (!codecCtx_->fmt_ctx) return AVERROR(ENOMEM);

	codecCtx_->avio_buffer = (uint8_t*)av_malloc(codecCtx_->avio_ctx_buffer_size);
	if (!codecCtx_->avio_buffer) return AVERROR(ENOMEM);

	codecCtx_->avio_ctx = avio_alloc_context(codecCtx_->avio_buffer, codecCtx_->avio_ctx_buffer_size, 0, buffer_, &read_packet_ptr, NULL, &seek_in_buffer_ptr);
	if (!codecCtx_->avio_ctx) return AVERROR(ENOMEM);
	codecCtx_->fmt_ctx->pb = codecCtx_->avio_ctx;

	int err = avformat_open_input(&codecCtx_->fmt_ctx, NULL, NULL, NULL);
	if (err < 0) return err;
	return avformat_find_stream_info(codecCtx_->fmt_ctx, NULL);
}

int FFDecoderBootstrap::OpenCodec() {
	codecCtx_->avcodec_context = avcodec_alloc_context3(NULL);
	codecCtx_->video_stream_index = av_find_best_stream(codecCtx_->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	int ret = avcodec_parameters_to_context(codecCtx_->avcodec_context, codecCtx_->fmt_ctx->streams[codecCtx_->video_stream_index]->codecpar);
	if (ret < 0) return ret;
	const AVCodec* codec = avcodec_find_decoder(codecCtx_->avcodec_context->codec_id);
	ret = avcodec_open2(codecCtx_->avcodec_context, codec, NULL);
	if (ret < 0) return ret;
#ifdef __APPLE__
	if (outputFile_ != NULL && outputFile_[0] != '\0') {
		outfile = fopen(outputFile_, "wb");
		if (!outfile) return -1;
	}
#endif
	return 0;
}

int FFDecoderBootstrap::SetupFrames() {
	codecCtx_->durationMs = get_videostream_durationMs(codecCtx_);
	codecCtx_->avpacket = av_packet_alloc();
	codecCtx_->srcFrame = av_frame_alloc();

	enum AVPixelFormat srcPixFmt = codecCtx_->avcodec_context->pix_fmt;
	enum AVPixelFormat dstPixFmt = state_->pixelFormat;
	int dstWidth = codecCtx_->avcodec_context->width;
	int dstHeight = codecCtx_->avcodec_context->height;

	codecCtx_->swsFrame = av_frame_alloc();
	codecCtx_->swsFrame->format = dstPixFmt;
	int ret = av_image_alloc(codecCtx_->swsFrame->data, codecCtx_->swsFrame->linesize, dstWidth, dstHeight, dstPixFmt, 1);
	if (ret < 0) return ret;
	codecCtx_->swsFrame->width = dstWidth;
	codecCtx_->swsFrame->height = dstHeight;
	codecCtx_->sws_context = sws_getContext(dstWidth, dstHeight, srcPixFmt, dstWidth, dstHeight, dstPixFmt, sws_flags, NULL, NULL, NULL);
	return codecCtx_->sws_context ? 0 : AVERROR(ENOMEM);
}

void FFDecoderBootstrap::SetupRuntimeFlags() {
	codecCtx_->avcodec_context->thread_count = 8;
	codecCtx_->avcodec_context->skip_frame = AVDISCARD_NONREF;
	codecCtx_->video_stream = codecCtx_->fmt_ctx->streams[codecCtx_->video_stream_index];
	codecCtx_->video_stream->discard = AVDISCARD_NONREF;
}

int FFDecoderBootstrap::SetupRotation() {
	int srcRotate = ffrotate::getRotateAngle(codecCtx_->video_stream);
	codecCtx_->rotate = srcRotate;
	if (!(srcRotate == 90 || srcRotate == 270 || srcRotate == 180)) return 0;

	if (fabs(srcRotate - 90) < 1.0 || fabs(srcRotate - 270) < 1.0 || fabs(srcRotate + 90) < 1.0) {
		codecCtx_->rotateWidth = codecCtx_->avcodec_context->height;
		codecCtx_->rotateHeight = codecCtx_->avcodec_context->width;
	} else {
		codecCtx_->rotateWidth = codecCtx_->avcodec_context->width;
		codecCtx_->rotateHeight = codecCtx_->avcodec_context->height;
	}

	enum AVPixelFormat srcPixFmt = codecCtx_->avcodec_context->pix_fmt;
	int srcWidth = codecCtx_->avcodec_context->width;
	int srcHeight = codecCtx_->avcodec_context->height;
	int dstWidth = codecCtx_->rotateWidth;
	int dstHeight = codecCtx_->rotateHeight;

	codecCtx_->rotateFrame = av_frame_alloc();
	codecCtx_->rotateFrame->format = srcPixFmt;
	int ret = av_image_alloc(codecCtx_->rotateFrame->data, codecCtx_->rotateFrame->linesize, dstWidth, dstHeight, srcPixFmt, 1);
	if (ret < 0) return ret;
	codecCtx_->rotateFrame->width = dstWidth;
	codecCtx_->rotateFrame->height = dstHeight;
	codecCtx_->sws_context = sws_getContext(srcWidth, srcHeight, srcPixFmt, dstWidth, dstHeight, srcPixFmt, sws_flags, NULL, NULL, NULL);
	return codecCtx_->sws_context ? 0 : AVERROR(ENOMEM);
}

int FFDecoderBootstrap::ParseVideoMeta() {
	float avg_frame_rate = av_q2d(codecCtx_->video_stream->avg_frame_rate);
	codecCtx_->frame_rate = 1000 / avg_frame_rate;
	state_->threshold = ceil(codecCtx_->frame_rate / 2);
	state_->seek_threshold = 150;
	return _ff_parser_keyframes(codecCtx_);
}

void FFDecoderBootstrap::NotifyInit() {
	if (!state_->initcb) return;
	VideoInfo info = {
		codecCtx_->avcodec_context->width,
		codecCtx_->avcodec_context->height,
		static_cast<int>(codecCtx_->durationMs),
		static_cast<float>(av_q2d(codecCtx_->video_stream->avg_frame_rate))
	};
	state_->initcb(info);
}

}  // namespace ffwasm
