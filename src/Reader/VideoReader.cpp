#include "VideoReader.h"

#include "driver_types.h"
#include <cuda.h>

#include "NvDecoder.h"
#include "NvCodecUtils.h"
#include "FFmpegDemuxer.h"
#include "Logger.h"

#include <opencv2/core/cuda_stream_accessor.hpp>

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

protected:
    std::string fileName;
    FFmpegDemuxer demuxer;
    NvDecoder* dec;
    int64_t lastPts;
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
    delete dec;
}

cv::cuda::GpuMat VideoReaderImp::NextFrame(cv::cuda::Stream& stream)
{
    if (dec->NumFrames())
        return dec->GetFrame();

    int nVideoBytes = 0, nFrameReturned = 0;
    int64_t pts = 0;

    uint8_t* pVideo = NULL;
    int tries = 0;

    while (tries < 10)
    {
        demuxer.Demux(&pVideo, &nVideoBytes, &pts);
        nFrameReturned = dec->Decode(pVideo, nVideoBytes, 0, pts, cv::cuda::StreamAccessor::getStream(stream));

        if (nFrameReturned)
        {
            break;
        }

        tries++;
    }

    if (dec->NumFrames() == 0)
        throw "Reading failed";

    lastPts = pts;

    return dec->GetFrame();
}

bool VideoReaderImp::Seek(unsigned long time)
{
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