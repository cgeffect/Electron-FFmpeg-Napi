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

## Electron 播放器（`napi/web`）

`napi/web` 下提供了一个 Electron 壳，流程是：

- Renderer: 控制 UI + Canvas(WebGL) 渲染 YUV
- Preload: 暴露受限 IPC API
- Main: 调用 N-API 解码并回传帧数据

运行方式：

```bash
cd napi
npm install
npm run build
npm run electron
```

界面支持：

- 打开本地视频文件
- 播放 / 暂停
- 拖动进度条（内部走 `holdSeek + seekFrame`）
- 正常时间轴连续解码播放（`decodeFrame`）

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

也可以用更简化的 JS 封装：

```js
const fs = require('fs')
const { NativePlayer } = require('./index')

const player = new NativePlayer()
player.openFromBuffer(fs.readFileSync('./640.mp4'))
const frame = player.decodeAt(1000)
player.close()
```

## 迁移说明

播放器与 N-API 绑定源码集中在 `src/core/`（解码、引导、资源释放、旋转等），FFmpeg 头文件与静态库在 `src/third-party/ffmpeg/deploy/`。  
迁移时保留 `src/core` 与 `src/third-party` 目录结构，并保证本机或目标环境能正确链接 FFmpeg 库即可。
