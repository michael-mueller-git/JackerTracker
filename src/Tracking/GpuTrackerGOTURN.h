#pragma once

#include "Trackers.h"

#include <opencv2/dnn/dnn.hpp>

class GpuTrackerGOTURN : public TrackerJT
{
public:
    GpuTrackerGOTURN(TrackingTarget& target, TrackingStatus& state);

protected:

    void initGpu(cv::cuda::GpuMat frame) override;
    bool updateGpu(cv::cuda::GpuMat frame, cv::cuda::Stream& stream) override;

    cv::dnn::Net net;
    cv::cuda::GpuMat image_;
};