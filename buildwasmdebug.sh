
cp ./xcode/ffwasm/ffwasm/main.cpp ./src/main.cpp

rm -rf ./dist
mkdir ./dist

export MEMORY=67108864
export FUNCTIONS="['_ffwasm_decode_open','_ffwasm_decode_frame','_ffwasm_decode_free','_ffwasm_hold_seek','_ffwasm_seek_frame']"

#-fdebug-compilation-dir='.' c 代码路径, 相对于ffmpeg.js的目录
#-g 
#EMCC_DEBUG=1
emcc -g -std=c++17 \
    ./src/main.cpp ./src/ffdecode.cpp ./src/ffrotate.cpp \
    -fdebug-compilation-dir='.' \
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
    -s USE_ES6_IMPORT_META=0 \
    -D __FFWASM__ \
    -o ./dist/libffmpeg.js

echo "build success!"

echo "copy ffwasm to web"
rm -rf ./web/ffmpegkit
mkdir -p ./web/ffmpegkit/src
cp -r ./dist/ ./web/ffmpegkit

cp ./src/main.cpp ./web/ffmpegkit/src/main.cpp
cp ./src/ffdecode.h ./web/ffmpegkit/src/ffdecode.h
cp ./src/ffdecode.cpp ./web/ffmpegkit/src/ffdecode.cpp
cp ./src/ffrotate.h ./web/ffmpegkit/src/ffrotate.h
cp ./src/ffrotate.cpp ./web/ffmpegkit/src/ffrotate.cpp

echo "copy ffwasm to video"
rm -rf ./video/libffwasm
mkdir -p ./video/libffwasm/src
cp -r ./dist/ ./video/libffwasm

cp ./src/main.cpp ./video/libffwasm/src/main.cpp
cp ./src/ffdecode.h ./video/libffwasm/src/ffdecode.h
cp ./src/ffdecode.cpp ./video/libffwasm/src/ffdecode.cpp
cp ./src/ffrotate.h ./video/libffwasm/src/ffrotate.h
cp ./src/ffrotate.cpp ./video/libffwasm/src/ffrotate.cpp
