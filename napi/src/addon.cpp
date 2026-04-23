#include <napi.h>

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "ffdecode.h"

namespace {

struct VideoInfoSnapshot {
	int width{0};
	int height{0};
	int durationMs{0};
	float fps{0.0f};
};

struct Session {
	long handle{0};
	Napi::Reference<Napi::Buffer<uint8_t>> videoBufferRef;
	VideoInfoSnapshot info;
};

std::mutex g_sessionsMutex;
std::unordered_map<long, Session> g_sessions;
VideoInfoSnapshot* g_openingInfo = nullptr;
std::mutex g_openingMutex;

void OnInitCallback(VideoInfo info) {
	std::lock_guard<std::mutex> lock(g_openingMutex);
	if (!g_openingInfo) {
		return;
	}
	g_openingInfo->width = info.width;
	g_openingInfo->height = info.height;
	g_openingInfo->durationMs = info.durationMs;
	g_openingInfo->fps = info.fps;
}

Napi::Value SetLogLevelWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 1 || !info[0].IsNumber()) {
		Napi::TypeError::New(env, "setLogLevel(level): level must be number").ThrowAsJavaScriptException();
		return env.Null();
	}
	int level = info[0].As<Napi::Number>().Int32Value();
	set_log_level(level);
	return env.Undefined();
}

Napi::Value OpenWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 1 || !info[0].IsBuffer()) {
		Napi::TypeError::New(env, "open(buffer): buffer must be Node.js Buffer").ThrowAsJavaScriptException();
		return env.Null();
	}

	Napi::Buffer<uint8_t> videoBuffer = info[0].As<Napi::Buffer<uint8_t>>();
	if (videoBuffer.Length() == 0) {
		Napi::Error::New(env, "open(buffer): empty buffer").ThrowAsJavaScriptException();
		return env.Null();
	}

	VideoInfoSnapshot snapshot;
	{
		std::lock_guard<std::mutex> openingLock(g_openingMutex);
		g_openingInfo = &snapshot;
	}

	long handle = ffcpp_decode_init(videoBuffer.Data(), videoBuffer.Length(), 1, nullptr, OnInitCallback, nullptr);

	{
		std::lock_guard<std::mutex> openingLock(g_openingMutex);
		g_openingInfo = nullptr;
	}

	if (handle <= 0) {
		Napi::Error::New(env, "ffcpp_decode_init failed").ThrowAsJavaScriptException();
		return env.Null();
	}

	Session session;
	session.handle = handle;
	session.videoBufferRef = Napi::Persistent(videoBuffer);
	session.info = snapshot;

	{
		std::lock_guard<std::mutex> lock(g_sessionsMutex);
		g_sessions[handle] = std::move(session);
	}

	return Napi::BigInt::New(env, static_cast<int64_t>(handle));
}

bool ReadHandle(const Napi::Value& value, long* outHandle) {
	if (value.IsBigInt()) {
		bool lossless = false;
		int64_t h = value.As<Napi::BigInt>().Int64Value(&lossless);
		if (!lossless) {
			return false;
		}
		*outHandle = static_cast<long>(h);
		return true;
	}
	if (value.IsNumber()) {
		double h = value.As<Napi::Number>().DoubleValue();
		if (!std::isfinite(h)) {
			return false;
		}
		*outHandle = static_cast<long>(h);
		return true;
	}
	return false;
}

Napi::Value GetInfoWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 1) {
		Napi::TypeError::New(env, "getInfo(handle): missing handle").ThrowAsJavaScriptException();
		return env.Null();
	}

	long handle = 0;
	if (!ReadHandle(info[0], &handle) || handle == 0) {
		Napi::TypeError::New(env, "getInfo(handle): invalid handle").ThrowAsJavaScriptException();
		return env.Null();
	}

	std::lock_guard<std::mutex> lock(g_sessionsMutex);
	auto it = g_sessions.find(handle);
	if (it == g_sessions.end()) {
		Napi::Error::New(env, "session not found").ThrowAsJavaScriptException();
		return env.Null();
	}

	Napi::Object out = Napi::Object::New(env);
	out.Set("width", Napi::Number::New(env, it->second.info.width));
	out.Set("height", Napi::Number::New(env, it->second.info.height));
	out.Set("durationMs", Napi::Number::New(env, it->second.info.durationMs));
	out.Set("fps", Napi::Number::New(env, it->second.info.fps));
	return out;
}

Napi::Value BuildFrameObject(Napi::Env env, AVFrame* frame, float ptsMs) {
	Napi::Object out = Napi::Object::New(env);
	out.Set("ptsMs", Napi::Number::New(env, ptsMs));
	out.Set("width", Napi::Number::New(env, frame->width));
	out.Set("height", Napi::Number::New(env, frame->height));
	out.Set("lineY", Napi::Number::New(env, frame->linesize[0]));
	out.Set("lineU", Napi::Number::New(env, frame->linesize[1]));
	out.Set("lineV", Napi::Number::New(env, frame->linesize[2]));

	size_t ySize = static_cast<size_t>(frame->linesize[0]) * static_cast<size_t>(frame->height);
	size_t uSize = static_cast<size_t>(frame->linesize[1]) * static_cast<size_t>(frame->height / 2);
	size_t vSize = static_cast<size_t>(frame->linesize[2]) * static_cast<size_t>(frame->height / 2);

	Napi::Buffer<uint8_t> y = Napi::Buffer<uint8_t>::Copy(env, frame->data[0], ySize);
	Napi::Buffer<uint8_t> u = Napi::Buffer<uint8_t>::Copy(env, frame->data[1], uSize);
	Napi::Buffer<uint8_t> v = Napi::Buffer<uint8_t>::Copy(env, frame->data[2], vSize);

	out.Set("y", y);
	out.Set("u", u);
	out.Set("v", v);
	return out;
}

Napi::Value DecodeFrameWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 2 || !info[1].IsNumber()) {
		Napi::TypeError::New(env, "decodeFrame(handle, ptsMs): invalid arguments").ThrowAsJavaScriptException();
		return env.Null();
	}

	long handle = 0;
	if (!ReadHandle(info[0], &handle) || handle == 0) {
		Napi::TypeError::New(env, "decodeFrame(handle, ptsMs): invalid handle").ThrowAsJavaScriptException();
		return env.Null();
	}
	float ptsMs = static_cast<float>(info[1].As<Napi::Number>().DoubleValue());

	AVFrame* frame = nullptr;
	int ret = ffcpp_decode_frame(handle, ptsMs, &frame);
	if (ret < 0 || !frame) {
		return env.Null();
	}
	return BuildFrameObject(env, frame, ptsMs);
}

Napi::Value SeekFrameWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 2 || !info[1].IsNumber()) {
		Napi::TypeError::New(env, "seekFrame(handle, ptsMs): invalid arguments").ThrowAsJavaScriptException();
		return env.Null();
	}
	long handle = 0;
	if (!ReadHandle(info[0], &handle) || handle == 0) {
		Napi::TypeError::New(env, "seekFrame(handle, ptsMs): invalid handle").ThrowAsJavaScriptException();
		return env.Null();
	}
	float ptsMs = static_cast<float>(info[1].As<Napi::Number>().DoubleValue());

	AVFrame* frame = nullptr;
	int ret = ffcpp_seek_frame(handle, ptsMs, &frame);
	if (ret < 0 || !frame) {
		return env.Null();
	}
	return BuildFrameObject(env, frame, ptsMs);
}

Napi::Value HoldSeekWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 2 || !info[1].IsBoolean()) {
		Napi::TypeError::New(env, "holdSeek(handle, seek): invalid arguments").ThrowAsJavaScriptException();
		return env.Null();
	}
	long handle = 0;
	if (!ReadHandle(info[0], &handle) || handle == 0) {
		Napi::TypeError::New(env, "holdSeek(handle, seek): invalid handle").ThrowAsJavaScriptException();
		return env.Null();
	}
	bool seek = info[1].As<Napi::Boolean>().Value();
	int ret = ffcpp_hold_seek(handle, seek);
	return Napi::Number::New(env, ret);
}

Napi::Value CloseWrapped(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if (info.Length() < 1) {
		Napi::TypeError::New(env, "close(handle): missing handle").ThrowAsJavaScriptException();
		return env.Null();
	}
	long handle = 0;
	if (!ReadHandle(info[0], &handle) || handle == 0) {
		return Napi::Boolean::New(env, false);
	}

	{
		std::lock_guard<std::mutex> lock(g_sessionsMutex);
		auto it = g_sessions.find(handle);
		if (it != g_sessions.end()) {
			it->second.videoBufferRef.Reset();
			g_sessions.erase(it);
		}
	}

	ffcpp_decode_free(handle);
	return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set("setLogLevel", Napi::Function::New(env, SetLogLevelWrapped));
	exports.Set("open", Napi::Function::New(env, OpenWrapped));
	exports.Set("getInfo", Napi::Function::New(env, GetInfoWrapped));
	exports.Set("decodeFrame", Napi::Function::New(env, DecodeFrameWrapped));
	exports.Set("seekFrame", Napi::Function::New(env, SeekFrameWrapped));
	exports.Set("holdSeek", Napi::Function::New(env, HoldSeekWrapped));
	exports.Set("close", Napi::Function::New(env, CloseWrapped));
	return exports;
}

}  // namespace

NODE_API_MODULE(ffmpeg_player_napi, Init)
