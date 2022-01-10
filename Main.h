#pragma once

#include <Windows.h>

#include <string>
#include <vector>
#include <chrono>
#include "opencv2/opencv.hpp"
#include <opencv2/plot.hpp>
#include "magic_enum.hpp"
#include <crossguid/guid.hpp>

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

string TargetTypeToString(TARGET_TYPE t);

enum TRACKING_TYPE
{
	TYPE_NONE,
	TYPE_POINTS,
	TYPE_RECT,
	TYPE_BOTH
};

enum FrameVariant
{
	VARIANT_UNKNOWN,
	GPU_RGBA,
	GPU_GREY,
	GPU_RGB,
	LOCAL_RGB,
	LOCAL_GREY
};

enum TrackerJTType
{
	TRACKER_TYPE_UNKNOWN,
	GPU_POINTS_PYLSPRASE,
	GPU_RECT_GOTURN,
	GPU_RECT_DIASIAM,

	CPU_RECT_MEDIAN_FLOW,
	CPU_RECT_CSRT,
	CPU_RECT_MIL,
	CPU_RECT_KCF
};

Scalar PickColor(int index);

class TrackingSet;
class TrackingTarget;
class TrackingState;
class TrackingStateBase;

struct PointState
{
	Point2f point;
	bool active = true;
};

class TrackingStateBase
{
public:
	TrackingStateBase(TRACKING_TYPE trackingType, TARGET_TYPE targetType)
		:trackingType(trackingType), targetType(targetType)
	{

	}

	void UpdateColor(int index)
	{
		color = PickColor(index);
	}

	void Draw(Mat& frame);

	TRACKING_TYPE trackingType;
	TARGET_TYPE targetType;

	Scalar color;
	vector<PointState> points;
	Rect rect;
	Point2f center;
	float size = 0;
	bool active = true;
};

struct TrackingEvent
{
	enum Type {
		TET_POSITION,
		TET_POINT,
		TET_SIZE,
		TET_SCALE,
		TET_STATE
	};

	TrackingEvent(Type type, time_t time)
		:type(type), time(time), state(TRACKING_TYPE::TYPE_NONE, TARGET_TYPE::TYPE_UNKNOWN)
	{

	}

	static TrackingEvent Unserialize(json& j);
	void Serialize(json& j);

	string targetGuid;
	TARGET_TYPE targetType;

	Type type;
	time_t time;

	Point2f point;
	float size;
	Rect rect;
	vector<PointState> points;
	TrackingStateBase state;
	
	float position;
	float minPosition;
	float maxPosition;
};

class TrackingCalculator
{
public:
	TrackingCalculator();

	bool Update(TrackingSet& set, time_t t);
	void Draw(TrackingSet& set, Mat& frame, time_t t, bool livePosition = true, bool drawState = false);
	void Reset()
	{
		malePoint = Point2f();
		femalePoint = Point2f();

		distanceMin = 0;
		distanceMax = 0;

		scale = 1;
		sizeStart = 0;
		position = 0;
	}


protected:
	Point2f malePoint;
	Point2f femalePoint;
	
	float distanceMin = 0;
	float distanceMax = 0;
	
	float scale = 1;
	float sizeStart = 0;

	float position;
	bool lockedOn = false;
	bool up = true;
};

class TrackingTarget {
public:
	TrackingTarget()
	{
		guid = xg::newGuid();
	}

	TrackingTarget(string guid)
		:guid(guid)
	{
		;
	};

	static TrackingTarget Unserialize(json& j);
	void Serialize(json& j);

	void UpdateColor(int index)
	{
		color = PickColor(index);
	}

	TrackingEvent* GetResult(TrackingSet& set, time_t time, TrackingEvent::Type type);
	bool SupportsTrackingType(TRACKING_TYPE t);
	void UpdateType();
	void Draw(Mat& frame);
	string GetName();
	string GetGuid()
	{
		return guid;
	}
	TrackingState* InitTracking(TRACKING_TYPE t);

	TARGET_TYPE targetType = TARGET_TYPE::TYPE_UNKNOWN;
	TRACKING_TYPE trackingType = TRACKING_TYPE::TYPE_NONE;
	Scalar color;

	vector<Point2f> intialPoints;
	Rect2f initialRect;
	TrackerJTType preferredTracker = TrackerJTType::TRACKER_TYPE_UNKNOWN;

private:
	string guid;
};


class TrackingState : public TrackingStateBase
{
public:
	TrackingState(TrackingTarget& parent, TRACKING_TYPE trackingType)
		:TrackingStateBase(trackingType, parent.targetType), parent(parent)
	{

	}

	void SnapResult(TrackingSet& set, track_time time);

protected:
	TrackingTarget& parent;
};

class TrackingSet {
public:
	TrackingSet(track_time timeStart, track_time timeEnd)
		:timeStart(timeStart), timeEnd(timeEnd)
	{

	}

	TrackingTarget* GetTarget(TARGET_TYPE type);
	TrackingEvent* GetResult(time_t time, TrackingEvent::Type type);

	void Draw(Mat& frame);

	vector<TrackingTarget> targets;
	vector<TrackingEvent> events;

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
	bool customHover = false;
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

	vector<TrackingSet> sets;

protected:
	string video;
};