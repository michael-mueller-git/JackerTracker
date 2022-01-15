#pragma once

#include "Trackers.h"

#include <opencv2/cudaoptflow.hpp>
#include <map>

class GpuTrackerPoints : public TrackerJT
{
public:
    struct Params {
        Params(){};
        int size = 10;
        int maxLevel = 3;
        int iters = 30;
    };

    GpuTrackerPoints(TrackingTarget& target, TrackingStatus& state, Params params = Params());

protected:
    struct GpuPointState
    {
        int cudaIndex;
        int cudaIndexNew;
    };

    void initGpu(cv::cuda::GpuMat frame) override;
    bool updateGpu(cv::cuda::GpuMat frame, cv::cuda::Stream& stream) override;

    cv::cuda::GpuMat image_;
    cv::cuda::GpuMat points_;
    std::map<PointState*, GpuPointState> pointStates;
    Params params;

    cv::Ptr<cv::cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;
};
