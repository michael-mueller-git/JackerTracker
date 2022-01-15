#include "VideoReader.h"

#include "NvDecoder.h"
#include "NvCodecUtils.h"
#include "FFmpegDemuxer.h"
#include "Logger.h"

#include <driver_types.h>
#include <cuda.h>
#include <opencv2/core/cuda_stream_accessor.hpp>
#include <chrono>

using namespace std;
using namespace chrono;

#if !WIN32
#ifndef high_resolution_clock
#define high_resolution_clock steady_clock
#endif
#endif

simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

class VideoReaderImp : public VideoReader
{
public:
    VideoReaderImp(std::string fileName);
    ~VideoReaderImp();

    cv::cuda::GpuMat NextFrame(cv::cuda::Stream& stream);
    bool Seek(unsigned long time);
    unsigned long GetPosition();
    unsigned long GetDuration();
    void RunThread();

protected:
    string fileName;
    FFmpegDemuxer demuxer;
    
    mutex decMtx;
    NvDecoder* dec;

    int64_t lastPts;
    thread readThread;
    bool running = false;
    bool err = false;
};

VideoReaderImp::VideoReaderImp(std::string fileName)
    :fileName(fileName), demuxer(fileName.c_str())
{
    // init context
    cv::cuda::GpuMat temp(1, 1, CV_8UC1);
    temp.release();

    Rect cropRect = {};
    Dim resizeDim = {};

    dec = new NvDecoder(true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
    dec->SetOperatingPoint(0, false);
}

VideoReaderImp::~VideoReaderImp()
{
    running = false;
    if (readThread.joinable())
        readThread.join();

    delete dec;
}

void VideoReaderImp::RunThread()
{
    while (running)
    {
        int nVideoBytes = 0, nFrameReturned = 0;
        int64_t pts = 0;

        uint8_t* pVideo = NULL;
        int tries = 0;

        while (tries < 10)
        {
            {
                lock_guard<mutex> lock(decMtx);
                demuxer.Demux(&pVideo, &nVideoBytes, &pts);
                nFrameReturned = dec->Decode(pVideo, nVideoBytes, 0, pts);
            }

            if (nFrameReturned)
            {
                break;
            }

            tries++;
        }

        if (nFrameReturned == 0)
        {
            err = true;
            running = false;
        }

        if (dec->NumFrames() > 60)
            running = false;

    }
}

cv::cuda::GpuMat VideoReaderImp::NextFrame(cv::cuda::Stream& stream)
{
    if (dec->NumFrames() < 30)
    {
        if (!running)
        {
            if (readThread.joinable())
                readThread.join();

            running = true;
            readThread = std::thread(&VideoReaderImp::RunThread, this);
        }
    }

    auto now = steady_clock::now();

    do {
        if (dec->NumFrames() > 0)
        {
            lock_guard<mutex> lock(decMtx);
            return dec->GetFrame(&lastPts);
        }
    } while (duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count() < 1000);

    throw "Reading failed";
}

bool VideoReaderImp::Seek(unsigned long time)
{
    lock_guard<mutex> lock(decMtx);

    dec->Flush();

    return demuxer.Seek(time);
}

unsigned long VideoReaderImp::GetPosition()
{
    return lastPts;
}

unsigned long VideoReaderImp::GetDuration()
{
    return demuxer.GetDuration();
}

cv::Ptr<VideoReader> VideoReader::create(std::string fileName)
{
    return cv::makePtr<VideoReaderImp>(fileName);
}
