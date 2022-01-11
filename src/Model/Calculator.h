#pragma once

#include "Model/Model.h"
#include "Model/TrackingEvent.h"

#include <opencv2/core.hpp>

class TrackingCalculator
{
public:
	TrackingCalculator();

	bool Update(TrackingSet& set, time_t t);
	void Recalc(TrackingSet& set, time_t t);
	void Draw(TrackingSet& set, cv::Mat& frame, time_t t, bool livePosition = true, bool drawState = false);
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