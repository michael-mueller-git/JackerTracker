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
    TrackerJT(TrackingTarget& target, TrackingTarget::TrackingState& state, TRACKING_TYPE type, const char* name);

    void init(cuda::GpuMat image);
    bool update(cuda::GpuMat image);
    virtual const char* GetName()
    {
        return name;
    };

protected:
    virtual void initInternal(cuda::GpuMat image) = 0;
    virtual bool updateInternal(cuda::GpuMat image) = 0;

    TRACKING_TYPE type;
    TrackingTarget::TrackingState& state;
    const char* name;
};

class GpuTrackerGOTURN : public TrackerJT
{  
public:
    GpuTrackerGOTURN(TrackingTarget& target, TrackingTarget::TrackingState& state);

protected:

    void initInternal(cuda::GpuMat image) override;
    bool updateInternal(cuda::GpuMat image) override;

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

    void initInternal(cuda::GpuMat image) override;
    bool updateInternal(cuda::GpuMat image) override;

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
    void initInternal(cuda::GpuMat image) override;
    bool updateInternal(cuda::GpuMat image) override;
    const char* trackerName;

    Ptr<Tracker> tracker;
};