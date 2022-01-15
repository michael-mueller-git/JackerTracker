#pragma once

#include "Model/Model.h"
#include "Model/TrackingTarget.h"
#include "Model/TrackingStatus.h"

#include <string>
#include <functional>
#include <opencv2/core/cuda.hpp>

class TrackerJT;

struct TrackerJTStruct
{
    TrackerJTType type;
    TRACKING_TYPE trackingType;
    std::string name;
    std::function<TrackerJT* (TrackingTarget& target, TrackingStatus& state)> Create;
};

TrackerJTStruct GetTracker(TrackerJTType type);


class TrackerJT
{
public:
    TrackerJT(TrackingTarget& target, TrackingStatus& state, TRACKING_TYPE type, const char* name, FrameVariant frameType);

    void init(cv::cuda::GpuMat frame);
    bool update(cv::cuda::GpuMat frame, cv::cuda::Stream& stream = cv::cuda::Stream::Null());
    virtual const char* GetName()
    {
        return name;
    };

protected:
    virtual void initCpu(cv::Mat frame) { throw "Not implemented"; };
    virtual bool updateCpu(cv::Mat frame) { throw "Not implemented"; };

    virtual void initGpu(cv::cuda::GpuMat frame) { throw "Not implemented"; };
    virtual bool updateGpu(cv::cuda::GpuMat, cv::cuda::Stream& stream = cv::cuda::Stream::Null()) { throw "Not implemented"; };

    TRACKING_TYPE type;
    TrackingStatus& state;
    const char* name;
    FrameVariant frameType = FrameVariant::VARIANT_UNKNOWN;
    cv::Rect window;
    bool isCpu = true;
};