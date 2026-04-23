# ffmpeg-player N-API prototype

这是一个独立的 Node.js + N-API 原型工程，不会改动现有 WASM 代码路径。

## 能力

- `open(buffer)`：打开视频（内存 Buffer）
- `getInfo(handle)`：获取视频信息
- `decodeFrame(handle, ptsMs)`：按时间点解码一帧
- `seekFrame(handle, ptsMs)`：seek 并解码一帧
- `holdSeek(handle, seek)`：传递 seek 状态
- `close(handle)`：释放资源

返回的帧为 YUV420P 三平面 `Buffer`：`y/u/v`，同时返回 `lineY/lineU/lineV` 和 `width/height`。

## 依赖

你需要本机可链接的 FFmpeg 开发库（`avformat/avcodec/avutil/swscale`）。

默认会尝试：

- include: `/opt/homebrew/include`、`/usr/local/include`
- lib: `/opt/homebrew/lib`、`/usr/local/lib`

如果你的路径不同，可在构建前设置：

```bash
export FFMPEG_INCLUDE_DIR=/path/to/include
export FFMPEG_LIB_DIR=/path/to/lib
```

## 构建

```bash
cd napi
npm install
npm run build
```

## 运行示例

```bash
npm run demo -- /absolute/path/to/your.mp4
```

如果不传参数，默认读 `../res/640.mp4`。

## 在 Node 项目里调用

```js
const fs = require('fs')
const addon = require('./napi')

const video = fs.readFileSync('./640.mp4')
const { handle, info } = addon.openFromBuffer(video)
console.log(info)

const frame = addon.decodeFrame(handle, 0)
if (frame) {
  // frame.y frame.u frame.v are Buffers
}
addon.close(handle)
```

## 迁移说明

当前原型使用 `napi/src` 下的本地源码进行构建（`ffdecode.cpp`、`ffrotate.cpp`、`third-party/libffmpeg/include`）。  
因此 `napi` 目录可以作为独立原型直接迁移，只需要保证目标机器安装了可链接的 FFmpeg 动态库。
