#pragma once

#include <Windows.h>

#include "opencv2/opencv.hpp"
#include "opencv2/cudacodec.hpp"
using namespace cv;

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavutil/avutil.h"
	#include "libswresample/swresample.h"
	#include "libavutil/audio_fifo.h"
}

#include "miniaudio.h"

class MyVideoSource : public cudacodec::RawVideoSource
{
public:
	MyVideoSource(const String& fname);
	~MyVideoSource();

	bool getNextPacket(unsigned char** data, size_t* size) CV_OVERRIDE;
	bool Seek(unsigned long timeMs);
	unsigned long GetPosition();
	unsigned long GetDuration();
	
	AVAudioFifo* GetAudioFifo();
	int GetNumAudioChannels();
	bool InitAudioDevice(ma_device& audioDevice, ma_device_callback_proc audioCallback, void* userData);

	cudacodec::FormatInfo format() const CV_OVERRIDE;
	void updateFormat(const int codedWidth, const int codedHeight);

private:
	unsigned long lastPacketTime = 0;

	cudacodec::FormatInfo format_;

	AVFormatContext* formatCtx = nullptr;
	AVPacket* avPacket = nullptr;
	
	AVStream* videoStream = nullptr;
	AVBSFContext* videoBsf = nullptr;

	AVStream* audioStream = nullptr;
	AVCodec* audioDecoder = nullptr;
	AVCodecContext* audioCodecCtx = nullptr;
	AVFrame* audioFrame = nullptr;
	SwrContext* audioResampler = nullptr;
	AVAudioFifo* audioFifo = nullptr;
};
