#pragma once

#include "Model.h"
#include "TrackingTarget.h"
#include "Main.h"

#include <opencv2/core.hpp>
#include <vector>

class TrackingStatusBase
{
public:
	TrackingStatusBase(TRACKING_TYPE trackingType, TARGET_TYPE targetType)
		:trackingType(trackingType), targetType(targetType)
	{

	}

	void UpdateColor(int index)
	{
		color = PickColor(index);
	}

	void Draw(cv::Mat& frame);

	TRACKING_TYPE trackingType;
	TARGET_TYPE targetType;

	cv::Scalar color;
	std::vector<PointState> points;
	cv::Rect rect;
	cv::Point center;
	float size = 0;
	bool active = true;
};

class TrackingStatus : public TrackingStatusBase
{
public:
	TrackingStatus(TrackingTarget& parent, TRACKING_TYPE trackingType)
		:TrackingStatusBase(trackingType, parent.targetType), parent(parent)
	{

	}

	void SnapResult(EventListPtr& events, time_t time);

protected:
	TrackingTarget& parent;
};