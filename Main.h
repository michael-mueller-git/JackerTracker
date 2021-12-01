#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;
using namespace chrono;

// Time in miliseconds
typedef unsigned long track_time;

long mapValue(long x, long in_min, long in_max, long out_min, long out_max);
void drawGpuPoints(cuda::GpuMat& in, Mat& out, Scalar c);
std::string getImageType(int number);

struct TrackResult {
	track_time time;
	float position;
};

class TrackingTarget {
public:
	TrackingTarget()
	{

	};

	void Draw(Mat& frame);
	string GetName();

	enum TARGET_TYPE {
		TYPE_MALE,
		TYPE_FEMALE,
		TYPE_BACKGROUND
	};

	enum TRACKING_TYPE
	{
		TYPE_POINTS,
		TYPE_RECT
	};

	string name;
	vector<Point2f>points;
	Rect rect;
	TARGET_TYPE targetType;
	TRACKING_TYPE trackingType;
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