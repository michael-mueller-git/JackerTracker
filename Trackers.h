#pragma once

#include <Windows.h>

#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/dnn.hpp>
#include "magic_enum.hpp"

using namespace cv;
using namespace std;

#include "Main.h"

class TrackerJT;

struct TrackerJTStruct
{
    TrackerJTType type;
    TRACKING_TYPE trackingType;
    string name;
    function<TrackerJT* (TrackingTarget& target, TrackingState& state)> Create;
};

TrackerJTStruct GetTracker(TrackerJTType type);

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
    FrameVariant currentType = FrameVariant::VARIANT_UNKNOWN;
};

class TrackerJT
{
public:
    TrackerJT(TrackingTarget& target, TrackingState& state, TRACKING_TYPE type, const char* name, FrameVariant frameType);

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
    TrackingState& state;
    const char* name;
    FrameVariant frameType = FrameVariant::VARIANT_UNKNOWN;
};

class GpuTrackerGOTURN : public TrackerJT
{  
public:
    GpuTrackerGOTURN(TrackingTarget& target, TrackingState& state);

protected:

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;

    dnn::Net net;
    cuda::GpuMat image_;
};

class GpuTrackerPoints : public TrackerJT
{
public:
    GpuTrackerPoints(TrackingTarget& target, TrackingState& state);

protected:
    struct GpuPointState
    {
        int cudaIndex;
        int cudaIndexNew;
    };

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;

    cuda::GpuMat image_;
    cuda::GpuMat points_;
    map<PointState*, GpuPointState> pointStates;

    Ptr<cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;
};

class TrackerOpenCV : public TrackerJT
{
public:
    TrackerOpenCV(TrackingTarget& target, TrackingState& state, Ptr<Tracker> tracker, const char* trackerName);

    virtual const char* GetName();

protected:
    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cuda::Stream& stream) override;
    const char* trackerName;

    Ptr<Tracker> tracker;
};

extern FrameCache* FRAME_CACHE;