#pragma once

#include <opencv2/core.hpp>

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

enum TrackingMode
{
	TM_UNKNOWN,
	TM_DIAGONAL,
	TM_HORIZONTAL,
	TM_VERTICAL,
	TM_DIAGONAL_SINGLE,
	TM_HORIZONTAL_SINGLE,
	TM_VERTICAL_SINGLE
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

struct PointState
{
	cv::Point point;
	bool active = true;
};

enum EventType {
	TET_POSITION,
	TET_POSITION_RANGE,
	TET_POINT,
	TET_SIZE,
	TET_SCALE,
	TET_STATE,
	TET_BADFRAME,
};

enum DRAGGING_BUTTON
{
	BUTTON_NONE,
	BUTTON_LEFT,
	BUTTON_RIGHT
};

class TrackingWindow;
class TrackingEvent;
class TrackingTarget;
class TrackingSet;
class TrackingStatus;
class EventList;
class TrackingRunner;

typedef std::unique_ptr<EventList> EventListPtr;
typedef std::shared_ptr<TrackingEvent> EventPtr;
typedef std::shared_ptr<TrackingSet> TrackingSetPtr;