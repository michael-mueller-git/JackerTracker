#include "TrackingStatus.h"
#include "TrackingEvent.h"
#include "TrackingSet.h"

#include <opencv2/imgproc.hpp>

using namespace cv;

TrackingStatus* TrackingTarget::InitTracking(TRACKING_TYPE t)
{
	TrackingStatus* trackingState = new TrackingStatus(*this, t);

	trackingState->active = true;
	trackingState->points.clear();
	trackingState->rect = Rect(0, 0, 0, 0);

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		for (auto& p : intialPoints)
		{
			PointState ps;
			ps.active = true;
			ps.point = p;
			trackingState->points.push_back(ps);
		}
	}

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
	{
		trackingState->rect = initialRect;
	}

	return trackingState;
}

void TrackingStatusBase::Draw(Mat& frame)
{
	circle(frame, center, 4, Scalar(0, 0, 0), 3);

	if (targetType == TARGET_TYPE::TYPE_BACKGROUND && size != 0)
	{
		circle(frame, center, size / 2, color, 2);
	}

	if (trackingType == TRACKING_TYPE::TYPE_POINTS)
	{
		for (auto& p : points)
		{
			if (p.active)
				circle(frame, p.point, 4, color, 3);
			else
				circle(frame, p.point, 4, Scalar(255, 0, 0), 3);
		}
	}

	if (trackingType == TRACKING_TYPE::TYPE_RECT)
	{
		if (active)
			rectangle(frame, rect, color, 3);
		else
			rectangle(frame, rect, Scalar(255, 0, 0), 3);
	}
}

void TrackingStatus::SnapResult(TrackingSet& set, time_t time)
{
	TrackingEvent e(EventType::TET_POINT, time);
	e.point = center;
	e.targetGuid = parent.GetGuid();
	set.events.push_back(e);

	if (size != 0)
	{
		TrackingEvent e(EventType::TET_SIZE, time);
		e.size = size;
		e.targetGuid = parent.GetGuid();
		set.events.push_back(e);
	}

	TrackingEvent e2(EventType::TET_STATE, time);
	TrackingStatusBase& state = *reinterpret_cast<TrackingStatusBase*>(this);

	e2.state = state;
	e2.targetGuid = parent.GetGuid();
	set.events.push_back(e2);
}