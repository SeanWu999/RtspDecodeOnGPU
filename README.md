# RtspDecodeOnGPU
使用nvidia GPU 硬解码rtsp摄像机流，主要使用了cuvid和ffmpeg。
由于cuvid的解码只能针对本地视频，无法获取网络摄像头，所以使用ffmpeg抓流，再结合cuvid硬解码
