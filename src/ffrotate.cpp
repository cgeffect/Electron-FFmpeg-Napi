
#include "ffrotate.h"
namespace ffwasm {

int ffrotate::getRotateAngle(AVStream *avStream) {
    AVDictionaryEntry *tag = NULL;
    int mRotate = 0;
    tag = av_dict_get(avStream->metadata, "rotate", tag, 0);
//    float count = av_dict_count(avStream->metadata);
    if (tag == NULL) {
        mRotate = 0;
    } else {
        int angle = atoi(tag->value);
        angle %= 360;
        if (angle == 90) {
            mRotate = 90;
        } else if (angle == 180) {
            mRotate = 180;
        } else if (angle == 270) {
            mRotate = 270;
        } else {
            mRotate = 0;
        }
    }
    return mRotate;
}

void ffrotate::frameRotate90(AVFrame *src, AVFrame *des) {
    int n = 0;
    int hw = src->width >> 1;
    int hh = src->height >> 1;
    int size = src->width * src->height;
    int hsize = size >> 2;

    int pos = 0;
    // copy y
    for (int j = 0; j < src->width; j++) {
        pos = size;
        for (int i = src->height - 1; i >= 0; i--) {
            pos -= src->width;
            des->data[0][n++] = src->data[0][pos + j];
        }
    }
    // copy uv
    n = 0;
    for (int j = 0; j < hw; j++) {
        pos = hsize;
        for (int i = hh - 1; i >= 0; i--) {
            pos -= hw;
            des->data[1][n] = src->data[1][pos + j];
            des->data[2][n] = src->data[2][pos + j];
            n++;
        }
    }

    des->linesize[0] = src->height;
    des->linesize[1] = src->height >> 1;
    des->linesize[2] = src->height >> 1;
    des->height = src->width;
    des->width = src->height;
    des->format = src->format;
    des->pts = src->pts;
    des->key_frame = src->key_frame;
}

void ffrotate::frameRotate180(AVFrame *src, AVFrame *des) {
    int n = 0, i = 0;
    int hw = src->width >> 1;
    int hh = src->height >> 1;
    int pos = src->width * src->height;

    for (i = 0; i < src->height; i++) {
        pos -= src->width;
        for (int j = 0; j < src->width; j++) {
            des->data[0][n++] = src->data[0][pos + j];
        }
    }

    n = 0;
    pos = src->width * src->height >> 2;

    for (i = 0; i < hh; i++) {
        pos -= hw;
        for (int j = 0; j < hw; j++) {
            des->data[1][n] = src->data[1][pos + j];
            des->data[2][n] = src->data[2][pos + j];
            n++;
        }
    }

    des->linesize[0] = src->width;
    des->linesize[1] = src->width >> 1;
    des->linesize[2] = src->width >> 1;

    des->width = src->width;
    des->height = src->height;
    des->format = src->format;
    des->pts = src->pts;
    des->key_frame = src->key_frame;
}

void ffrotate::frameRotate270(AVFrame *src, AVFrame *des) {
    int n = 0, i = 0, j = 0;
    int hw = src->width >> 1;
    int hh = src->height >> 1;
    int pos = 0;

    for (i = src->width - 1; i >= 0; i--) {
        pos = 0;
        for (j = 0; j < src->height; j++) {
            des->data[0][n++] = src->data[0][pos + i];
            pos += src->width;
        }
    }

    n = 0;
    for (i = hw - 1; i >= 0; i--) {
        pos = 0;
        for (j = 0; j < hh; j++) {
            des->data[1][n] = src->data[1][pos + i];
            des->data[2][n] = src->data[2][pos + i];
            pos += hw;
            n++;
        }
    }

    des->linesize[0] = src->height;
    des->linesize[1] = src->height >> 1;
    des->linesize[2] = src->height >> 1;

    des->width = src->height;
    des->height = src->width;
    des->format = src->format;
    des->pts = src->pts;
    des->key_frame = src->key_frame;
}

} // namespace ffwasm
