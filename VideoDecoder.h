#ifndef _VIDEO_DECODER
#define _VIDEO_DECODER

#include "dynlink_nvcuvid.h" // <nvcuvid.h>
#include "dynlink_cuda.h"    // <cuda.h>
#include "FrameQueue.h"

class CudaDecoder
{
public:
    CudaDecoder();
    virtual ~CudaDecoder(void);

    bool IsFinished()            { return m_bFinish; }
    virtual void InitVideoDecoder(CUvideoctxlock ctxLock, FrameQueue* pFrameQueue,
            int targetWidth = 0, int targetHeight = 0);
    //virtual void Start();
    //virtual void GetCodecParam(int* width, int* height, int* frame_rate_num, int* frame_rate_den, int* is_progressive);
    virtual void* GetDecoder()   { return m_videoDecoder; }
    
    //add
    bool mapFrame(int iPictureIndex, CUdeviceptr *ppDevice, unsigned int *nPitch, CUVIDPROCPARAMS *pVideoProcessingParameters);
    void YUV420P2BGR32(unsigned char *yuv_buffer_in, unsigned char *rgb_buffer_out, unsigned int pitch, int width, int height);
    int tWidth;
    int tHeight;
    
    CUdeviceptr g_pInteropFrame{0};
    
    


public:
    CUvideosource  m_videoSource;
    CUvideoparser  m_videoParser;
    CUvideodecoder m_videoDecoder;
    CUvideoctxlock m_ctxLock;
    CUVIDDECODECREATEINFO m_oVideoDecodeCreateInfo;

    FrameQueue*    m_pFrameQueue;
    int            m_decodedFrames;

protected:
    bool m_bFinish;
};

#endif
