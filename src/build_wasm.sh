rm -rf ./dist
mkdir ./dist

export MEMORY=67108864
export FUNCTIONS="['_open_decode','_decode_data']"

emcc ff_decode_video.c libffmpeg/lib/libavcodec.a libffmpeg/lib/libavformat.a libffmpeg/lib/libavutil.a libffmpeg/lib/libswscale.a \
    -I "libffmpeg/include" \
    --no-entry \
    -s TOTAL_MEMORY=${MEMORY} \
    -s EXPORTED_FUNCTIONS=${FUNCTIONS} \
    -s EXPORTED_RUNTIME_METHODS="['addFunction']" \
    -s MODULARIZE=1 \
    -s EXPORT_ES6=1 \
    -s ENVIRONMENT="web" \
    -s USE_ES6_IMPORT_META=0 \
    -O2 \
    -o ./dist/libffmpeg.js

echo "build success!"
