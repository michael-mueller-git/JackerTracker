#include "Timebar.h"
#include "States/StateEditSet.h"
#include "TrackingWindow.h"

#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

Timebar::Timebar(TrackingWindow* window)
	:StateBase(window)
{

}

void Timebar::UpdateButtons(vector<GuiButton>& out)
{
	auto me = this;

	Rect barRect(
		20,
		20,
		window->GetOutFrame()->cols - 40,
		20
	);

	for (auto& s : window->project.sets)
	{
		int x = mapValue<time_t, int>(
			s.timeStart,
			0,
			window->GetDuration(),
			barRect.x,
			barRect.x + barRect.width
			);

		Rect setRect(
			x - 5,
			barRect.y - 2,
			10,
			barRect.height + 4
		);

		TrackingSet* setptr = &s;

		GuiButton b;
		b.text = to_string(s.targets.size());
		b.textScale = 0.4;
		b.rect = setRect;
		b.customHover = true;
		b.onClick = [me, setptr]() {
			auto s = me->window->stack.GetState();
			if (!s)
				return false;

			if (s->GetName() == "Paused")
			{
				me->window->stack.PushState(new StateEditSet(me->window, setptr));
			}
			else if (s->GetName() == "EditSet")
			{
				me->window->stack.ReplaceState(new StateEditSet(me->window, setptr));
			}

		};

		if (selectedSet == &s)
			b.hover = true;

		out.push_back(b);
	}
}

void Timebar::AddGui(Mat& frame)
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

	for (auto& s : window->project.sets)
	{
		if (s.targets.size() == 0 || s.timeStart == s.timeEnd)
			continue;

		int x1 = mapValue<time_t, int>(
			s.timeStart,
			0,
			window->GetDuration(),
			barRect.x,
			barRect.x + barRect.width
			);

		int x2 = mapValue<time_t, int>(
			s.timeEnd,
			0,
			window->GetDuration(),
			barRect.x,
			barRect.x + barRect.width
			);

		line(frame, Point(x1, 30), Point(x2, 30), Scalar(0, 0, 255), 2);

	}
}

void Timebar::SelectTrackingSet(TrackingSet* s)
{
	selectedSet = s;
	if (s)
	{
		if (s->timeStart != window->GetCurrentPosition())
			window->SetPosition(s->timeStart);
	}
};

TrackingSet* Timebar::GetNextSet()
{
	if (window->project.sets.size() == 0)
		return nullptr;

	auto s = GetSelectedSet();
	auto& sets = window->project.sets;

	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index < sets.size() - 1)
			return &(sets.at(index + 1));
		else
			return nullptr;
	}
	else
	{
		auto t = window->GetCurrentPosition();
		auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return set.timeStart > t; });
		if (it == sets.end())
			return nullptr;

		return &(*it);
	}
}

TrackingSet* Timebar::GetPreviousSet()
{
	if (window->project.sets.size() == 0)
		return nullptr;


	auto s = GetSelectedSet();
	auto& sets = window->project.sets;

	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index > 0)
			return &(sets.at(index - 1));
		else
			return nullptr;
	}
	else
	{

		auto t = window->GetCurrentPosition();

		auto it = find_if(sets.rbegin(), sets.rend(), [t](TrackingSet& set) { return set.timeStart < t; });
		if (it == sets.rend())
			return nullptr;

		return &(*it);
	}
}