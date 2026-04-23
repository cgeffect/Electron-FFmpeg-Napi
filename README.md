# ffwasm

FFmpeg based wasm decoder for web playback.

## Directory layout

- `src`: C/C++ source code for the wasm bridge and decoder logic
- `src/third-party`: prebuilt FFmpeg static libraries and headers
- `build/wasm`: generated wasm runtime artifacts
- `res`: web demo static resources (media + copied wasm runtime)
- `video`: Vite web app, serves `res` via `publicDir`

## Build wasm

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd ffwasm/emsdk
source ./emsdk/emsdk_env.sh
cd ..
./buildwasm.sh
./buildwasm.sh debug
```

After build:

- generated wasm/js are in `build/wasm`
- wasm runtime is copied to `res/wasm`
- in `debug` mode, source files are also copied to `res/wasm/src` for Chrome source-level wasm debugging

## Run web demo

```bash
cd video
npm install
npm run dev
```

## Recommended workflow

```bash
# Build wasm runtime and sync to res/wasm
sh buildwasm.sh
# Or debug build:
sh buildwasm.sh debug

# Run dev server
cd video
npm run dev

# For release package only (generates video/dist temporarily)
npm run build
```

## Video assets

- place test media files in `res`
- Vite serves `res` as `publicDir`, so files are available like `/11.mp4`
- wasm runtime is available as `/wasm/libffmpeg.js` and `/wasm/libffmpeg.wasm`
- default demo source is `/11.mp4` (in `video/src/main.ts`)