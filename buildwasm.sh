#!/usr/bin/env sh

set -eu

MODE="${1:-release}"
if [ "$MODE" != "release" ] && [ "$MODE" != "debug" ]; then
  echo "Usage: ./buildwasm.sh [release|debug]"
  exit 1
fi

OUT_DIR="./build/wasm"
WEB_WASM_DIR="./res/wasm"

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

export MEMORY=67108864
export FUNCTIONS="['_ffwasm_decode_open','_ffwasm_decode_frame','_ffwasm_decode_free','_ffwasm_hold_seek','_ffwasm_seek_frame','_malloc','_free','_main']"

if [ "$MODE" = "debug" ]; then
  # -fdebug-compilation-dir='.' keeps debug source paths relative to libffmpeg.js.
  emcc -g -fdebug-compilation-dir='.' -std=c++17 \
    ./src/main.cpp ./src/ffdecode.cpp \
    ./src/third-party/libffmpeg/lib/libavcodec.a ./src/third-party/libffmpeg/lib/libavformat.a ./src/third-party/libffmpeg/lib/libavutil.a ./src/third-party/libffmpeg/lib/libswscale.a \
    -I "src/third-party/libffmpeg/include" \
    --no-entry \
    --bind \
    -s TOTAL_MEMORY=${MEMORY} \
    -s EXPORTED_FUNCTIONS=${FUNCTIONS} \
    -s EXPORTED_RUNTIME_METHODS="['addFunction']" \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ALLOW_TABLE_GROWTH=1 \
    -s EXPORT_NAME="FFInit" \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s ENVIRONMENT="web" \
    -s EXPORT_ES6=1 \
    -O3 \
    -o "${OUT_DIR}/libffmpeg.js"
else
  emcc -std=c++17 \
    ./src/main.cpp ./src/ffdecode.cpp \
    ./src/third-party/libffmpeg/lib/libavcodec.a ./src/third-party/libffmpeg/lib/libavformat.a ./src/third-party/libffmpeg/lib/libavutil.a ./src/third-party/libffmpeg/lib/libswscale.a \
    -I "src/third-party/libffmpeg/include" \
    --no-entry \
    --bind \
    -s TOTAL_MEMORY=${MEMORY} \
    -s EXPORTED_FUNCTIONS=${FUNCTIONS} \
    -s EXPORTED_RUNTIME_METHODS="['addFunction']" \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ALLOW_TABLE_GROWTH=1 \
    -s EXPORT_NAME="FFInit" \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s ENVIRONMENT="web" \
    -s EXPORT_ES6=1 \
    -O3 \
    -o "${OUT_DIR}/libffmpeg.js"
fi

echo "build success! mode=${MODE}"

echo "copy wasm runtime to web asset directory"
rm -rf "${WEB_WASM_DIR}"
mkdir -p "${WEB_WASM_DIR}"
cp "${OUT_DIR}/libffmpeg.js" "${WEB_WASM_DIR}/libffmpeg.js"
cp "${OUT_DIR}/libffmpeg.wasm" "${WEB_WASM_DIR}/libffmpeg.wasm"

if [ "$MODE" = "debug" ]; then
  echo "copy source files for chrome wasm debugging"
  mkdir -p "${WEB_WASM_DIR}/src"
  cp ./src/*.cpp "${WEB_WASM_DIR}/src/"
  cp ./src/*.h "${WEB_WASM_DIR}/src/"
fi