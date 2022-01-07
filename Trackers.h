#pragma once

#include <Windows.h>

#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/dnn.hpp>

using namespace cv;

#include "Main.h"

enum FrameVariant
{
    UNKNOWN,
    GPU_RGBA,
    GPU_GREY,
    GPU_RGB,
    LOCAL_RGB,
    LOCAL_GREY
};

class FrameCache
{
public:

    void Clear();
    void Update(void* frame, FrameVariant v);
    void* GetVariant(FrameVariant v, cuda::Stream& stream = cuda::Stream::Null());

protected:
    struct someFrame
    {
        someFrame(bool isCpu)
            :isCpu(isCpu)
        {
        };

        cuda::GpuMat gpuFrame;
        Mat cpuFrame;

        bool isCpu = false;

        void* GetPtr()
        {
            if (isCpu)
                return &cpuFrame;
            else
                return &gpuFrame;
        }
    };

    map<FrameVariant, someFrame> cache;

    void* current = nullptr;
    FrameVariant currentType = FrameVariant::UNKNOWN;
};

class TrackerJT
{
public:
    TrackerJT(TrackingTarget& target, TrackingTarget::TrackingState& state, TRACKING_TYPE type, const char* name, FrameVariant frameType);

    void init();
    bool update(cuda::Stream& stream = cuda::Stream::Null());
    virtual const char* GetName()
    {
        return name;
    };

protected:
    virtual void initInternal(void* frame) = 0;
    virtual bool updateInternal(void* frame, cuda::Stream& stream = cuda::Stream::Null()) = 0;

    TRACKING_TYPE type;
    TrackingTarget::TrackingState& state;
    const char* name;
    FrameVariant frameType = FrameVariant::UNKNOWN;
};

class GpuTrackerGOTURN : public TrackerJT
{  
public:
    GpuTrackerGOTURN(TrackingTarget& target, TrackingTarget::TrackingState& state);

protected:

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;

    dnn::Net net;
    cuda::GpuMat image_;
};

class GpuTrackerPoints : public TrackerJT
{
public:
    GpuTrackerPoints(TrackingTarget& target, TrackingTarget::TrackingState& state);

protected:
    struct PointState
    {
        int cudaIndex;
        int cudaIndexNew;
    };

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;

    cuda::GpuMat image_;
    cuda::GpuMat points_;
    map<TrackingTarget::PointState*, PointState> pointStates;

    Ptr<cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;
};

class TrackerOpenCV : public TrackerJT
{
public:
    TrackerOpenCV(TrackingTarget& target, TrackingTarget::TrackingState& state, Ptr<Tracker> tracker, const char* trackerName);

    virtual const char* GetName();

protected:
    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;
    const char* trackerName;

    Ptr<Tracker> tracker;
};

extern FrameCache* FRAME_CACHE;