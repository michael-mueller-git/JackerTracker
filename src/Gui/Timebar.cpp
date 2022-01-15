#include "Timebar.h"
#include "States/Set/StateEditSet.h"
#include "TrackingWindow.h"

#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

// Timebar

Timebar::Timebar(TrackingWindow* window)
	:StateBase(window)
{

}

void Timebar::UpdateButtons(ButtonListOut out)
{
	auto me = this;

	Rect barRect(
		20,
		20,
		window->GetOutFrame()->cols - 40,
		20
	);

	for (auto s : window->project.sets)
	{
		out.emplace_back(new TimebarButton(me->window, s, barRect));
	}
}

void Timebar::Draw(Mat& frame)
{
	int w = frame.cols;

	Rect barRect(
		20,
		20,
		w - 40,
		20
	);

	rectangle(frame, barRect, Scalar(100, 100, 100), -1);
	rectangle(frame, barRect, Scalar(160, 160, 160), 1);

	int x = mapValue<time_t, int>(
		window->GetCurrentPosition(),
		0,
		window->GetDuration(),
		barRect.x,
		barRect.x + barRect.width
		);

	line(
		frame,
		Point(
			x,
			barRect.y - 5
		),
		Point(
			x,
			barRect.y + barRect.height + 5
		),
		Scalar(0, 0, 0), 2
	);

	for (auto s : window->project.sets)
	{
		if (s->targets.size() == 0 || s->timeStart == s->timeEnd)
			continue;

		int x1 = mapValue<time_t, int>(
			s->timeStart,
			0,
			window->GetDuration(),
			barRect.x,
			barRect.x + barRect.width
			);

		int x2 = mapValue<time_t, int>(
			s->timeEnd,
			0,
			window->GetDuration(),
			barRect.x,
			barRect.x + barRect.width
			);

		line(frame, Point(x1, 30), Point(x2, 30), Scalar(0, 0, 255), 2);

	}
}

void Timebar::SelectTrackingSet(TrackingSetPtr s)
{
	selectedSet = s;
	if (s)
	{
		if (s->timeStart != window->GetCurrentPosition())
			window->SetPosition(s->timeStart);
	}
};

TrackingSetPtr Timebar::GetNextSet()
{
	if (window->project.sets.size() == 0)
		return nullptr;

	auto s = GetSelectedSet();
	auto& sets = window->project.sets;

	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSetPtr set) { return set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index < sets.size() - 1)
			return (sets.at(index + 1));
		else
			return nullptr;
	}
	else
	{
		auto t = window->GetCurrentPosition();
		auto it = find_if(sets.begin(), sets.end(), [t](TrackingSetPtr set) { return set->timeStart > t; });
		if (it == sets.end())
			return nullptr;

		return *it;
	}
}

TrackingSetPtr Timebar::GetPreviousSet()
{
	if (window->project.sets.size() == 0)
		return nullptr;


	auto s = GetSelectedSet();
	auto& sets = window->project.sets;

	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSetPtr set) { return set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index > 0)
			return (sets.at(index - 1));
		else
			return nullptr;
	}
	else
	{

		auto t = window->GetCurrentPosition();

		auto it = find_if(sets.rbegin(), sets.rend(), [t](TrackingSetPtr set) { return set->timeStart < t; });
		if (it == sets.rend())
			return nullptr;

		return *it;
	}
}

// TimebarButton

TimebarButton::TimebarButton(TrackingWindow* w, TrackingSetPtr s, Rect bar)
	:w(w), set(s)
{
	int x = mapValue<time_t, int>(
		s->timeStart,
		0,
		w->GetDuration(),
		bar.x,
		bar.x + bar.width
	);

	rect = Rect(
		x - 5,
		bar.y - 2,
		10,
		bar.height + 4
	);

	text = to_string(s->targets.size());
	textScale = 0.3;
}

void TimebarButton::Handle()
{
	if (!w->stack.HasState())
		return;
	
	StateBase& s = w->stack.GetState();
	
	if (s.GetName() == "Playing")
	{
		w->PushState(new StateEditSet(w, set));
	}
	else if (s.GetName() == "EditSet")
	{
		w->stack.ReplaceState(new StateEditSet(w, set));
	}
}

bool TimebarButton::Highlighted()
{
	return w->timebar.GetSelectedSet() == set;
}