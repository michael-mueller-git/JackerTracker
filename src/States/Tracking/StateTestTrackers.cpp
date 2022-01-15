#include "StateTestTrackers.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/FrameCache.h"

#include <opencv2/imgproc.hpp>
#include <magic_enum.hpp>

using namespace cv;
using namespace std;
using namespace chrono;

StateTestTrackers::StateTestTrackers(TrackingWindow* window, TrackingSetPtr set, TrackingTarget* target)
	:StatePlayer(window), set(set), target(target), runner(window, set, target, false, true, false)
{

}

void StateTestTrackers::Update()
{
	if (!playing)
		return;

	auto state = runner.GetState();
	if (state.framesRdy > 0)
	{
		UpdateFPS(state.framesRdy);
		AskDraw();
		runner.GetState(true);
	}

	runner.Update();

	//SyncFps();
}

void StateTestTrackers::UpdateButtons(ButtonListOut out)
{
	StatePlayer::UpdateButtons(out);

	if (!playing)
	{
		auto me = this;

		for (auto& b : runner.bindings)
		{
			TrackerJTType t = b->trackerStruct.type;

			GuiButton& btn = AddButton(out, "Pick " + b->trackerStruct.name, [me, t](auto w) {
				me->target->preferredTracker = t;
				me->Pop();
			});

			btn.textColor = b->state->color;
		}

	}
}

void StateTestTrackers::EnterState(bool again)
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

void StateTestTrackers::SetPlaying(bool b)
{
	StatePlayer::SetPlaying(b);
	runner.SetRunning(playing);
}

void StateTestTrackers::NextFrame()
{
	runner.Update(true);
	AskDraw();
}

void StateTestTrackers::Draw(Mat& frame)
{
	if (playing)
		putText(frame, "Press space to pause", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	runner.Draw(frame);
}