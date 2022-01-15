#include "StateEditSet.h"
#include "StateEditRange.h"
#include "States/Target/StateEditTarget.h"
#include "States/Tracking/StateTracking.h"
//#include "States/Tracking/StateTrackingThreaded.h"
#include "Gui/TrackingWindow.h"
#include "Gui/GuiButtonExpand.h"

#include <magic_enum.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;
using namespace OIS;

// StateEditSet

StateEditSet::StateEditSet(TrackingWindow* window, TrackingSetPtr set)
	:StatePlayerImpl(window), set(set), runner(window, set, nullptr, true, false, true)
{

}

void StateEditSet::Update()
{
	StatePlayerImpl::Update();

	if (updatePreview)
	{
		auto state = runner.GetState();
		if (state.framesRdy > 3)
		{
			UpdateFPS(state.framesRdy);
			AskDraw();
			runner.GetState(true);
		}

		runner.Update();
	}
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

void StateEditSet::Draw(Mat& frame)
{
	StatePlayer::Draw(frame);

	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	auto pos = window->GetCurrentPosition();
	bool drawState = false;
	if (pos == set->timeStart)
		set->Draw(frame);
	else
		drawState = true;

	calculator.Draw(set, frame, window->GetCurrentPosition(), false, drawState);
}

void StateEditSet::SetPreview(bool b)
{
	if (updatePreview == b)
		return;

	if (b)
	{
		runner.Setup();
	}

	updatePreview = b;
}

void StateEditSet::UpdateButtons(ButtonListOut out)
{
	StateBase::UpdateButtons(out);
	auto me = this;

	AddButton(out, "Copy tracking here", [me](auto w) { me->CopySet(); }, KC_A);

	// Live tracking
	string updateText = "Auto update (";
	updateText.append(updatePreview ? "Y" : "N");
	updateText.append(")");
	AddButton(out, updateText, [me](TrackingWindow* w) {
		me->SetPreview(!me->updatePreview);
		w->DrawWindow(true);
	});

	// Delete
	GuiButtonExpand* deleteBtn = new GuiButtonExpand(GuiButton::Next(out), "Delete", KC_MINUS);
	deleteBtn->AddButton(new GuiButton(deleteBtn->Next(), [me]() {
		me->window->DeleteTrackingSet(me->set);
		me->Pop();
	}, "I am sure", KC_Y));
	out.emplace_back(deleteBtn);

	// Run tracking
	out.emplace_back(new GuiButton(GuiButton::Next(out), [me]() {
		me->window->PushState(new StateTracking(me->window, me->set));
	}, "Track", KC_T));

	
	// Set range here
	AddButton(out, "Range update", [me](auto w) {
		w->PushState(new StateEditRange(w, me->set, w->GetCurrentPosition()));
	});

	// Toggle bad
	AddButton(out, "Toggle frame", [me](auto w) {
		time_t p = w->GetCurrentPosition();
		auto frameEvent = me->set->events->GetEvent(p, EventType::TET_BADFRAME);
		if (frameEvent)
		{
			me->set->events->ClearEvents([frameEvent](EventPtr e) {
				return !(e->time == frameEvent->time && e->type == frameEvent->type);
			});
		}
		else
		{
			me->set->events->AddEvent(EventType::TET_BADFRAME, p);
		}

		me->AskDraw();
	}, KC_B);
	 
	// Tracking mode
	GuiButtonExpand* modeBtn = new GuiButtonExpand(GuiButton::Next(out), "Mode: " + TrackingModeToString(set->trackingMode));
	for (auto& t : magic_enum::enum_entries<TrackingMode>())
	{
		auto& b = modeBtn->AddButton(new GuiButton(modeBtn->Next(), [me, t]() {
			me->set->trackingMode = t.first;
			me->modeOpen = false;

			if (me->updatePreview)
			{
				me->SetPreview(false);
				me->SetPreview(true);
			}

			me->window->DrawWindow(true);
		}, TrackingModeToString(t.first)));
	}
	out.emplace_back(modeBtn);

	// Target list

	int y = 220;
	Rect r;
	GuiButton* b;

	for (int i = 0; i < set->targets.size(); i++)
	{
		TrackingTarget* t = &set->targets.at(i);
		TrackingSetPtr s = set;

		r = Rect(
			380,
			y,
			230,
			40
		);

		b = &AddButton(
			out,
			t->GetName(),
			[s, t](auto w) {
				w->PushState(new StateEditTarget(w, s, t));
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
			[me, i](auto w) {
				auto& tv = me->set->targets;
				tv.erase(tv.begin() + i);

				for (int c = 0; c < tv.size(); c++)
					tv.at(c).UpdateColor(c);

				w->DrawWindow(true);
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

	// Target + 

	b = &AddButton(
		out,
		"Targets",
		[me](auto w) {

			me->set->targets.emplace_back();
			TrackingTarget* t = &me->set->targets.back();
			t->UpdateColor(me->set->targets.size());

			if (me->set->targets.size() == 1)
				t->targetType = TARGET_TYPE::TYPE_FEMALE;
			else if (me->set->targets.size() == 2)
				t->targetType = TARGET_TYPE::TYPE_MALE;
			else if (me->set->targets.size() == 3)
				t->targetType = TARGET_TYPE::TYPE_BACKGROUND;
			else
				t->targetType = TARGET_TYPE::TYPE_UNKNOWN;

			me->window->stack.PushState(new StateEditTarget(w, me->set, t));
		},
		r,
		KC_ADD
	);
}

void StateEditSet::CopySet()
{
	TrackingSetPtr s = window->AddSet();
	if (s != nullptr)
	{
		int i = 0;

		for (auto& t : set->targets)
		{
			s->targets.emplace_back();
			auto& newTarget = s->targets.back();

			EventPtr state = s->events->GetEvent(window->GetCurrentPosition(), EventType::TET_STATE, nullptr, true);

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
}