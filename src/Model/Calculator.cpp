#include "Calculator.h"
#include "TrackingSet.h"

#include <opencv2/imgproc.hpp>

using namespace cv;

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
					set.events.emplace_back(EventType::TET_SCALE, t);
					set.events.back().size = scale;
					set.events.back().targetGuid = backgroundTarget->GetGuid();
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

	float distance = 0;
	int x, y;

	switch (set.trackingMode) {
	case TrackingMode::TM_HORIZONTAL:
		y = (malePoint.y + femalePoint.y) / 2;
		malePoint.y = y;
		femalePoint.y = y;
		distance = (float)norm(malePoint - femalePoint) * scale;
		break;
	
	case TrackingMode::TM_VERTICAL:
		x = (malePoint.x + femalePoint.x) / 2;
		malePoint.x = x;
		femalePoint.x = x;
		distance = (float)norm(malePoint - femalePoint) * scale;
		break;

	case TrackingMode::TM_DIAGONAL:
		distance = (float)norm(malePoint - femalePoint) * scale;
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

	if (distanceMin == 0)
		distanceMin = distance;
	else
		distanceMin = min(distanceMin, distance);

	if (distanceMax == 0)
		distanceMax = distance;
	else
		distanceMax = max(distanceMax, distance);

	float newPosition = mapValue<int, float>(distance, distanceMin, distanceMax, 0, 1);
	float d = position - newPosition;

	if (d < 0 && up)
	{
		up = false;
		set.events.emplace_back(EventType::TET_POSITION, t);
		set.events.back().position = position;
	}
	else if (d > 0 && !up)
	{
		up = true;

		set.events.emplace_back(EventType::TET_POSITION, t);
		set.events.back().position = position;
	}

	position = newPosition;

	return true;
}

void TrackingCalculator::Draw(TrackingSet& set, Mat& frame, time_t t, bool livePosition, bool drawState)
{
	if(lockedOn)
		line(frame, malePoint, femalePoint, Scalar(255, 255, 255), 2);

	int barHeight = 300;

	int barBottom = frame.size().height - barHeight - 30;
	rectangle(
		frame,
		Rect(30, barBottom, 30, barHeight),
		Scalar(0, 0, 255),
		2
	);

	std::vector<TrackingEvent> eventsFiltered;

	time_t timeFrom = max(0LL, t - 10000);
	time_t timeTo = t + 10000;

	copy_if(
		set.events.begin(),
		set.events.end(),
		back_inserter(eventsFiltered),
		[timeFrom, timeTo](auto& e) {
			return e.time >= timeFrom && e.time <= timeTo;
		}
	);

	TrackingEvent* e1 = nullptr;
	TrackingEvent* e2 = nullptr;

	if (!livePosition)
	{
		for (int i = 0; i < eventsFiltered.size(); i++)
		{
			auto& e = eventsFiltered.at(i);

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

	for (auto& e : eventsFiltered)
	{
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

		if (drawState)
		{
			if (e.type == EventType::TET_STATE)
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
	}
}