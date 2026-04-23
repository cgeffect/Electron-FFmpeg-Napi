# ffwasm

FFmpeg based wasm decoder for web playback.

## Directory layout

- `src`: C/C++ source code for the wasm bridge and decoder logic
- `third`: prebuilt third-party FFmpeg static libraries and headers
- `video`: Vite web app that consumes `video/libffwasm/libffmpeg.js`

## Build wasm

```bash
source ./emsdk/emsdk_env.sh
./buildwasm.sh
```

After build:

- generated wasm/js are in `dist`
- files are copied to `video/libffwasm`

## Run web demo

```bash
cd video
npm install
npm run dev
```