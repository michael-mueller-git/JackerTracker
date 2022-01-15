#pragma once

#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <string>

class VideoReader
{
public:
    VideoReader() {};

    virtual cv::cuda::GpuMat NextFrame(cv::cuda::Stream& stream = cv::cuda::Stream::Null()) = 0;
    virtual bool Seek(unsigned long time) = 0;
    virtual unsigned long GetPosition() = 0;
    virtual unsigned long GetDuration() = 0;

    static cv::Ptr<VideoReader> create(std::string fileName);
};