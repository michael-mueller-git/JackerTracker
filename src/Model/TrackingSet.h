#pragma once

#include "Model.h"
#include "TrackingEvent.h"

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
	TrackingEvent* GetResult(time_t time, EventType type, TrackingTarget* target = nullptr, bool findLast = false);
	TrackingEvent& AddEvent(EventType type, time_t t, TrackingTarget* target = nullptr);
	void GetEvents(time_t from, time_t to, std::vector<TrackingEvent*>& out);
	void ClearEvents(std::function<bool(TrackingEvent& e)> callback);

	static TrackingSet Unserialize(json& j);
	void Serialize(json& j);

	void Draw(cv::Mat& frame);

	std::vector<TrackingTarget> targets;

	TrackingMode trackingMode = TrackingMode::TM_DIAGONAL;
	time_t timeStart;
	time_t timeEnd;

protected:
	std::vector<TrackingEvent> events;
};
