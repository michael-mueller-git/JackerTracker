#pragma once

#include <Windows.h>

#include <string>
#include <vector>
#include <chrono>
#include "opencv2/opencv.hpp"

#include <json.hpp>
using json = nlohmann::json;

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
	TYPE_UNKNOWN,
	TYPE_MALE,
	TYPE_FEMALE,
	TYPE_BACKGROUND
};

enum TRACKING_TYPE
{
	TYPE_NONE,
	TYPE_POINTS,
	TYPE_RECT,
	TYPE_BOTH
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
		TrackingState(TrackingTarget& parent)
			:parent(parent)
		{
			color = Scalar(
				(double)std::rand() / RAND_MAX * 255,
				(double)std::rand() / RAND_MAX * 255,
				(double)std::rand() / RAND_MAX * 255
			);
		}

		Scalar color;
		vector<PointState> points;
		Rect rect;
		Point2f center;
		bool active = true;

		TrackingTarget& parent;

		void Draw(Mat& frame);
	};

	TrackingTarget()
	{
		color = Scalar(
			(double)std::rand() / RAND_MAX * 255,
			(double)std::rand() / RAND_MAX * 255,
			(double)std::rand() / RAND_MAX * 255
		);
	};

	bool SupportsTrackingType(TRACKING_TYPE t);
	void UpdateType();
	void Draw(Mat& frame);
	string GetName();
	TrackingState* InitTracking();

	TARGET_TYPE targetType = TARGET_TYPE::TYPE_UNKNOWN;
	TRACKING_TYPE trackingType = TRACKING_TYPE::TYPE_NONE;
	Scalar color;

	vector<Point2f> intialPoints;
	Rect2f initialRect;
};

class TrackingSet {
public:
	TrackingSet(track_time timeStart, track_time timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd)
	{

	}

	TrackingTarget* GetTarget(TARGET_TYPE type);

	void Draw(Mat& frame);

	vector<TrackResult> results;
	vector<TrackingTarget> targets;

	track_time timeStart;
	track_time timeEnd;
};

struct GuiButton
{
	Rect rect;
	string text;
	bool hover = false;
	function<void()> onClick;

	bool IsSelected(int x, int y)
	{
		return (
			x > rect.x&& x < rect.x + rect.width &&
			y > rect.y&& y < rect.y + rect.height
		);
	}
};

class Project
{
public:
	Project(string video);
	~Project();

	string GetConfigPath();

	void Load();
	void Load(json j);
	void Save();
	void Save(json &j);

	TrackingSet* AddSet();
	void DeleteTrackingSet(TrackingSet* s);

	vector<TrackingSet> sets;

protected:
	string video;
};