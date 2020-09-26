#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include "VideoDecoder.h"


static const char* getProfileName(int profile)
{
    switch (profile) {
        case 66:    return "Baseline";
        case 77:    return "Main";
        case 88:    return "Extended";
        case 100:   return "High";
        case 110:   return "High 10";
        case 122:   return "High 4:2:2";
        case 244:   return "High 4:4:4";
        case 44:    return "CAVLC 4:4:4";
    }

    return "Unknown Profile";
}

static int CUDAAPI HandleVideoData(void* pUserData, CUVIDSOURCEDATAPACKET* pPacket)
{
    assert(pUserData);
    CudaDecoder* pDecoder = (CudaDecoder*)pUserData;

    CUresult oResult = cuvidParseVideoData(pDecoder->m_videoParser, pPacket);
    if(oResult != CUDA_SUCCESS) {
        printf("error!\n");
    }

    return 1;
}

static int CUDAAPI HandleVideoSequence(void* pUserData, CUVIDEOFORMAT* pFormat)
{
    assert(pUserData);
    CudaDecoder* pDecoder = (CudaDecoder*)pUserData;

    if ((pFormat->codec         != pDecoder->m_oVideoDecodeCreateInfo.CodecType) ||         // codec-type
        (pFormat->coded_width   != pDecoder->m_oVideoDecodeCreateInfo.ulWidth)   ||
        (pFormat->coded_height  != pDecoder->m_oVideoDecodeCreateInfo.ulHeight)  ||
        (pFormat->chroma_format != pDecoder->m_oVideoDecodeCreateInfo.ChromaFormat))
    {
        fprintf(stderr, "NvTranscoder doesn't deal with dynamic video format changing\n");
        return 0;
    }

    return 1;
}

static int CUDAAPI HandlePictureDecode(void* pUserData, CUVIDPICPARAMS* pPicParams)
{
    assert(pUserData);
    CudaDecoder* pDecoder = (CudaDecoder*)pUserData;
    pDecoder->m_pFrameQueue->waitUntilFrameAvailable(pPicParams->CurrPicIdx);
    assert(CUDA_SUCCESS == cuvidDecodePicture(pDecoder->m_videoDecoder, pPicParams));
    return 1;
}

static int CUDAAPI HandlePictureDisplay(void* pUserData, CUVIDPARSERDISPINFO* pPicParams)
{
    assert(pUserData);
    CudaDecoder* pDecoder = (CudaDecoder*)pUserData;
    pDecoder->m_pFrameQueue->enqueue(pPicParams);
    pDecoder->m_decodedFrames++;

    return 1;
}

CudaDecoder::CudaDecoder() : m_videoSource(NULL), m_videoParser(NULL), m_videoDecoder(NULL),
    m_ctxLock(NULL), m_decodedFrames(0), m_bFinish(false)
{
}


CudaDecoder::~CudaDecoder(void)
{
    if(m_videoDecoder) cuvidDestroyDecoder(m_videoDecoder);
    if(m_videoParser)  cuvidDestroyVideoParser(m_videoParser);
    if(m_videoSource)  cuvidDestroyVideoSource(m_videoSource);
}

void CudaDecoder::InitVideoDecoder(CUvideoctxlock ctxLock, FrameQueue* pFrameQueue,
        int targetWidth, int targetHeight)
{
    assert(ctxLock);
    assert(pFrameQueue);

    tWidth = targetWidth;
    tHeight = targetHeight;
    m_pFrameQueue = pFrameQueue;

    CUresult oResult;
    m_ctxLock = ctxLock;

    //init video source
    /*CUVIDSOURCEPARAMS oVideoSourceParameters;
    memset(&oVideoSourceParameters, 0, sizeof(CUVIDSOURCEPARAMS));
    oVideoSourceParameters.pUserData = this;
    oVideoSourceParameters.pfnVideoDataHandler = HandleVideoData;
    oVideoSourceParameters.pfnAudioDataHandler = NULL;

    oResult = cuvidCreateVideoSource(&m_videoSource, videoPath, &oVideoSourceParameters);
    if (oResult != CUDA_SUCCESS) {
        fprintf(stderr, "cuvidCreateVideoSource failed\n");
        fprintf(stderr, "Please check if the path exists, or the video is a valid H264 file\n");
        exit(-1);
    }*/

    //init video decoder
    //CUVIDEOFORMAT oFormat;
    //cuvidGetSourceVideoFormat(m_videoSource, &oFormat, 0);

    /*if (oFormat.codec != cudaVideoCodec_H264) {
        fprintf(stderr, "The sample only supports H264 input video!\n");
        exit(-1);
    }

    if (oFormat.chroma_format != cudaVideoChromaFormat_420) {
        fprintf(stderr, "The sample only supports 4:2:0 chroma!\n");
        exit(-1);
    }*/

	CUVIDDECODECREATEINFO oVideoDecodeCreateInfo;
    memset(&oVideoDecodeCreateInfo, 0, sizeof(CUVIDDECODECREATEINFO));
    oVideoDecodeCreateInfo.CodecType = cudaVideoCodec_H264;
    oVideoDecodeCreateInfo.ulWidth   = targetWidth;
    oVideoDecodeCreateInfo.ulHeight  = targetHeight;
    oVideoDecodeCreateInfo.ulNumDecodeSurfaces = FrameQueue::cnMaximumSize;

    // Limit decode memory to 24MB (16M pixels at 4:2:0 = 24M bytes)
    // Keep atleast 6 DecodeSurfaces
    while (oVideoDecodeCreateInfo.ulNumDecodeSurfaces > 6 && 
        oVideoDecodeCreateInfo.ulNumDecodeSurfaces * targetWidth * targetHeight > 16 * 1024 * 1024)
    {
        oVideoDecodeCreateInfo.ulNumDecodeSurfaces--;
    }

    oVideoDecodeCreateInfo.ChromaFormat = cudaVideoChromaFormat_420;
    oVideoDecodeCreateInfo.OutputFormat = cudaVideoSurfaceFormat_NV12;
    oVideoDecodeCreateInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;

    if (targetWidth <= 0 || targetHeight <= 0) {
        oVideoDecodeCreateInfo.ulTargetWidth  = oVideoDecodeCreateInfo.ulWidth;
        oVideoDecodeCreateInfo.ulTargetHeight = oVideoDecodeCreateInfo.ulHeight;
    }
    else {
        oVideoDecodeCreateInfo.ulTargetWidth  = targetWidth;
        oVideoDecodeCreateInfo.ulTargetHeight = targetHeight;
    }

    oVideoDecodeCreateInfo.ulNumOutputSurfaces = 2;
    oVideoDecodeCreateInfo.ulCreationFlags = cudaVideoCreate_PreferCUVID;
    oVideoDecodeCreateInfo.vidLock = m_ctxLock;

    //创建cuda的解码器
    oResult = cuvidCreateDecoder(&m_videoDecoder, &oVideoDecodeCreateInfo);
    if (oResult != CUDA_SUCCESS) {
        fprintf(stderr, "cuvidCreateDecoder() failed, error code: %d\n", oResult);
        exit(-1);
    }

    m_oVideoDecodeCreateInfo = oVideoDecodeCreateInfo;

    //init video parser
    CUVIDPARSERPARAMS oVideoParserParameters;
    memset(&oVideoParserParameters, 0, sizeof(CUVIDPARSERPARAMS));
    oVideoParserParameters.CodecType = oVideoDecodeCreateInfo.CodecType;
    oVideoParserParameters.ulMaxNumDecodeSurfaces = oVideoDecodeCreateInfo.ulNumDecodeSurfaces;
    oVideoParserParameters.ulMaxDisplayDelay = 1;
    oVideoParserParameters.pUserData = this;

    //设置回调函数
    oVideoParserParameters.pfnSequenceCallback = HandleVideoSequence;
    oVideoParserParameters.pfnDecodePicture = HandlePictureDecode;
    oVideoParserParameters.pfnDisplayPicture = HandlePictureDisplay;

    //创建cuda的解析器
    oResult = cuvidCreateVideoParser(&m_videoParser, &oVideoParserParameters);
    if (oResult != CUDA_SUCCESS) {
        fprintf(stderr, "cuvidCreateVideoParser failed, error code: %d\n", oResult);
        exit(-1);
    }

}

bool CudaDecoder::mapFrame(int iPictureIndex, CUdeviceptr *ppDevice, unsigned int *pPitch, CUVIDPROCPARAMS *pVideoProcessingParameters)
{
    CUresult oResult = cuvidMapVideoFrame(m_videoDecoder,iPictureIndex,ppDevice,pPitch, pVideoProcessingParameters);
    return ((CUDA_SUCCESS == oResult) && (0 != *ppDevice) && (0 != *pPitch));
}

void CudaDecoder::YUV420P2BGR32(unsigned char *yuv_buffer_in, unsigned char *rgb_buffer_out, unsigned int pitch, int height, int width) {
    unsigned char *yuv_buffer = (unsigned char *)yuv_buffer_in;
    unsigned char *rgb_buffer = (unsigned char *)rgb_buffer_out;

    int channels = 3;
    int index_Y;
    int index_U;
    int index_V;
    unsigned char Y;
    unsigned char U;
    unsigned char V;
    int i = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            //std::cout << i << std::endl;
            // 取出 YUV
            index_Y = y * pitch + x;
            Y = yuv_buffer[index_Y];

            if (x % 2 == 0){
                index_U = (height + y / 2) * pitch + x;
                index_V = (height + y / 2) * pitch + x + 1;
                U = yuv_buffer[index_U];
                V = yuv_buffer[index_V];
            }else if (x % 2 == 1){
                index_V = (height + y / 2) * pitch + x;
                index_U = (height + y / 2) * pitch + x - 1;
                U = yuv_buffer[index_U];
                V = yuv_buffer[index_V];
            }




            // YCbCr420
            int R = Y + 1.402 * (V - 128);
            int G = Y - 0.34413 * (U - 128) - 0.71414*(V - 128);
            int B = Y + 1.772*(U - 128);


            // 确保取值范围在 0 - 255 中
            R = (R < 0) ? 0 : R;
            G = (G < 0) ? 0 : G;
            B = (B < 0) ? 0 : B;
            R = (R > 255) ? 255 : R;
            G = (G > 255) ? 255 : G;
            B = (B > 255) ? 255 : B;

            rgb_buffer[(y*width + x)*channels + 2] = (unsigned char)R;
            rgb_buffer[(y*width + x)*channels + 1] = (unsigned char)G;
            rgb_buffer[(y*width + x)*channels + 0] = (unsigned char)B;

            i++;
        }
    }
}




/*void CudaDecoder::Start()
{
    CUresult oResult;

    oResult = cuvidSetVideoSourceState(m_videoSource, cudaVideoState_Started);
    assert(oResult == CUDA_SUCCESS);

    while(cuvidGetVideoSourceState(m_videoSource) == cudaVideoState_Started);

    m_bFinish = true;

    m_pFrameQueue->endDecode();
}

void CudaDecoder::GetCodecParam(int* width, int* height, int* frame_rate_num, int* frame_rate_den, int* is_progressive)
{
    assert (width != NULL && height != NULL && frame_rate_num != NULL && frame_rate_den != NULL);
    CUVIDEOFORMAT oFormat;
    cuvidGetSourceVideoFormat(m_videoSource, &oFormat, 0);

    *width  = oFormat.coded_width;
    *height = oFormat.coded_height;
    *frame_rate_num = oFormat.frame_rate.numerator;
    *frame_rate_den = oFormat.frame_rate.denominator;
    *is_progressive = oFormat.progressive_sequence;
}*/


