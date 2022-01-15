#pragma once

#include "Trackers.h"

#include <opencv2/tracking.hpp>

class TrackerOpenCV : public TrackerJT
{
public:
    TrackerOpenCV(TrackingTarget& target, TrackingStatus& state, cv::Ptr<cv::Tracker> tracker, const char* trackerName);

    virtual const char* GetName();

protected:
    void initCpu(cv::Mat frame) override;
    bool updateCpu(cv::Mat frame) override;
    const char* trackerName;

    cv::Ptr<cv::Tracker> tracker;
};