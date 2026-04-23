#!/bin/bash

rm -rf libffmpeg
mkdir -p ./libffmpeg

cd ../ffmpeg
make clean

emconfigure ./configure --cc="emcc" --cxx="em++" --ar="emar" --nm="emnm" \
	--prefix=$(pwd)/../decoder/libffmpeg \
	--enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
	--disable-avdevice --disable-swresample --disable-postproc --disable-avfilter \
	--disable-programs --disable-debug --disable-doc \
	--disable-asm \
	--enable-gpl --enable-version3

make install
