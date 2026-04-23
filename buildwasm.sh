
set -euo pipefail

rm -rf ./dist
mkdir ./dist

export MEMORY=67108864
export FUNCTIONS="['_ffwasm_decode_open','_ffwasm_decode_frame','_ffwasm_decode_free','_ffwasm_hold_seek','_ffwasm_seek_frame','_malloc','_free','_main']"

emcc -std=c++17 \
    ./src/main.cpp ./src/ffdecode.cpp \
    ./third/libffmpeg/lib/libavcodec.a ./third/libffmpeg/lib/libavformat.a ./third/libffmpeg/lib/libavutil.a ./third/libffmpeg/lib/libswscale.a \
    -I "third/libffmpeg/include" \
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
    -o ./dist/libffmpeg.js

echo "build success!"

echo "copy ffwasm to video"
rm -rf ./video/libffwasm
mkdir -p ./video/libffwasm/src
cp -r ./dist/ ./video/libffwasm

cp ./src/*.cpp ./video/libffwasm/src/
cp ./src/*.h ./video/libffwasm/src/