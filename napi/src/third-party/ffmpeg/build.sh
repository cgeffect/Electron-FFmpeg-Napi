#!/bin/sh
workdir=$(cd $(dirname $0); pwd)
rootdir=$(cd $workdir/../../; pwd)

src_dir_inside=ffmpeg-4.4.1

# x264_dir="$workdir/../x264/deploy"
# x265_dir="$workdir/../x265/deploy"
# vpx_dir="$workdir/../libvpx/deploy"
# fdk_aac_dir="$workdir/../fdk-aac/deploy"
# rtmp_dir="$workdir/../librtmp/deploy"
# zimg_dir="$workdir/../zimg/deploy"

############ build ffmpeg ############
echo "--- build ffmpeg ${lib_type} ---"
rm -rf "$workdir/deploy" && rm -rf "$workdir/${src_dir_inside}" && \
cd "$workdir" && tar -zxvf "$workdir/source/${src_dir_inside}.tar.gz"

cd "$workdir/${src_dir_inside}" && \
./configure \
    --prefix="${workdir}/deploy/" \
    --target-os=darwin \
    --enable-ffmpeg \
    --enable-ffplay \
    --enable-ffprobe \
    --enable-doc \
    --enable-gpl \
    --enable-nonfree \
    --enable-version3 \
    --enable-static \
    --enable-pic \
    --enable-version3 \
    --disable-avdevice

make -j12 && make install

if [[ $? -ne 0 ]]; then
    echo "ERROR: Failed to build ffmpeg"
    exit -1
fi

# x265 需要链接 stdc++ 库
echo "success!"
