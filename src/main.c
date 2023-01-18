#include <stdio.h>

// /usr/loacal/ffmpeg/include
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

int open_decode()
{
    const char *config = avcodec_configuration();
    printf("open decode %s\n", config);
    return 0;
}

int decode_data()
{
    printf("%s\n", "decode_data");
    return 0;
}

// https://www.jianshu.com/p/e4236635c981
int main(int agrc, char *argv[])
{
    open_decode();
    decode_data();
    printf("%s\n", "hello world");
    return 0;
}

// 使用clang编译c文件
// clang -g -o ff_decode_video ff_decode_video.c