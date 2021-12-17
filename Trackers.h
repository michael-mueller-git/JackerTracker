#pragma once

#include <Windows.h>

#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/dnn.hpp>

using namespace cv;

#include "Main.h"

class TrackerJT
{
public:
    TrackerJT(TrackingTarget& target, TRACKING_TYPE type);

    void init(cuda::GpuMat image);
    bool update(cuda::GpuMat image);

protected:
    virtual void initInternal(cuda::GpuMat image);
    virtual bool updateInternal(cuda::GpuMat image);

    TRACKING_TYPE type;
    TrackingTarget::TrackingState& state;
};

class GpuTrackerGOTURN : public TrackerJT
{  
public:
    GpuTrackerGOTURN(TrackingTarget& target);

protected:

    void initInternal(cuda::GpuMat image) override;
    bool updateInternal(cuda::GpuMat image) override;

    dnn::Net net;
    cuda::GpuMat image_;
};



class GpuTrackerPoints : public TrackerJT
{
public:
    GpuTrackerPoints(TrackingTarget& target);

protected:
    struct PointState
    {
        Point2f lastPosition;
        int cudaIndex;
        int cudaIndexNew;
    };

    void initInternal(cuda::GpuMat image) override;
    bool updateInternal(cuda::GpuMat image) override;

    cuda::GpuMat image_;
    cuda::GpuMat points_;
    map<TrackingTarget::PointState*, PointState> pointStates;

    Ptr<cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;
};