#include "ff_resource_guard.h"

namespace ffwasm {

void ReleaseCodecContext(FFCodecContext*& ioCodecCtx) {
	if (!ioCodecCtx) {
		return;
	}

	bool sws_alias_src = ioCodecCtx->swsFrame && ioCodecCtx->swsFrame == ioCodecCtx->srcFrame;
	bool sws_alias_rotate = ioCodecCtx->swsFrame && ioCodecCtx->swsFrame == ioCodecCtx->rotateFrame;

	if (ioCodecCtx->avio_ctx) {
		avio_context_free(&ioCodecCtx->avio_ctx);
		if (ioCodecCtx->fmt_ctx) {
			ioCodecCtx->fmt_ctx->pb = NULL;
		}
	}
	
	if (ioCodecCtx->swsFrame && !sws_alias_src && !sws_alias_rotate) {
		av_freep(&ioCodecCtx->swsFrame->data[0]);
		av_frame_free(&ioCodecCtx->swsFrame);
	}

	if (ioCodecCtx->srcFrame) {
		av_frame_free(&ioCodecCtx->srcFrame);
	}
	if (ioCodecCtx->avpacket) {
		av_packet_free(&ioCodecCtx->avpacket);
	}
	if (ioCodecCtx->sws_context) {
		sws_freeContext(ioCodecCtx->sws_context);
	}

	if (ioCodecCtx->rotateFrame && !sws_alias_src) {
		av_freep(&ioCodecCtx->rotateFrame->data[0]);
		av_frame_free(&ioCodecCtx->rotateFrame);
	}
	if (ioCodecCtx->swsContext) {
		sws_freeContext(ioCodecCtx->swsContext);
	}

	if (ioCodecCtx->avcodec_context) {
		avcodec_close(ioCodecCtx->avcodec_context);
		avcodec_free_context(&ioCodecCtx->avcodec_context);
	}
	if (ioCodecCtx->fmt_ctx) {
		avformat_free_context(ioCodecCtx->fmt_ctx);
	}

	av_free(ioCodecCtx);
	ioCodecCtx = NULL;
}

FFResourceGuard::FFResourceGuard(FFVideoState** state, FFBufferData** buffer)
	: state_(state), buffer_(buffer) {}

void FFResourceGuard::Dismiss() {
	active_ = false;
}

void FFResourceGuard::ReleaseAll() {
	if (state_ && *state_) {
		ReleaseCodecContext((*state_)->ioCodecCtx);
		av_free(*state_);
		*state_ = NULL;
	}
	if (buffer_ && *buffer_) {
		delete *buffer_;
		*buffer_ = NULL;
	}
}

FFResourceGuard::~FFResourceGuard() {
	if (active_) {
		ReleaseAll();
	}
}

}  // namespace ffwasm
