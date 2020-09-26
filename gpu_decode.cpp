#include <stdio.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#include "VideoSource.h"
#include "dynlink_nvcuvid.h"

int main(int argc, const char* argv[])
{

    const char *rtsp;
    rtsp = "rtsp://admin:admin12345@192.168.0.191:554/h264";
    bool ret;

    VideoSource *videoDecoder = new VideoSource();

    //初始化解码器
    ret = videoDecoder->init(rtsp);

    //运行解码
    ret = videoDecoder->run(); 
    return 0;
    
}
