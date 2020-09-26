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
#include "VideoSource.h"
#include "VideoDecoder.h"
#include <assert.h>
#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgcodecs.hpp"

//一个大的结构体,包含ffmpeg各种参数
AVCodecContext	*pCodecCtx;
//创建包含了媒体流的格式信息的结构体
AVFormatContext	*pFormatCtx;
int	videoindex;

AVBitStreamFilterContext* h264bsfc = NULL;

//构造函数
VideoSource::VideoSource()
{

}

//析构函数
VideoSource::~VideoSource()
{
	
}

//初始化ffmpeg获取视频流
bool VideoSource::init(const std::string sFileName)
{

	int	i;
	AVCodec	*pCodec;

	//初始化ffmpeg的所有组件
	av_register_all();
	//打开网络摄像机需要初始化 network
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//打开摄像机视频流
	if (avformat_open_input(&pFormatCtx, sFileName.c_str(), NULL, NULL) != 0){
		printf("Couldn't open input stream.\n");
		return false;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL)<0){
		printf("Couldn't find stream information.\n");
		return false;
	}
	videoindex = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
		videoindex = i;
		break;
	}

	if (videoindex == -1){
		printf("Didn't find a video stream.\n");
		return false;
	}

	//摄像机rtsp流的格式信息
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	
	//获取FFmpeg的解码器
	/*pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		printf("Codec not found.\n");
		return false;
	}*/

	//打印rtsp流的基本信息
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, sFileName.c_str(), 0);
	printf("-------------------------------------------------\n");

	memset(&g_stFormat, 0, sizeof(CUVIDEOFORMAT));
	
	//配置cuda的解码参数
	switch (pCodecCtx->codec_id) {
	case AV_CODEC_ID_H263:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

	case AV_CODEC_ID_H264:
		g_stFormat.codec = cudaVideoCodec_H264;
		break;

	case AV_CODEC_ID_HEVC:
		g_stFormat.codec = cudaVideoCodec_HEVC;
		break;

	case AV_CODEC_ID_MJPEG:
		g_stFormat.codec = cudaVideoCodec_JPEG;
		break;

	case AV_CODEC_ID_MPEG1VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG1;
		break;

	case AV_CODEC_ID_MPEG2VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG2;
		break;

	case AV_CODEC_ID_MPEG4:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

	case AV_CODEC_ID_VP8:
		g_stFormat.codec = cudaVideoCodec_VP8;
		break;

	case AV_CODEC_ID_VP9:
		g_stFormat.codec = cudaVideoCodec_VP9;
		break;

	case AV_CODEC_ID_VC1:
		g_stFormat.codec = cudaVideoCodec_VC1;
		break;
	default:
		return false;
	}

	//设置cuda解码的输入流类型
	switch (pCodecCtx->sw_pix_fmt)
	{
	case AV_PIX_FMT_YUV420P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	case AV_PIX_FMT_YUV422P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_422;
		break;
	case AV_PIX_FMT_YUV444P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_444;
		break;
	default:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	}

	//以下这一段是参考别人的代码,目前还不太清楚作用
	switch (pCodecCtx->field_order)
	{
	case AV_FIELD_PROGRESSIVE:
	case AV_FIELD_UNKNOWN:
		g_stFormat.progressive_sequence = true;
		break;
	default:
		g_stFormat.progressive_sequence = false;
		break;
	}

	pCodecCtx->thread_safe_callbacks = 1;

	g_stFormat.coded_width = pCodecCtx->coded_width;
	g_stFormat.coded_height = pCodecCtx->coded_height;

	g_stFormat.display_area.right = pCodecCtx->width;
	g_stFormat.display_area.left = 0;
	g_stFormat.display_area.bottom = pCodecCtx->height;
	g_stFormat.display_area.top = 0;

	if (pCodecCtx->codec_id == AV_CODEC_ID_H264 || pCodecCtx->codec_id == AV_CODEC_ID_HEVC) {
		if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
			h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
		else
			h264bsfc = av_bitstream_filter_init("hevc_mp4toannexb");
	}

	return true;
}

/*CUVIDEOFORMAT VideoSource::format()
{
	return g_stFormat;
}*/

/*void VideoSource::getSourceDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();

    width  = rCudaVideoFormat.coded_width;
    height = rCudaVideoFormat.coded_height;
}*/

/*void VideoSource::getDisplayDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();
pFormatCtx
    width  = abs(rCudaVideoFormat.display_area.right  - rCudaVideoFormat.display_area.left);
    height = abs(rCudaVideoFormat.display_area.bottom - rCudaVideoFormat.display_area.top);
}*/

/*void VideoSource::getProgressive(bool &progressive)
{
	CUVIDEOFORMAT rCudaVideoFormat = format();
	progressive = rCudaVideoFormat.progressive_sequence ? true : false;
}*/


int iPkt = 0;

bool VideoSource::run()
{
	void* hHandleDriver = nullptr;
	//Initialize the CUDA driver
	__cu(cuInit(0, __CUDA_API_VERSION, hHandleDriver));
	__cu(cuvidInit(0));

	//cuda上下文
    CUcontext cudaCtx;
	//设置cuda设备的id
    CUdevice device = 0;
	int deviceID = 0;

	__cu(cuDeviceGet(&device, deviceID));
    __cu(cuCtxCreate(&cudaCtx, CU_CTX_SCHED_AUTO, device));

 	CUcontext curCtx;
    CUvideoctxlock ctxLock;
    __cu(cuCtxPopCurrent(&curCtx));
	//上下文锁定
    __cu(cuvidCtxLockCreate(&ctxLock, curCtx));
    CudaDecoder* pDecoder   = new CudaDecoder;
    FrameQueue* pFrameQueue = new CUVIDFrameQueue(ctxLock);

	pDecoder->InitVideoDecoder(ctxLock, pFrameQueue, 1920, 1088);
	//pDecoder->m_videoParser

	/*---------------------------------------------------------------------------*/
	CUdeviceptr  pDecodedFrame = 0;
	unsigned char *pFrameBufferYUV[1] = {0};

	unsigned int nDecodedPitch = 0;
    unsigned int nWidth = 0;
    unsigned int nHeight = 0;

	/*---------------------------------------------------------------------------*/
	AVPacket *avpkt;
	avpkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	CUVIDSOURCEDATAPACKET cupkt;
	int iPkt = 0;
	CUresult oResult;
	//取rtsp流放到avpkt
	while (av_read_frame(pFormatCtx, avpkt) >= 0){
		if (avpkt->stream_index == videoindex){

			if (avpkt && avpkt->size) {
				if (h264bsfc)
				{
					av_bitstream_filter_filter(h264bsfc, pFormatCtx->streams[videoindex]->codec, NULL, &avpkt->data, &avpkt->size, avpkt->data, avpkt->size, 0);
				}

				//将avptk的内容转换到cupkt
				cupkt.payload_size = (unsigned long)avpkt->size;
				cupkt.payload = (const unsigned char*)avpkt->data;

				if (avpkt->pts != AV_NOPTS_VALUE) {
					cupkt.flags = CUVID_PKT_TIMESTAMP;
					if (pCodecCtx->pkt_timebase.num && pCodecCtx->pkt_timebase.den){
						AVRational tb;
						tb.num = 1;
						tb.den = AV_TIME_BASE;
						cupkt.timestamp = av_rescale_q(avpkt->pts, pCodecCtx->pkt_timebase, tb);
					}
					else
						cupkt.timestamp = avpkt->pts;
				}
			}
			else {
				cupkt.flags = CUVID_PKT_ENDOFSTREAM;
			}
			//解码cupkt里的数据流
			oResult = cuvidParseVideoData(pDecoder->m_videoParser, &cupkt);
			if ((cupkt.flags & CUVID_PKT_ENDOFSTREAM) || (oResult != CUDA_SUCCESS)){
				std::cout << "break" << std::endl;
				break;
			}
			iPkt++;

		}

		/*-----------------------------------------------------------------------------------*/
		if (!(pDecoder->m_pFrameQueue->isEndOfDecode() && pDecoder->m_pFrameQueue->isEmpty())){
			CUVIDPARSERDISPINFO pData;
			if (pDecoder->m_pFrameQueue->dequeue(&pData)){
				CCtxAutoLock lck(ctxLock);
				//解耦的context之后可被其他thread设置为current current ????????
				cuCtxPushCurrent(cudaCtx);
				CUVIDPROCPARAMS oVideoProcessingParameters;
    			memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));

				oVideoProcessingParameters.progressive_frame = pData.progressive_frame;
        		oVideoProcessingParameters.top_field_first = pData.top_field_first;
        		oVideoProcessingParameters.unpaired_field = (pData.repeat_first_field < 0);
        		oVideoProcessingParameters.second_field = 0;

				//获取数据在GPU中的地址dMappedFrame，大小为pitch个
				if(pDecoder->mapFrame(pData.picture_index, &pDecodedFrame, &nDecodedPitch, &oVideoProcessingParameters)){
					unsigned int nv12_size = nDecodedPitch * (pDecoder->tHeight + pDecoder->tHeight/2);
					
					//将显存转换到内存
					oResult = cuMemAllocHost((void **)&pFrameBufferYUV[0], nv12_size);
					oResult = cuMemcpyDtoH(pFrameBufferYUV[0], pDecodedFrame, nv12_size);  

					cv::Mat img = cv::Mat::zeros(pDecoder->tHeight, pDecoder->tWidth, CV_8UC3);
					//yuv420格式的buffer转为BGR
					pDecoder->YUV420P2BGR32(pFrameBufferYUV[0], (unsigned char *)img.data, nDecodedPitch, pDecoder->tHeight, pDecoder->tWidth);
					//cv::imwrite("./1.jpg", img);
					cv::imshow("test", img);
            		if(cv::waitKey(1) == 27){
                		cv::destroyAllWindows();
                		break;
            		}

					//取消映射
					cuvidUnmapVideoFrame(pDecoder->GetDecoder(), pDecodedFrame);
					//释放队列frame
					pDecoder->m_pFrameQueue->releaseFrame(&pData);
					//释放Mat
					img.release();
				}else{
					cuvidUnmapVideoFrame(pDecoder->GetDecoder(), pDecodedFrame);
					pDecoder->m_pFrameQueue->releaseFrame(&pData);
					//将当前host thread的current context解耦成为float context ????????
					cuCtxPopCurrent(NULL);
				}

			
			}
		}

		av_free_packet(avpkt);
	}

}


