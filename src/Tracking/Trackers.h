#pragma once

#include "Model/Model.h"
#include "Model/TrackingTarget.h"
#include "Model/TrackingStatus.h"

#include <string>
#include <functional>
#include <map>
#include <opencv2/core/cuda.hpp>
#include <opencv2/dnn/dnn.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/tracking.hpp>

class TrackerJT;

struct TrackerJTStruct
{
    TrackerJTType type;
    TRACKING_TYPE trackingType;
    std::string name;
    std::function<TrackerJT* (TrackingTarget& target, TrackingStatus& state)> Create;
};

TrackerJTStruct GetTracker(TrackerJTType type);

class FrameCache
{
public:

    void Clear();
    void Update(void* frame, FrameVariant v);
    void* GetVariant(FrameVariant v, cv::cuda::Stream& stream = cv::cuda::Stream::Null());

protected:
    struct someFrame
    {
        someFrame(bool isCpu)
            :isCpu(isCpu)
        {
        };

        cv::cuda::GpuMat gpuFrame;
        cv::Mat cpuFrame;

        bool isCpu = false;

        void* GetPtr()
        {
            if (isCpu)
                return &cpuFrame;
            else
                return &gpuFrame;
        }
    };

    std::map<FrameVariant, someFrame> cache;

    void* current = nullptr;
    FrameVariant currentType = FrameVariant::VARIANT_UNKNOWN;
};

class TrackerJT
{
public:
    TrackerJT(TrackingTarget& target, TrackingStatus& state, TRACKING_TYPE type, const char* name, FrameVariant frameType);

    void init();
    bool update(cv::cuda::Stream& stream = cv::cuda::Stream::Null());
    virtual const char* GetName()
    {
        return name;
    };

protected:
    virtual void initInternal(void* frame) = 0;
    virtual bool updateInternal(void* frame, cv::cuda::Stream& stream = cv::cuda::Stream::Null()) = 0;

    TRACKING_TYPE type;
    TrackingStatus& state;
    const char* name;
    FrameVariant frameType = FrameVariant::VARIANT_UNKNOWN;
    cv::Rect window;
};

class GpuTrackerGOTURN : public TrackerJT
{  
public:
    GpuTrackerGOTURN(TrackingTarget& target, TrackingStatus& state);

protected:

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cv::cuda::Stream& stream) override;

    cv::dnn::Net net;
    cv::cuda::GpuMat image_;
};

class GpuTrackerPoints : public TrackerJT
{
public:
    GpuTrackerPoints(TrackingTarget& target, TrackingStatus& state);

protected:
    struct GpuPointState
    {
        int cudaIndex;
        int cudaIndexNew;
    };

    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cv::cuda::Stream& stream) override;

    cv::cuda::GpuMat image_;
    cv::cuda::GpuMat points_;
    std::map<PointState*, GpuPointState> pointStates;

    cv::Ptr<cv::cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;
};

class TrackerOpenCV : public TrackerJT
{
public:
    TrackerOpenCV(TrackingTarget& target, TrackingStatus& state, cv::Ptr<cv::Tracker> tracker, const char* trackerName);

    virtual const char* GetName();

protected:
    void initInternal(void* frame) override;
    bool updateInternal(void* frame, cv::cuda::Stream& stream) override;
    const char* trackerName;

    cv::Ptr<cv::Tracker> tracker;
};

extern FrameCache* FRAME_CACHE;