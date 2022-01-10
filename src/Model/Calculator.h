#pragma once

#include "Model/Model.h"

#include <opencv2/core.hpp>

class TrackingCalculator
{
public:
	TrackingCalculator();

	bool Update(TrackingSet& set, time_t t);
	void Draw(TrackingSet& set, cv::Mat& frame, time_t t, bool livePosition = true, bool drawState = false);
	void Reset()
	{
		malePoint = cv::Point();
		femalePoint = cv::Point();

		distanceMin = 0;
		distanceMax = 0;

		scale = 1;
		sizeStart = 0;
		position = 0;
	}


protected:
	cv::Point malePoint;
	cv::Point femalePoint;

	float distanceMin = 0;
	float distanceMax = 0;

	float scale = 1;
	float sizeStart = 0;

	float position;
	bool lockedOn = false;
	bool up = true;
};