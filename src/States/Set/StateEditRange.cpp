#include "StateEditRange.h"
#include "Gui/TrackingWindow.h"

#include <opencv2/highgui.hpp>

using namespace cv;
using namespace std;

StateEditRange::StateEditRange(TrackingWindow* window, TrackingSetPtr set, time_t time)
	:StateBase(window), set(set), time(time)
{

}

void StateEditRange::Draw(cv::Mat& frame)
{
	calculator.Draw(set, frame, time, false);
}

void StateEditRange::EnterState(bool again)
{
	auto& eventList = set->events;

	window->SetPosition(time);
	rangeEvent = eventList->GetEvent(time, EventType::TET_POSITION_RANGE);
	if (!rangeEvent)
	{
		TrackingEvent e = calculator.GetRange(eventList, time);
		rangeEvent = eventList->AddEvent(make_shared<TrackingEvent>(e));
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
	calculator.UpdatePositions(set->events);
	AskDraw();
}