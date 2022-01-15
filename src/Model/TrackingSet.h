#pragma once

#include "Model.h"
#include "TrackingEvent.h"
#include "EventList.h"

#include <opencv2/core.hpp>
#include <vector>
#include <functional>

class TrackingSet {
public:
	TrackingSet(time_t timeStart, time_t timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd)
	{

	}

	TrackingTarget* GetTarget(TARGET_TYPE type);

	static TrackingSetPtr Unserialize(json& j);
	void Serialize(json& j);

	void Draw(cv::Mat& frame);

	std::vector<TrackingTarget> targets;

	TrackingMode trackingMode = TrackingMode::TM_DIAGONAL;
	time_t timeStart;
	time_t timeEnd;

	EventListPtr events;
};

typedef std::shared_ptr<TrackingSet> TrackingSetPtr;