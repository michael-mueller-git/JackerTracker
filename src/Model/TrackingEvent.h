#pragma once

#include "Model.h"
#include "TrackingStatus.h"

#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <json.hpp>
using json = nlohmann::json;

struct TrackingEvent
{
	TrackingEvent(EventType type, time_t time)
		:type(type), time(time), state(TRACKING_TYPE::TYPE_NONE, TARGET_TYPE::TYPE_UNKNOWN)
	{

	}

	static TrackingEvent Unserialize(json& j);
	void Serialize(json& j);

	std::string targetGuid;
	TARGET_TYPE targetType;

	EventType type;
	time_t time;

	cv:: Point2f point;
	float size;
	cv::Rect rect;
	std::vector<PointState> points;
	TrackingStatusBase state;

	float position;
	float minPosition;
	float maxPosition;
};