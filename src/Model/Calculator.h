#pragma once

#include "Model/Model.h"
#include "Model/TrackingEvent.h"

#include <opencv2/core.hpp>

class TrackingCalculator
{
public:
	TrackingCalculator();

	TrackingEvent GetRange(EventListPtr& eventList, time_t at);
	bool Update(TrackingSetPtr set, time_t t);
	void UpdatePositions(EventListPtr& eventList);
	void Draw(TrackingSetPtr set, cv::Mat& frame, time_t t, bool livePosition = true, bool drawState = false);
	void Reset()
	{
		malePoint = cv::Point();
		femalePoint = cv::Point();

		scale = 1;
		sizeStart = 0;
		position = 0;
	}


protected:
	cv::Point malePoint;
	cv::Point femalePoint;

	float scale = 1;
	float sizeStart = 0;

	time_t lastPositionUpdate = 0;
	float position;
	bool lockedOn = false;
	bool up = true;
};