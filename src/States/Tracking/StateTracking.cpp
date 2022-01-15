#include "StateTracking.h"
#include "States/StateSelectRoi.h"
#include "Gui/TrackingWindow.h"
#include "Model/TrackingEvent.h"
#include "Tracking/FrameCache.h"

#include <opencv2/imgproc.hpp>
#include <magic_enum.hpp>

using namespace std;
using namespace cv;
using namespace chrono;
using namespace OIS;

StateTracking::StateTracking(TrackingWindow* window, TrackingSetPtr set)
	:StatePlayer(window), set(set), runner(window, set, nullptr, true, false, false)
{

}

void StateTracking::EnterState(bool again)
{
	StatePlayer::EnterState(again);

	if (!again)
	{
		if (!runner.Setup())
		{
			Pop();
			return;
		}

		SetPlaying(true);
	}
}

void StateTracking::SetPlaying(bool b)
{
	StatePlayer::SetPlaying(b);
	runner.SetRunning(playing);
}

void StateTracking::UpdateButtons(ButtonListOut out)
{
	StatePlayer::UpdateButtons(out);

	if (!playing)
	{
		auto me = this;

		/*
		AddButton(out, "Remove points", [me](auto w) {
			w->PushState(new StateSelectRoi(w, "Select which points to remove",
				[me](Rect& r) {
					if (!me->RemovePoints(r))
						return;


				},
				// Failed
					[]() {; },
					[me](Mat& frame) {
					for (auto& b : me->trackerBindings)
					{
						b->state->Draw(frame);
					}
				}
				));

		}, KC_R);
		*/

		AddButton(out, "Remove bads", [me](auto w) {
			me->set->events->ClearEvents([](EventPtr e) { return e->type != EventType::TET_BADFRAME; });
		});
	}
}

bool StateTracking::RemovePoints(Rect r)
{
	bool ret = false;

	/*
	for (auto& tb : trackerBindings)
	{
		if (tb->trackerStruct.type != TRACKING_TYPE::TYPE_POINTS)
			continue;

		for (int i = tb->state->points.size(); i > 0; i--)
		{
			auto& p = tb->state->points.at(i - 1);

			bool inside =
				p.point.x > r.x && p.point.x < r.x + r.width &&
				p.point.y > r.y && p.point.y < r.y + r.height;
			if (!inside)
				continue;

			tb->target->initialPoints.erase(tb->target->initialPoints.begin() + i - 1);
			ret = true;
		}
	}
	*/

	return ret;
}

void StateTracking::Draw(Mat& frame)
{
	if (playing)
		putText(frame, "Press space to pause", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	runner.Draw(frame);
}

void StateTracking::Update()
{
	if (!playing)
		return;

	auto state = runner.GetState();
	if (state.framesRdy > 3)
	{
		UpdateFPS(state.framesRdy);
		AskDraw();
		runner.GetState(true);
	}

	runner.Update();
	
	//SyncFps();
}

void StateTracking::NextFrame()
{
	runner.Update(true);
	AskDraw();
}