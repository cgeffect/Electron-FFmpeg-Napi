#include <napi.h>

#include <cmath>
#include <cstdint>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "ffdecode_api.h"

// 本文件：N-API 绑定层。JS 通过 exports 上的函数进入各 *Wrapped，再调 ffdecode_api / FFmpeg；会话表在匿名命名空间内。

namespace {

// open 成功后缓存在 Session 里，供 getInfo 返回给 JS（与 C 侧 OnInitCallback 写入的 snapshot 一致）。
struct VideoInfoSnapshot {
	int width{0};
	int height{0};
	int durationMs{0};
	float fps{0.0f};
};

// 每个 decode 会话：C 句柄 + 持久化 Buffer（防止 ffcpp 仍读内存时 JS 侧 buffer 被 GC）+ 打开时的视频元信息。
struct Session {
	long handle{0};
	Napi::Reference<Napi::Buffer<uint8_t>> videoBufferRef;
	VideoInfoSnapshot info;
};

// C 层 decode 句柄 → 会话；与 open/getInfo/decode/close 等互斥使用 g_sessionsMutex。
std::mutex g_sessionsMutex;
std::unordered_map<long, Session> g_sessions;
// ffcpp_decode_init 同步回调 OnInitCallback 时，把 VideoInfo 写回此处指向的快照（仅 open 路径使用）。
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

// native.setLogLevel(level) → set_log_level（C API）。
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

// native.open(buffer)：整文件 Buffer 交给 ffcpp_decode_init，返回 BigInt 句柄；会话进 g_sessions，Buffer 做 Persistent。
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

// 从 JS 传入的 handle 解析为 long（支持 BigInt 或 Number，与 open 返回类型一致）。
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

// native.getInfo(handle)：读 g_sessions 里缓存的宽高、时长、fps，打成普通 Object 返回 JS。
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

// 把 FFmpeg AVFrame（C 侧指针与平面数据）拷贝/封装成 Napi::Object、Buffer 等，作为 Napi::Value 回到 JS；decodeFrame / seekFrame 成功路径共用。
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

// JS 调用 native.decodeFrame(handle, ptsMs) 时由 N-API 进入此函数；info 为运行时注入的本次调用上下文（参数见 info[0]、[1]），内部再调 ffcpp_decode_frame。
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

// native.seekFrame(handle, ptsMs) → ffcpp_seek_frame，成功则同样经 BuildFrameObject 回传 YUV 平面到 JS。
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

// native.holdSeek(handle, seek) → ffcpp_hold_seek，返回整型结果给 JS。
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

// native.close(handle)：移出 g_sessions、释放 Persistent，再 ffcpp_decode_free；无效 handle 返回 false。
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

// 加载顺序见文件末尾「步骤」；本函数由 Node 在加载本 .node 时调用，负责把 API 挂到 exports。
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

// 从源码到 JS 可调 native.open / close 等的顺序（与本文件的关系）：
// 1. node-gyp 按 binding.gyp 编译、链接 → 生成 build/Release/ffmpeg_player_napi.node。
// 2. JS 执行 require（如 index.js 里 bindings）→ Node 解析并打开该 .node 路径。
// 3. 进程首次载入该动态库时，由 NODE_API_MODULE 展开出的逻辑向 Node 注册：模块名 = 首参（须与 target_name 一致）、入口 = Init。
// 4. Node 调用上面的 Init(env, exports)，把各 Wrapped 挂到 exports 并 return。
// 5. require 的返回值即为 exports，此后 JS 可调用 native.open 等。
NODE_API_MODULE(ffmpeg_player_napi, Init)
