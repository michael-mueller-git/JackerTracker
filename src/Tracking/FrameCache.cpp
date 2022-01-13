#include "FrameCache.h"

#include <opencv2/cudaimgproc.hpp>

using namespace cv;
using namespace std;

FrameCache* FRAME_CACHE = new FrameCache();

bool FrameCache::IsCpu(FrameVariant v)
{
    switch (v)
    {
    case LOCAL_RGB:
    case LOCAL_GREY:
        return true;
    default:
        return false;
    }
}

deque<FrameCache::CacheRecord>::iterator FrameCache::FindCache(cv::cuda::GpuMat frame, FrameVariant v)
{
    uint cudaPtr = *frame.ptr<uint>();

    auto found = find_if(cache.begin(), cache.end(), [cudaPtr, v](auto& c) {
        return c.cudaPtr == cudaPtr && c.variant == v;
    });

    return found;
}

Mat FrameCache::CpuVariant(cv::cuda::GpuMat from, FrameVariant to, cv::cuda::Stream& stream)
{
    {
        lock_guard<mutex> lock(cacheMtx);

        auto f = FindCache(from, to);
        if (f != cache.end())
            return f->GetCpu();
    }


    CacheRecord r(true, *from.ptr<uint>(), to);
    cuda::GpuMat buffer;

    switch (to) {
    case FrameVariant::LOCAL_RGB:
        buffer = GpuVariant(from, FrameVariant::GPU_RGB, stream);
        buffer.download(r.cpuFrame);
        break;
    case FrameVariant::LOCAL_GREY:
        buffer = GpuVariant(from, FrameVariant::GPU_GREY, stream);
        buffer.download(r.cpuFrame);
    default:
        throw "Failed";
    }

    Store(r);
    return r.GetCpu();
}

cuda::GpuMat FrameCache::GpuVariant(cv::cuda::GpuMat from, FrameVariant to, cv::cuda::Stream& stream)
{
    if (to == GPU_RGBA)
        return from;

    {
        lock_guard<mutex> lock(cacheMtx);

        auto f = FindCache(from, to);
        if (f != cache.end())
            return f->GetGpu();
    }


    CacheRecord r(false, *from.ptr<uint>(), to);
    cuda::GpuMat buffer;

    switch (to) {
    case FrameVariant::GPU_RGB:
        cuda::cvtColor(from, r.gpuFrame, COLOR_BGRA2BGR, 0, stream);
        break;
    case FrameVariant::GPU_GREY:
        cuda::cvtColor(from, r.gpuFrame, COLOR_BGRA2GRAY, 0, stream);
    default:
        throw "Failed";
    }

    Store(r);
    return r.GetGpu();
}

void FrameCache::Store(CacheRecord& r)
{
    lock_guard<mutex> lock(cacheMtx);
    cache.push_front(r);
    
    if (cache.size() > 10)
    {
        cache.pop_back();
    }
}