# ffwasm

基于 FFmpeg 的 wasm 视频解码示例工程（用于 Web 播放）。

## 目录结构

- `src`：C/C++ 源码（wasm bridge 与解码逻辑）
- `src/third-party`：预编译的 FFmpeg 静态库与头文件
- `build/wasm`：wasm 编译输出产物
- `res`：Web 示例静态资源（媒体文件 + wasm runtime）
- `video`：Vite 前端工程，通过 `publicDir` 读取 `res`

## 编译 wasm

```bash
# 首次或需要更新 emsdk 时执行（默认安装并激活 latest）
sh init_submodule.sh
cd emsdk
# 如需指定版本可在执行脚本前设置环境变量，例如：
# EMSDK_TAG=3.1.67 sh init_submodule.sh
source ./emsdk_env.sh
cd ..
./buildwasm.sh
./buildwasm.sh debug
```

编译完成后：

- wasm/js 产物位于 `build/wasm`
- 运行时文件会复制到 `res/wasm`
- `debug` 模式会额外复制源码到 `res/wasm/src`，便于 Chrome wasm 源码级调试

## 运行 Web 示例

```bash
cd video
npm install
npm run dev
```

## 推荐工作流

```bash
# 生成 wasm runtime 并同步到 res/wasm
sh buildwasm.sh
# 或调试构建
sh buildwasm.sh debug

# 启动前端开发服务
cd video
npm run dev

# 仅在需要发布时构建（临时生成 video/dist）
npm run build
```

## 视频与资源说明

- 测试媒体文件放在 `res`
- Vite 将 `res` 作为 `publicDir`，可通过 `/640.mp4` 这类路径访问
- wasm runtime 访问路径：
  - `/wasm/libffmpeg.js`
  - `/wasm/libffmpeg.wasm`
- 默认示例视频在 `video/src/main.ts` 中配置