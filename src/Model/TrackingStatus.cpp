#include "TrackingStatus.h"
#include "TrackingEvent.h"
#include "TrackingSet.h"

#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

TrackingStatus* TrackingTarget::InitTracking(TRACKING_TYPE t)
{
	TrackingStatus* trackingState = new TrackingStatus(*this, t);

	trackingState->active = true;
	trackingState->points.clear();
	trackingState->rect = Rect(0, 0, 0, 0);

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		for (auto& p : initialPoints)
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

void TrackingStatus::SnapResult(EventListPtr& events, time_t time)
{
	lock_guard<mutex> lock(events->mtx);

	EventPtr e;

	e = events->AddEvent(EventType::TET_POINT, time, &parent);
	e->point = center;

	if (size != 0)
	{
		e = events->AddEvent(EventType::TET_SIZE, time, &parent);
		e->size = size;
	}

	e = events->AddEvent(EventType::TET_STATE, time, &parent);
	TrackingStatusBase& state = *reinterpret_cast<TrackingStatusBase*>(this);
	e->state = state;
}