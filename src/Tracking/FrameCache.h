#pragma once

#include "Model/Model.h"

#include <opencv2/core/cuda.hpp>
#include <map>
#include <deque>

class FrameCache
{
public:
    // From should always be FrameVariant::GPU_RGBA !

    cv::Mat CpuVariant(cv::cuda::GpuMat from, FrameVariant to, cv::cuda::Stream& stream = cv::cuda::Stream::Null());
    cv::cuda::GpuMat GpuVariant(cv::cuda::GpuMat from, FrameVariant to, cv::cuda::Stream& stream = cv::cuda::Stream::Null());

    bool IsCpu(FrameVariant v);

protected:
    struct CacheRecord
    {
        CacheRecord(bool isCpu, uint cudaPtr, FrameVariant variant)
            :isCpu(isCpu), cudaPtr(cudaPtr), variant(variant)
        {
        };

        uint cudaPtr;

        cv::cuda::GpuMat gpuFrame;
        cv::Mat cpuFrame;

        FrameVariant variant;
        int reads = 0;

        bool isCpu = false;

        cv::cuda::GpuMat GetGpu()
        {
            assert(!isCpu);
            reads++;
            return gpuFrame;
        }

        cv::Mat GetCpu()
        {
            assert(isCpu);
            reads++;
            return cpuFrame;
        }
    };

    void Store(CacheRecord& r);

    std::deque<CacheRecord>::iterator FindCache(cv::cuda::GpuMat frame, FrameVariant v);

    std::mutex cacheMtx;
    std::deque<CacheRecord> cache;
};

extern FrameCache* FRAME_CACHE;