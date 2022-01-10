#pragma once

#include "Model.h"
#include "TrackingEvent.h"

#include <opencv2/core.hpp>
#include <vector>

class TrackingSet {
public:
	TrackingSet(time_t timeStart, time_t timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd)
	{

	}

	TrackingTarget* GetTarget(TARGET_TYPE type);
	TrackingEvent* GetResult(time_t time, EventType type);

	static TrackingSet Unserialize(json& j);
	void Serialize(json& j);

	void Draw(cv::Mat& frame);

	std::vector<TrackingTarget> targets;
	std::vector<TrackingEvent> events;

	TrackingMode trackingMode = TrackingMode::TM_DIAGONAL;
	time_t timeStart;
	time_t timeEnd;
};