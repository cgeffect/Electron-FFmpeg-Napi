
#ifndef YUVRotate_hpp
#define YUVRotate_hpp

#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
}

namespace ffwasm {
class ffrotate {
public:
    static int getRotateAngle(AVStream *avStream);
    static void frameRotate90(AVFrame *src, AVFrame *des);
    static void frameRotate180(AVFrame *src, AVFrame *des);
    static void frameRotate270(AVFrame *src, AVFrame *des);
};
} // namespace ffwasm
#endif /* YUVRotate_hpp */
