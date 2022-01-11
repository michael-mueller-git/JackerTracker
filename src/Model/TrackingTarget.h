#pragma once

#include "Model.h"
#include "Main.h"

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <json.hpp>
using json = nlohmann::json;

class TrackingTarget {
public:
	TrackingTarget();

	TrackingTarget(std::string guid);

	static TrackingTarget Unserialize(json& j);
	void Serialize(json& j);

	void UpdateColor(int index)
	{
		color = PickColor(index);
	}

	TrackingEvent* GetResult(TrackingSet& set, time_t time, EventType type, bool findLast = false);
	bool SupportsTrackingType(TRACKING_TYPE t);
	void UpdateType();
	void Draw(cv::Mat& frame);
	std::string GetName();
	std::string GetGuid()
	{
		return guid;
	}
	TrackingStatus* InitTracking(TRACKING_TYPE t);

	TARGET_TYPE targetType = TARGET_TYPE::TYPE_UNKNOWN;
	TRACKING_TYPE trackingType = TRACKING_TYPE::TYPE_NONE;
	cv::Scalar color;

	std::vector<cv::Point> initialPoints;
	cv::Rect initialRect;
	cv::Rect range;
	TrackerJTType preferredTracker = TrackerJTType::TRACKER_TYPE_UNKNOWN;

private:
	std::string guid;
};