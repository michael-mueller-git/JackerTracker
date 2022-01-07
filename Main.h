#pragma once

#include <Windows.h>

#include <string>
#include <vector>
#include <chrono>
#include "opencv2/opencv.hpp"
#include <opencv2/plot.hpp>

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

class TrackingSet;

struct TrackingEvent
{
	enum Type {
		TET_POSITION
	};

	TrackingEvent(Type type, time_t time)
		:type(type), time(time)
	{

	}

	Type type;
	time_t time;
	float position;
	float minPosition;
	float maxPosition;
};

class TrackingCalculator
{
public:
	TrackingCalculator();

	bool Update(TrackingSet& set, time_t t);
	void Draw(Mat& frame, time_t t);


protected:
	Point2f malePoint;
	Point2f femalePoint;
	float distanceMin = 0;
	float distanceMax = 0;
	float position;
	bool up = true;

	Ptr<plot::Plot2d> plot;
	vector<TrackingEvent> events;
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

		void SnapResult(track_time time);

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
	map<time_t, Point2f> results;
};

class TrackingSet {
public:
	TrackingSet(track_time timeStart, track_time timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd), calculator()
	{

	}

	TrackingTarget* GetTarget(TARGET_TYPE type);

	void Draw(Mat& frame);

	vector<TrackingTarget> targets;
	TrackingCalculator calculator;

	track_time timeStart;
	track_time timeEnd;
};

struct GuiButton
{
	GuiButton()
		:textColor(0, 0, 0)
	{

	}

	Rect rect;
	string text;
	float textScale = 0.6;
	bool hover = false;
	function<void()> onClick;
	Scalar textColor;

	bool IsSelected(int x, int y)
	{
		return (
			x > rect.x&& x < rect.x + rect.width &&
			y > rect.y&& y < rect.y + rect.height
		);
	}

	void Draw(Mat& frame)
	{
		if (hover)
			rectangle(frame, rect, Scalar(70, 70, 70), -1);
		else
			rectangle(frame, rect, Scalar(100, 100, 100), -1);

		if (hover)
			rectangle(frame, rect, Scalar(20, 20, 20), 2);
		else
			rectangle(frame, rect, Scalar(160, 160, 160), 2);

		if (text.length() > 0)
		{
			auto size = getTextSize(text, FONT_HERSHEY_SIMPLEX, textScale, 2, nullptr);

			putText(
				frame,
				text,
				Point(
					rect.x + (rect.width / 2) - (size.width / 2),
					rect.y + (rect.height / 2)
				),
				FONT_HERSHEY_SIMPLEX,
				textScale,
				textColor,
				2
			);
		}
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