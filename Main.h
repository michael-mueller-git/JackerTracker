#pragma once

#include <Windows.h>

#include <string>
#include <vector>
#include <chrono>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;
using namespace chrono;

// Time in miliseconds
typedef unsigned long track_time;

template<typename I, typename O>
O mapValue(I x, I in_min, I in_max, O out_min, O out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void drawGpuPoints(cuda::GpuMat& in, Mat& out, Scalar c);
std::string getImageType(int number);

struct TrackResult {
	track_time time;
	float position;
};

enum TARGET_TYPE {
	TYPE_MALE,
	TYPE_FEMALE,
	TYPE_BACKGROUND
};

enum TRACKING_TYPE
{
	TYPE_NONE,
	TYPE_POINTS,
	TYPE_RECT
};

class TrackingTarget {
public:
	struct PointState
	{
		Point2f point;
		bool active = true;
	};

	struct TrackingState
	{
		vector<PointState> points;
		Rect currentRect;
		Point2f currentCenter;
		bool active = true;
	};

	TrackingTarget()
	{
		color = Scalar(
			(double)std::rand() / RAND_MAX * 255,
			(double)std::rand() / RAND_MAX * 255,
			(double)std::rand() / RAND_MAX * 255
		);
	};

	void Draw(Mat& frame);
	string GetName();
	TrackingState& InitTracking();

	string name;
	TARGET_TYPE targetType;
	TRACKING_TYPE trackingType;
	Scalar color;

	vector<Point2f> intialPoints;
	Rect initialRect;

	TrackingState trackingState;
};

class TrackingSet {
public:
	TrackingSet(track_time timeStart, track_time timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd)
	{

	}

	TrackingTarget* GetTarget(TrackingTarget::TARGET_TYPE type);

	void Draw(Mat& frame);

	vector<TrackResult> results;
	vector<TrackingTarget> targets;

	track_time timeStart;
	track_time timeEnd;
};