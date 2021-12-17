#include "VideoSource.h"
#include <opencv2/core/utils/logger.hpp>
#include "opencv2/videoio/registry.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

using namespace cv;
using namespace cv::cudacodec;

FormatInfo MyVideoSource::format() const
{
    return format_;
}

void MyVideoSource::updateFormat(const int codedWidth, const int codedHeight)
{
    format_.width = codedWidth;
    format_.height = codedHeight;
    format_.valid = true;
}

AVAudioFifo* MyVideoSource::GetAudioFifo() 
{
    return audioFifo; 
};

int MyVideoSource::GetNumAudioChannels()
{
    return audioStream->codecpar->channels;
}

MyVideoSource::MyVideoSource(const String& fname)
{
    int ret = avformat_open_input(&formatCtx, fname.c_str(), nullptr, nullptr);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV open failed"));
        return;
    }

    ret = avformat_find_stream_info(formatCtx, nullptr);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV stream info failed"));
        return;
    }

    int videoIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIndex < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV no video found"));
        return;
    }

    int audioIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIndex < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV no audio found"));
        return;
    }

    

    // Audio stream

    audioStream = formatCtx->streams[audioIndex];

    audioDecoder = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!audioDecoder)
    {
        CV_LOG_WARNING(NULL, cv::format("AV audio decoder not found"));
        return;
    }

    CV_LOG_WARNING(NULL, cv::format("AV audio decoder: %s", audioDecoder->name));

    audioCodecCtx = avcodec_alloc_context3(audioDecoder);
    avcodec_parameters_to_context(audioCodecCtx, audioStream->codecpar);
    ret = avcodec_open2(audioCodecCtx, audioDecoder, nullptr);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV audio decoder open failed"));
        return;
    }
    
    audioResampler = swr_alloc_set_opts(
        nullptr,
        audioStream->codecpar->channel_layout,
        AV_SAMPLE_FMT_FLT,
        audioStream->codecpar->sample_rate,
        audioStream->codecpar->channel_layout,
        (AVSampleFormat)audioStream->codecpar->format,
        audioStream->codecpar->sample_rate,
        0,
        nullptr
    );

    audioFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, audioStream->codecpar->channels, 1);

    // Video stream

    videoStream = formatCtx->streams[videoIndex];
    const AVBitStreamFilter* videoBitStreamFilter = av_bsf_get_by_name("h264_mp4toannexb");
    ret = av_bsf_alloc(videoBitStreamFilter, &videoBsf);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV Bitstream alloc failed"));
        return;
    }

    avcodec_parameters_copy(videoBsf->par_in, videoStream->codecpar);
    
    ret = av_bsf_init(videoBsf);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV Bitstream init failed"));
        return;
    }


    
    /*
    //videoDecoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
    videoDecoder = avcodec_find_decoder_by_name("h264_cuvid");
    if (!videoDecoder)
    {
        CV_LOG_WARNING(NULL, cv::format("AV video decoder not found"));
        return;
    }

    CV_LOG_WARNING(NULL, cv::format("AV video decoder: %s", videoDecoder->name));

    videoCodecCtx = avcodec_alloc_context3(videoDecoder);
    avcodec_parameters_to_context(videoCodecCtx, videoStream->codecpar);
    ret = avcodec_open2(videoCodecCtx, videoDecoder, nullptr);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV video decoder open failed"));
        return;
    }
    */

    avPacket = av_packet_alloc();
    audioFrame = av_frame_alloc();


    format_.codec = H264;
    format_.height = videoStream->codecpar->height;
    format_.width = videoStream->codecpar->width;
    format_.displayArea = Rect(0, 0, format_.width, format_.height);
    format_.valid = false;
    format_.chromaFormat = YUV420;
    format_.nBitDepthMinus8 = 0;
}

bool MyVideoSource::InitAudioDevice(ma_device& audioDevice, ma_device_callback_proc audioCallback, void* userData)
{
    ma_device_config audioDeviceConfig = ma_device_config_init(ma_device_type_playback);
    audioDeviceConfig.playback.format = ma_format_f32;
    audioDeviceConfig.playback.channels = audioStream->codecpar->channels;
    audioDeviceConfig.sampleRate = audioStream->codecpar->sample_rate;
    audioDeviceConfig.dataCallback = audioCallback;
    audioDeviceConfig.pUserData = userData;

    if (ma_device_init(NULL, &audioDeviceConfig, &audioDevice) != MA_SUCCESS) {
        CV_LOG_WARNING(NULL, cv::format("MA open failed"));
        return false;
    }

    if (ma_device_start(&audioDevice) != MA_SUCCESS) {
        CV_LOG_WARNING(NULL, cv::format("MA start failed"));
        ma_device_uninit(&audioDevice);
        return false;
    }

    return true;
}

bool MyVideoSource::Seek(unsigned long timeMs)
{
    // Seek start position
    int frameNum = av_rescale(
        timeMs,
        videoStream->time_base.den,
        videoStream->time_base.num
    ) / 1000;

    int ret = avformat_seek_file(formatCtx, videoStream->index, 0, frameNum, frameNum, AVSEEK_FLAG_FRAME);
    if (ret < 0)
    {
        CV_LOG_WARNING(NULL, cv::format("AV seek failed"));
        return false;
    }

    lastPacketTime = timeMs;

    return true;
}

unsigned long MyVideoSource::GetPosition()
{
    return lastPacketTime;
}

unsigned long MyVideoSource::GetDuration()
{
    double time_base = (double)videoStream->time_base.num / (double)videoStream->time_base.den;
    unsigned long duration = (double)videoStream->duration * time_base * 1000.0;

    return duration;
}

MyVideoSource::~MyVideoSource()
{
    avformat_close_input(&formatCtx);
    av_packet_free(&avPacket);
    
    av_frame_free(&audioFrame);
    avcodec_free_context(&audioCodecCtx);

    av_audio_fifo_free(audioFifo);
    swr_free(&audioResampler);
}

bool MyVideoSource::getNextPacket(unsigned char** data, size_t* size)
{
    do
    {
        int ret = av_read_frame(formatCtx, avPacket);
        if (ret != 0)
            continue;

        if (avPacket->stream_index == audioStream->index)
        {
            // Decode audio
            ret = avcodec_send_packet(audioCodecCtx, avPacket);
            if (ret < 0)
            {
                CV_LOG_WARNING(NULL, cv::format("AV decode audio frame failed"));
                continue;
            }

            while (ret == 0)
            {
                ret = avcodec_receive_frame(audioCodecCtx, audioFrame);

                AVFrame* avFrameRespampled = av_frame_alloc();
                avFrameRespampled->sample_rate = audioFrame->sample_rate;
                avFrameRespampled->channel_layout = audioFrame->channel_layout;
                avFrameRespampled->channels = audioFrame->channels;
                avFrameRespampled->format = AV_SAMPLE_FMT_FLT;

                ret = swr_convert_frame(audioResampler, avFrameRespampled, audioFrame);
                av_frame_unref(audioFrame);
                av_audio_fifo_write(audioFifo, (void**)avFrameRespampled->data, avFrameRespampled->nb_samples);
                av_frame_free(&avFrameRespampled);

                if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
            }

            continue;
        }

        if (avPacket->stream_index == videoStream->index)
        {
            ret = av_bsf_send_packet(videoBsf, avPacket);
            if (ret != 0)
            {
                CV_LOG_WARNING(NULL, cv::format("AV bfs send failed"));
            }

            ret = av_bsf_receive_packet(videoBsf, avPacket);
            if (ret != 0)
            {
                CV_LOG_WARNING(NULL, cv::format("AV bfs receive failed"));
            }

            *data = avPacket->data;
            *size = avPacket->size;

            if (avPacket->pts > 0)
            {
                double time_base = (double)videoStream->time_base.num / (double)videoStream->time_base.den;
                double time = (double)avPacket->pts * time_base * 1000.0;

                lastPacketTime = time;
            }

            return true;
        }
    } while (true);

    CV_LOG_WARNING(NULL, cv::format("AV loop end"));
    return false;
}