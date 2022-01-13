#include "StateEditRange.h"
#include "Gui/TrackingWindow.h"

#include <opencv2/highgui.hpp>

using namespace cv;
using namespace std;

StateEditRange::StateEditRange(TrackingWindow* window, TrackingSet* set, time_t time)
	:StateBase(window), set(set), time(time)
{

}

void StateEditRange::Draw(cv::Mat& frame)
{
	calculator.Draw(*set, frame, time, false);
}

void StateEditRange::EnterState(bool again)
{
	window->SetPosition(time);
	rangeEvent = set->GetResult(time, EventType::TET_POSITION_RANGE);
	int rangeMin = 0, rangeMax = 0;

	if (!rangeEvent)
	{
		rangeEvent = set->GetResult(time, EventType::TET_POSITION_RANGE, nullptr, true);

		if (rangeEvent)
		{
			rangeMin = rangeEvent->minDistance;
			rangeMax = rangeEvent->maxDistance;
		}
		else
		{
			vector<TrackingEvent*> events;
			set->GetEvents(set->timeStart, set->timeEnd, events);

			for (auto e : events)
			{
				if (e->type != EventType::TET_POSITION)
					continue;

				rangeMin = min((int)e->size, rangeMin);
				rangeMax = max((int)e->size, rangeMax);
			}
		}

		if (rangeMin == 0 && rangeMax == 0)
		{
			Pop();
			return;
		}

		rangeEvent = &set->AddEvent(EventType::TET_POSITION_RANGE, time);
		rangeEvent->minDistance = rangeMin;
		rangeEvent->maxDistance = rangeMax;
	}

	namedWindow("Settings");
	createTrackbar("Min", "Settings", &rangeEvent->minDistance, rangeEvent->maxDistance + 30, [](int v, void* p) { ((StateEditRange*)p)->Recalc(); }, this);
	createTrackbar("Max", "Settings", &rangeEvent->maxDistance, rangeEvent->maxDistance + 30, [](int v, void* p) { ((StateEditRange*)p)->Recalc(); }, this);
}

void StateEditRange::LeaveState()
{
	if (getWindowProperty("Settings", WND_PROP_VISIBLE))
		destroyWindow("Settings");
}

void StateEditRange::Update()
{
	if (!getWindowProperty("Settings", WND_PROP_VISIBLE))
		Pop();

}

void StateEditRange::Recalc()
{
	calculator.Recalc(*set, time);
	AskDraw();
}