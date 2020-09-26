/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#ifndef NV_VIDEO_SOURCE
#define NV_VIDEO_SOURCE

#include "dynlink_nvcuvid.h" // <nvcuvid.h>

#include <string>

extern "C"
{
#include "libavutil/avstring.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
//#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

//#include "libavfilter/avfiltergraph.h"
//#include "libavfilter/buffersink.h"
//#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
}

#define __cu(a) do { \
    CUresult  ret; \
    if ((ret = (a)) != CUDA_SUCCESS) { \
        fprintf(stderr, "%s has returned CUDA error %d\n", #a, ret); \
        return false;\
    }} while(0)

typedef struct
{
    int  codecs;
    char *name;
} _sVideoFormats;

static _sVideoFormats eVideoFormats[] =
{
    { cudaVideoCodec_MPEG1,    "MPEG-1" },
    { cudaVideoCodec_MPEG2,    "MPEG-2" },
    { cudaVideoCodec_MPEG4,    "MPEG-4 (ASP)" },
    { cudaVideoCodec_VC1,      "VC-1/WMV" },
    { cudaVideoCodec_H264,     "AVC/H.264" },
    { cudaVideoCodec_JPEG,     "M-JPEG" },
	{ cudaVideoCodec_H264_SVC, "H.264/SVC" },
	{ cudaVideoCodec_H264_MVC, "H.264/MVC" },
	{ cudaVideoCodec_HEVC,     "H.265/HEVC" },
	{ cudaVideoCodec_NumCodecs,"Invalid" },
    { cudaVideoCodec_YUV420,   "YUV  4:2:0" },
    { cudaVideoCodec_YV12,     "YV12 4:2:0" },
    { cudaVideoCodec_NV12,     "NV12 4:2:0" },
    { cudaVideoCodec_YUYV,     "YUYV 4:2:2" },
    { cudaVideoCodec_UYVY,     "UYVY 4:2:2" },
    {                  -1 , "Unknown" },
};

// forward declarations
class FrameQueue;
class VideoParser;


// A wrapper class around the CUvideosource entity and API.
//  The CUvideosource manages video-source streams (right now
// via openening a file containing the stream.) After successfully
// opening a video stream, one can query its properties, such as
// video and audio compression format, frame-rate, etc.
//
// The video-source spawns its own thread for processing the stream.
// The user can register call-back methods for handling chucks of demuxed
// audio and video data.
class VideoSource
{
    public:
        VideoSource();

        // Destructor
        ~VideoSource();

        // This reloads the video source file
        void ReloadVideo(const std::string sFileName, FrameQueue *pFrameQueue, VideoParser *pVideoParser);

        //delete
        //CUVIDEOFORMAT format();

        // In order to process video-frames, we need to hook up a video-parser
        // object to this source.
        // Internally we set the CUvideoparser wrapped in rVideoParser on the
        // user-data struct, that gets passed into the source's callbacks. That
        // way we can feed the CUDA video parser with the video-data chunks delivered
        // by this source.
        ////void setParser(VideoParser &rVideoParser, CUcontext cuCtx);

        // Begin processing the video stream.
        void
        start();

        // End processing the video stream.
        void
        stop();


        // Retrieve source dimensions (width, height) from the video
        void getSourceDimensions(unsigned int &width, unsigned int &height);

        // Retrieve display dimensions (width, height) for the video
        void getDisplayDimensions(unsigned int &width, unsigned int &height);

        // Retrieve information about the video (is this progressive?)
        void getProgressive(bool &progressive);

        //add 
        bool init(const std::string sFileName);
        bool run();
        

    private:
        // This struct contains the data we need inside the source's
        // video callback in order to processes the video data.
        struct VideoSourceData
        {
            CUvideoparser hVideoParser;
            FrameQueue   *pFrameQueue;
        };


        // Callback for handling packages of demuxed video data.
        //
        // Parameters:
        //      pUserData - Pointer to user data. We must pass a pointer to a
        //          VideoSourceData struct here, that contains a valid CUvideoparser
        //          and FrameQueue.
        //      pPacket - video-source data packet.
        //
        // NOTE: called from a different thread that doesn't not have a cuda context
        //
        static
        int
        CUDAAPI
        HandleVideoData(void *pUserData, CUVIDSOURCEDATAPACKET *pPacket);

        // Copy constructor. Don't implement.
        VideoSource(const VideoSource &);

        // Assignment operator. Don't implement.
        void
        operator= (const VideoSource &);

        VideoSourceData oSourceData_;       // Instance of the user-data struct we use in the video-data handle callback.
        CUvideosource   hVideoSource_;      // Handle to the CUDA video-source object.

        CUVIDEOFORMAT g_stFormat; //cuda里的摄像机流格式
		CUcontext g_oContext;

};


#endif // NV_VIDEO_SOURCE

