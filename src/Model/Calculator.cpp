#include "Calculator.h"
#include "TrackingSet.h"

#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

TrackingCalculator::TrackingCalculator()
{

}

bool TrackingCalculator::Update(TrackingSet& set, time_t t)
{
	TrackingTarget* backgroundTarget = set.GetTarget(TARGET_TYPE::TYPE_BACKGROUND);
	if (backgroundTarget)
	{
		auto backgroundResult = backgroundTarget->GetResult(set, t, EventType::TET_SIZE);
		if (backgroundResult)
		{
			if (sizeStart == 0)
			{
				sizeStart = backgroundResult->size;
			}
			else
			{
				float newScale = sizeStart / backgroundResult->size;
				if (scale != newScale)
				{
					scale = newScale;
					TrackingEvent& e = set.AddEvent(EventType::TET_SCALE, t, backgroundTarget);
					e.size = scale;
				}

			}
		}
	}

	TrackingTarget* maleTarget = set.GetTarget(TARGET_TYPE::TYPE_MALE);
	TrackingTarget* femaleTarget = set.GetTarget(TARGET_TYPE::TYPE_FEMALE);
	TrackingEvent* maleResult = nullptr;
	TrackingEvent* femaleResult = nullptr;
	TrackingEvent* result = nullptr;

	if (maleTarget)
		maleResult = maleTarget->GetResult(set, t, EventType::TET_POINT);
	if (femaleTarget)
		femaleResult = femaleTarget->GetResult(set, t, EventType::TET_POINT);

	switch (set.trackingMode) {
	case TrackingMode::TM_DIAGONAL:
	case TrackingMode::TM_HORIZONTAL:
	case TrackingMode::TM_VERTICAL:
		if (!maleResult || !femaleResult)
			return false;

		malePoint = maleResult->point;
		femalePoint = femaleResult->point;
		lockedOn = true;
		break;

	case TrackingMode::TM_DIAGONAL_SINGLE:
	case TrackingMode::TM_HORIZONTAL_SINGLE:
	case TrackingMode::TM_VERTICAL_SINGLE:
		if (maleResult)
			result = maleResult;
		else if (femaleResult)
			result = femaleResult;
		else
			return false;

		break;
	}

	int distance = 0;
	int x, y;

	switch (set.trackingMode) {
	case TrackingMode::TM_HORIZONTAL:
		y = (malePoint.y + femalePoint.y) / 2;
		malePoint.y = y;
		femalePoint.y = y;
		distance = norm(malePoint - femalePoint) * scale;
		break;
	
	case TrackingMode::TM_VERTICAL:
		x = (malePoint.x + femalePoint.x) / 2;
		malePoint.x = x;
		femalePoint.x = x;
		distance = norm(malePoint - femalePoint) * scale;
		break;

	case TrackingMode::TM_DIAGONAL:
		distance = norm(malePoint - femalePoint) * scale;
		break;

	case TrackingMode::TM_DIAGONAL_SINGLE:
		distance = (result->point.x + result->point.y) / 2;
		break;

	case TrackingMode::TM_HORIZONTAL_SINGLE:
		distance = result->point.x;
		break;

	case TrackingMode::TM_VERTICAL_SINGLE:
		distance = result->point.y;
		break;
	default:
		return false;
	}

	float newMin, newMax;

	TrackingEvent* distanceEvent = set.GetResult(t, EventType::TET_POSITION_RANGE, nullptr, true);

	if (!distanceEvent)
	{
		distanceEvent = &set.AddEvent(EventType::TET_POSITION_RANGE, t);
		distanceEvent->minDistance = distance - 1;
		distanceEvent->maxDistance = distance;
	}
	else
	{
		int newMin = min(distanceEvent->minDistance, distance);
		int newMax = max(distanceEvent->maxDistance, distance);

		if (newMin != distanceEvent->minDistance || newMax != distanceEvent->maxDistance)
		{
			distanceEvent = &set.AddEvent(EventType::TET_POSITION_RANGE, t);
			distanceEvent->minDistance = newMin;
			distanceEvent->maxDistance = newMax;
		}
	}

	
	float newPosition = mapValue<int, float>(distance, distanceEvent->minDistance, distanceEvent->maxDistance, 0, 1);
	float d = position - newPosition;
	//int dTime = lastPositionUpdate - t;

	//if (abs(d) > 0.08 || dTime > 200) {
		if (d < 0 && up)
		{
			up = false;
			TrackingEvent& e = set.AddEvent(EventType::TET_POSITION, t);
			e.size = distance;
			e.position = position;
			lastPositionUpdate = t;
		}
		else if (d > 0 && !up)
		{
			up = true;

			TrackingEvent& e = set.AddEvent(EventType::TET_POSITION, t);
			e.size = distance;
			e.position = position;
			lastPositionUpdate = t;
		}
	//}

	position = newPosition;

	return true;
}

void TrackingCalculator::Recalc(TrackingSet& set, time_t t)
{
	TrackingEvent* rangeEvent = set.GetResult(t, EventType::TET_POSITION_RANGE);
	if (!rangeEvent)
		return;

	vector<TrackingEvent*> events;
	set.GetEvents(t + 1, set.timeEnd, events);

	for (auto e : events)
	{
		if (e->type == EventType::TET_POSITION_RANGE)
			return;

		if (e->type != EventType::TET_POSITION)
			continue;

		if (e->size < rangeEvent->minDistance)
			e->position = 0;
		else if (e->size > rangeEvent->maxDistance)
			e->position = 1;
		else
			e->position = mapValue<int, float>(e->size, rangeEvent->minDistance, rangeEvent->maxDistance, 0, 1);
	}
}

void DrawReferenceLineMiddle(Mat& frame, int length, double angle, Point middle, Scalar color)
{
	Point2f p1(
		round(middle.x + (length / 2) * cos(angle)),
		round(middle.y + (length / 2) * sin(angle))
	);
	Point2f p2(
		round(middle.x + (-length / 2) * cos(angle)),
		round(middle.y + (-length / 2) * sin(angle))
	);

	line(frame, p1, p2, color, 8);
}

void DrawReferenceLineAngle(Mat& frame, int length, double angle, Point p1, Scalar color)
{
	Point2f p2(
		round(p1.x + length * cos(angle)),
		round(p1.y + length * sin(angle))
	);

	line(frame, p1, p2, color, 8);
}

void TrackingCalculator::Draw(TrackingSet& set, Mat& frame, time_t t, bool livePosition, bool drawState)
{
	int barHeight = 300;

	int barBottom = frame.size().height - barHeight - 30;
	rectangle(
		frame,
		Rect(30, barBottom, 30, barHeight),
		Scalar(0, 0, 255),
		2
	);

	std::vector<TrackingEvent*> eventsFiltered;

	time_t timeFrom = max(time_t(0LL), t - 10000);
	time_t timeTo = t + 10000;

	set.GetEvents(timeFrom, timeTo, eventsFiltered);

	TrackingEvent* e1 = nullptr;
	TrackingEvent* e2 = nullptr;

	if (!livePosition)
	{
		for (int i = 0; i < eventsFiltered.size(); i++)
		{
			TrackingEvent& e = *eventsFiltered.at(i);

			if (e.type != EventType::TET_POSITION)
				continue;

			if (e.time > t)
			{
				if (!e1)
					break;

				e2 = &e;
				break;
			}
			else
			{
				e1 = &e;
			}
		}

		if (e1 && e2)
		{
			position = mapValue<time_t, float>(t, e1->time, e2->time, e1->position, e2->position);
		}
		else
			position = 0;
	}

	int y = mapValue<float, int>(position, 0, 1, barHeight, 0);
	rectangle(
		frame,
		Rect(30, barBottom - y + barHeight, 30, y),
		Scalar(0, 255, 0),
		-1
	);

	int x = mapValue<time_t, int>(t, timeFrom, timeTo, 0, frame.cols);
	line(frame, Point(x, frame.rows - 150), Point(x, frame.rows), Scalar(0, 0, 0), 2);

	int graphTop = frame.rows - 150;
	int graphBottom = frame.rows;

	line(frame, Point(0, graphTop), Point(frame.cols, graphTop), Scalar(0, 0, 0), 1);
	line(frame, Point(0, graphBottom), Point(frame.cols, graphBottom), Scalar(0, 0, 0), 1);

	Point lastPointPosition(-1, -1);
	int stateIndex = 0;
	time_t stateTime = 0;
	time_t lastBad = 0;
	time_t trailTime = max(time_t(0LL), t - 2000);
	TrackingEvent* maleEvent = nullptr;
	TrackingEvent* femaleEvent = nullptr;

	for (auto ePtr : eventsFiltered)
	{
		TrackingEvent& e = *ePtr;

		if (e.time == t && e.type == EventType::TET_POINT)
		{
			for (auto& t : set.targets)
			{
				if (t.GetGuid() != e.targetGuid)
					continue;
					
				if (t.targetType == TARGET_TYPE::TYPE_MALE)
					maleEvent = ePtr;

				if (t.targetType == TARGET_TYPE::TYPE_FEMALE)
					femaleEvent = ePtr;
			}
		}

		if (e.type == EventType::TET_POINT && e.time < t && e.time > trailTime)
		{
			Scalar c(
				mapValue<time_t, int>(e.time, trailTime, t, 50, 255),
				0,
				0
			);

			circle(frame, e.point, 2, c, 2);
		}

		if (e.type == EventType::TET_POSITION)
		{
			x = mapValue<int, int>(e.time - timeFrom, 0, timeTo - timeFrom, 0, frame.cols);
			y = mapValue<float, int>(e.position, 0, 1, graphTop, graphBottom);
			Point p(x, y);

			if (lastPointPosition.x != -1)
			{
				line(frame, p, lastPointPosition, Scalar(255, 0, 0), 2);
			}

			circle(frame, p, 3, Scalar(255, 255, 255), 2);

			lastPointPosition = p;
		}

		if (e.type == EventType::TET_BADFRAME)
		{
			if (e.time == lastBad)
				continue;

			x = mapValue<int, int>(e.time - timeFrom, 0, timeTo - timeFrom, 0, frame.cols);
			line(frame, Point(x, graphTop), Point(x, graphBottom), Scalar(0, 0, 255), 2);

			lastBad = e.time;
		}

		if (drawState && e.type == EventType::TET_STATE)
		{
			if (stateTime == 0)
			{
				if (e.time >= t)
					stateTime = e.time;
			}

			if (stateTime != 0 && e.time == stateTime)
			{
				e.state.UpdateColor(stateIndex);
				e.state.Draw(frame);
				stateIndex++;
			}
		}
	}

	TrackingEvent* distanceEvent = set.GetResult(t, EventType::TET_POSITION_RANGE, nullptr, true);

	if (!livePosition)
	{
		if (maleEvent && femaleEvent)
		{
			malePoint = maleEvent->point;
			femalePoint = femaleEvent->point;
			
			lockedOn = true;
		}
		else
		{
			lockedOn = false;
		}
	}

	if (lockedOn)
	{
		//Point middle = (malePoint + femalePoint) / 2;

		double angle = atan2(
			femalePoint.y - malePoint.y,
			femalePoint.x - malePoint.x
		);

		if (distanceEvent)
		{
			DrawReferenceLineAngle(frame, distanceEvent->maxDistance, angle, malePoint, Scalar(0, 255, 0));
			DrawReferenceLineAngle(frame, distanceEvent->minDistance, angle, malePoint, Scalar(0, 0, 255));
		}

		line(frame, malePoint, femalePoint, Scalar(255, 255, 255), 2);
	}
}
