#include "StateEditSet.h"
#include "StateConfirm.h"
#include "StateEditTracker.h"
#include "StateTracking.h"
#include "Gui/TrackingWindow.h"

#include <magic_enum.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

// StateEditSet

StateEditSet::StateEditSet(TrackingWindow* window, TrackingSet* set)
	:StatePlayerImpl(window), set(set)
{

}

void StateEditSet::EnterState(bool again)
{
	StatePlayer::EnterState(again);

	if(!again)
		window->timebar.SelectTrackingSet(set);
}

void StateEditSet::LeaveState()
{
	window->timebar.SelectTrackingSet(nullptr);
}

void StateEditSet::AddGui(Mat& frame)
{
	StatePlayer::AddGui(frame);

	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	auto pos = window->GetCurrentPosition();
	bool drawState = false;
	if (pos == set->timeStart)
		set->Draw(frame);
	else
		drawState = true;

	calculator.Draw(*set, frame, window->GetCurrentPosition(), false, drawState);
}

void StateEditSet::UpdateButtons(vector<GuiButton>& out)
{
	StateBase::UpdateButtons(out);
	auto me = this;

	AddButton(out, "Copy tracking here", 'a');
	AddButton(out, "Delete tracking", '-');
	AddButton(out, "Start tracking", 'l');
	AddButton(out, "Range update", [me]() {
		me->window->stack.PushState(new StateEditRange(me->window, me->set, me->window->GetCurrentPosition()));
	});

	auto& modeBtn = AddButton(out, "Mode: " + TrackingModeToString(set->trackingMode), [me]() {
		me->modeOpen = !me->modeOpen;
		me->window->DrawWindow(true);
	});

	if (modeOpen)
	{
		constexpr auto types = magic_enum::enum_entries<TrackingMode>();

		int x = modeBtn.rect.x + modeBtn.rect.width + 20;

		for (auto& t : types)
		{
			Rect r = modeBtn.rect;
			r.width = 120;

			r.x = x;
			x += r.width + 20;

			GuiButton newBtn;
			newBtn.rect = r;
			newBtn.text = TrackingModeToString(t.first);
			newBtn.textScale = 0.5;
			newBtn.onClick = [me, t]() {
				me->set->trackingMode = t.first;
				me->modeOpen = false;
				me->window->DrawWindow(true);
			};

			out.push_back(newBtn);
		}
	}

	int y = 220;

	Rect r;
	GuiButton* b;

	for (int i = 0; i < set->targets.size(); i++)
	{
		TrackingTarget* t = &set->targets.at(i);
		TrackingSet* s = set;

		r = Rect(
			380,
			y,
			230,
			40
		);

		b = &AddButton(
			out,
			t->GetName(),
			[me, s, t]() {
				me->window->stack.PushState(new StateEditTracker(me->window, s, t));
			},
			r
				);

		b->textColor = t->color;

		r = Rect(
			r.x + r.width + 20,
			y,
			40,
			40
		);

		b = &AddButton(
			out,
			"X",
			[me, i]() {
				auto& tv = me->set->targets;
				tv.erase(tv.begin() + i);

				for (int c = 0; c < tv.size(); c++)
					tv.at(c).UpdateColor(c);

				me->window->DrawWindow(true);
			},
			r
				);

		y += (40 + 20);
	}

	r = Rect(
		380,
		y,
		230,
		40
	);

	b = &AddButton(
		out,
		"Targets (+)",
		[me]() {
			me->HandleInput('+');
		},
		r
			);

	me->window->DrawWindow();
}

bool StateEditSet::HandleInput(int c)
{
	auto window = this->window;
	auto me = this;
	auto s = me->set;

	if (c == 'a')
	{
		TrackingSet* s = window->AddSet();
		if (s != nullptr)
		{
			int i = 0;

			for (auto& t : set->targets)
			{
				s->targets.emplace_back();
				auto& newTarget = s->targets.back();

				TrackingEvent* state = t.GetResult(*set, window->GetCurrentPosition(), EventType::TET_STATE, true);

				if (state)
				{
					newTarget.initialRect = state->state.rect;

					newTarget.initialPoints = t.initialPoints;
					for (auto& p : state->state.points)
						newTarget.initialPoints.push_back(p.point);
				}
				else
				{
					newTarget.initialRect = t.initialRect;
					newTarget.initialPoints = t.initialPoints;
				}

				newTarget.preferredTracker = t.preferredTracker;
				newTarget.range = t.range;
				newTarget.targetType = t.targetType;
				newTarget.trackingType = t.trackingType;
				newTarget.UpdateColor(i);
				i++;
			}

			s->trackingMode = set->trackingMode;

			window->stack.ReplaceState(new StateEditSet(window, s));
		}
		return true;
	}

	if (c == 'l')
	{
		window->stack.PushState(new StateTracking(window, set));
		return true;
	}

	if (c == '+')
	{
		s->targets.emplace_back();
		TrackingTarget* t = &s->targets.back();
		t->UpdateColor(s->targets.size());

		if (s->targets.size() == 1)
			t->targetType = TARGET_TYPE::TYPE_FEMALE;
		else if (s->targets.size() == 2)
			t->targetType = TARGET_TYPE::TYPE_MALE;
		else if (s->targets.size() == 3)
			t->targetType = TARGET_TYPE::TYPE_BACKGROUND;
		else
			t->targetType = TARGET_TYPE::TYPE_UNKNOWN;

		window->stack.PushState(new StateEditTracker(window, set, t));

		return true;
	}

	if (c == '-')
	{
		window->stack.PushState(new StateConfirm(window, "Are you sure you want to delete this set?",
			// Yes
			[me, window, s]()
			{
				window->DeleteTrackingSet(s);
				me->Pop();
			},
			// No
				[]() {; }
			));

		return true;
	}

	if (StatePlayer::HandleInput(c))
		return true;

	return false;
}

// StateEditRange

StateEditRange::StateEditRange(TrackingWindow* window, TrackingSet* set, time_t time)
	:StateBase(window), set(set), time(time)
{

}

void StateEditRange::AddGui(cv::Mat& frame)
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
	if(getWindowProperty("Settings", WND_PROP_VISIBLE))
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
	window->DrawWindow();
}