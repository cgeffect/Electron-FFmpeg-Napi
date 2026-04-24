# 本仓库中的 N-API、`addon.cpp` 与 `binding.gyp`

本文说明在本项目中如何把 **Node.js 原生扩展（N-API）** 编出来、**`binding.gyp`** 负责什么、**`addon.cpp`** 如何把 C++ 与 JavaScript 接起来，以及从 JS 侧如何调用。

---

## 1. 概念：N-API 与 node-addon-api

- **N-API**（Node-API）是 Node 提供的 **C 风格** 稳定 ABI，用于编写原生插件；同一套 `.node` 二进制在不同 Node 主版本间更易兼容。
- 本项目在 C++ 侧使用的是 **node-addon-api**（N-API 的 C++ 封装，`#include <napi.h>`）。它把 `napi_env`、`napi_value` 等包装成 `Napi::Env`、`Napi::Object` 等类型，写起来比纯 C API 更简洁。
- **编译与链接** 由 **node-gyp** 根据仓库根目录的 **`binding.gyp`** 生成平台相关的工程（如 macOS 上的 Xcode/Makefile），再产出 **`build/Release/ffmpeg_player_napi.node`**（文件名与 `binding.gyp` 里的 `target_name` 一致）。

---

## 2. `binding.gyp` 做什么、本项目里怎么配

`binding.gyp` 是 **gyp** 格式的构建描述文件，node-gyp 会读取它来决定：

| 字段 | 含义（本项目） |
|------|----------------|
| `target_name` | 生成的原生模块名，也是 `.node` 文件基名：`ffmpeg_player_napi`。 |
| `sources` | 参与编译的 `.cpp` 源文件列表。入口 **`src/addon.cpp`** 必须包含在内；其余为 **`src/core/`** 下的解码与辅助实现。 |
| `include_dirs` | 头文件搜索路径：`node-addon-api`、`src`（放 `ffdecode_api.h`）、`src/core`、FFmpeg 头文件目录。 |
| `libraries` | 链接的静态库：通过 `-L` 指向 **`src/third-party/ffmpeg/deploy/lib`**，并链接 `lavformat`、`lavcodec`、`lavutil`、`lswscale`。 |
| `cflags_cc` | C++ 标准为 **C++17**。 |
| `defines` | **`NAPI_DISABLE_CPP_EXCEPTIONS`**：与 node-addon-api 常见配置一致，避免在 N-API 边界依赖 C++ 异常跨边界传播。 |

构建命令（见 `package.json`）：

```bash
npm run build
```

等价于 `node-gyp rebuild`，会清理并重新编译。产出物路径一般为：

`build/Release/ffmpeg_player_napi.node`

若要改模块名或增减源文件，应同步修改 **`binding.gyp`** 的 `target_name` / `sources`，并保证 **`index.js`** 里 `bindings({ bindings: '...' })` 与 `target_name` 一致。

---

## 3. `addon.cpp` 的角色：原生入口与 JS 导出

`addon.cpp` **不是** 解码算法本体，而是 **N-API 绑定层**：把 JavaScript 的参数转成 C/C++ 类型，调用 **`ffdecode_api.h`** 里声明的 C 接口（实现在 `src/core/ffdecode.cpp` 等），再把结果包装成 JS 能用的值。

### 3.1 从构建到 `require` 可用的步骤顺序

1. **`npm run build`（node-gyp）**  
   按 **`binding.gyp`** 编译、链接 → 产出 **`build/Release/ffmpeg_player_napi.node`**。

2. **JS 执行 `require`**（本仓库中为 `index.js` 里 **`bindings({ bindings: 'ffmpeg_player_napi', ... })`**）  
   Node 解析路径并准备加载上述 **`.node`**。

3. **进程首次载入该动态库**  
   **`NODE_API_MODULE(...)`** 在编译产物里展开为向 Node **注册** 的信息：**模块名** = 宏的第一个实参（须与 **`target_name`** 一致）、**初始化入口** = **`Init`**。

4. **Node 调用 `Init(env, exports)`**  
   在 **`addon.cpp`** 里把 `open`、`close` 等 **`exports.Set(...)`** 挂好并 **`return exports`**。

5. **`require` 返回**  
   JS 拿到的就是带 **`native.open`** 等属性的对象，之后才是业务上的「打开视频、解码」等调用。

### 3.2 模块注册宏（与上述步骤 3～4 对应）

文件末尾：

```cpp
NODE_API_MODULE(ffmpeg_player_napi, Init)
```

- 第一个参数须与 **`binding.gyp` 的 `target_name`** 一致（本仓库为 `ffmpeg_player_napi`）。
- 第二个参数 **`Init`** 即上表 **步骤 4**：加载时被 Node 调用，向 **`exports`** 挂载函数。

### 3.3 `Init`：导出给 JavaScript 的 API

`Init` 把 C++ 包装函数注册到 `exports` 上，例如：

- `setLogLevel`
- `open`
- `getInfo`
- `decodeFrame`
- `seekFrame`
- `holdSeek`
- `close`

因此 **`require(...)` 得到的对象** 上会有这些属性名（与 `index.js` 里 `native.open`、`native.getInfo` 等调用对应）。

### 3.4 典型模式（与本文件一致）

1. **`Napi::Value XxxWrapped(const Napi::CallbackInfo& info)`**  
   从 `info[0]`、`info[1]` 读取参数，做类型检查；错误时用 `Napi::TypeError::New(...).ThrowAsJavaScriptException()` 等抛到 JS。

2. **与 C API 的衔接**  
   例如 `OpenWrapped` 里把 `Napi::Buffer<uint8_t>` 的指针和长度传给 `ffcpp_decode_init`；用 `Napi::Persistent(videoBuffer)` 延长 Buffer 生命周期，避免 C 层仍指向已被 GC 的内存。

3. **句柄**  
   `open` 返回 **`BigInt`** 句柄；`getInfo` / `decodeFrame` 等接受 **BigInt 或 Number**（见 `ReadHandle`）。

### 3.5 与 `ffdecode_api.h` 的关系

- **`ffdecode_api.h`**：对外 C 接口声明（初始化、解码、seek、释放等）。
- **`addon.cpp`**：只负责 **Node 世界 ↔ C 世界** 的转换与会话表（如 `g_sessions`）；复杂逻辑在 **`src/core/`**。

---

## 4. 从 JavaScript 如何使用（与 `addon.cpp` 的对应关系）

根目录 **`index.js`** 使用 **`bindings`** 包按名字加载 `.node`：

```js
const native = bindings({
  bindings: 'ffmpeg_player_napi',
  module_root: __dirname,
});
```

这里的 **`ffmpeg_player_napi`** 必须与 **`binding.gyp` 的 `target_name`** 一致。

随后可直接调用原生导出（即 `addon.cpp` 里 `exports.Set` 的那些名字），例如：

| JS 调用 | 说明 |
|---------|------|
| `native.setLogLevel(level)` | 对应 C 侧 `set_log_level`。 |
| `native.open(buffer)` | 传入 **Node Buffer**（整段视频文件字节），返回 **BigInt** 句柄。 |
| `native.getInfo(handle)` | 返回含 `width`、`height`、`durationMs`、`fps` 的对象。 |
| `native.decodeFrame(handle, ptsMs)` | 按时间解码一帧；失败时可能返回 `null`。 |
| `native.seekFrame(handle, ptsMs)` | Seek 后取帧。 |
| `native.holdSeek(handle, seek)` | 布尔 `seek` 控制 hold 行为。 |
| `native.close(handle)` | 释放会话并 `ffcpp_decode_free`。 |

`index.js` 里的 **`NativePlayer`** 类是对上述 API 的薄封装（保存 `handle`、`openFromBuffer`、`decodeAt` 等）。

---

## 5. 扩展：在 `addon.cpp` 里增加一个新导出

简要步骤：

1. 在 **`ffdecode_api.h` / `src/core`** 中实现所需 C/C++ 能力（若需要）。
2. 在 **`addon.cpp`** 中新增 `Napi::Value YourFuncWrapped(const Napi::CallbackInfo& info)`，做好参数校验与返回值构造。
3. 在 **`Init`** 里 `exports.Set("yourFunc", Napi::Function::New(env, YourFuncWrapped));`。
4. **一般不需要改 `binding.gyp`**，除非新增 **`.cpp` 源文件**（把新文件加入 `sources`）。
5. **`npm run build`** 后，在 JS 里通过 `native.yourFunc(...)` 调用。

---

## 6. 小结

- **`binding.gyp`**：告诉 node-gyp **编译哪些文件、搜哪些头、链哪些库、模块叫什么**。
- **`addon.cpp`**：实现 **`NODE_API_MODULE`** 与 **`Init`**，把 **node-addon-api** 包装函数挂到 **`exports`**，内部调用 **`ffdecode_api.h`** 的 C API。
- **JS**：用 **`bindings({ bindings: 'ffmpeg_player_napi', ... })`** 加载 **`ffmpeg_player_napi.node`**，函数名与 **`addon.cpp` 的 `exports.Set` 第一个参数** 一一对应。

如需对照实现，可直接打开仓库中的 **`src/addon.cpp`**、**`binding.gyp`** 与 **`index.js`**。
