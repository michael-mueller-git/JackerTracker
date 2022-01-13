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

StateTracking::StateTracking(TrackingWindow* window, TrackingSet* set)
	:StatePlayer(window), set(set)
{

}

void StateTracking::EnterState(bool again)
{
	StatePlayer::EnterState(again);

	window->timebar.SelectTrackingSet(set);
	calculator.Reset();

	cuda::GpuMat gpuFrame = window->GetInFrame();

	if (trackerBindings.size() > 0)
	{
		trackerBindings.clear();
	}

	set->ClearEvents([](auto& e) { return e.type == EventType::TET_BADFRAME; });

	trackerBindings.reserve(set->targets.size());

	for (auto& t : set->targets)
	{
		TrackerJTType type = t.preferredTracker;

		if (type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
			if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
				type = TrackerJTType::GPU_POINTS_PYLSPRASE;
			else if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
				type = TrackerJTType::CPU_RECT_MEDIAN_FLOW;

		TrackerJTStruct jts = GetTracker(type);
		if (!t.SupportsTrackingType(jts.trackingType))
			continue;

		trackerBindings.emplace_back(make_unique<trackerBinding>(&t, jts));
		trackerBindings.back()->tracker->init(gpuFrame);
		trackerBindings.back()->state->UpdateColor(trackerBindings.size());
	}

	if (trackerBindings.size() == 0)
	{
		Pop();
		return;
	}

	playing = true;
	window->SetPlaying(playing);
}

void StateTracking::LeaveState()
{
	window->timebar.SelectTrackingSet(set);
}

void StateTracking::UpdateButtons(ButtonListOut out)
{
	StatePlayer::UpdateButtons(out);

	if (!playing)
	{
		auto me = this;

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

		AddButton(out, "Remove bads", [me](auto w) {
			me->set->ClearEvents([](auto& e) { return e.type != EventType::TET_BADFRAME; });
		});
	}
}

bool StateTracking::RemovePoints(Rect r)
{
	bool ret = false;

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

	return ret;
}

void StateTracking::Draw(Mat& frame)
{
	if (playing)
		putText(frame, "Press space to pause", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 100;

	for (auto& b : trackerBindings)
	{
		b->state->Draw(frame);
		putText(frame, format("%s: %dms", b->tracker->GetName(), b->lastUpdateMs), Point(400, y), FONT_HERSHEY_SIMPLEX, 0.6, b->state->color, 2);
		y += 20;
	}

	calculator.Draw(*set, frame, window->GetCurrentPosition());
}

void StateTracking::Update()
{
	if (!playing)
		return;

	UpdateFPS();
	SyncFps();
	NextFrame();
	
}

void StateTracking::LastFrame()
{
	
}

void StateTracking::NextFrame()
{
	cuda::Stream stream;

	window->ReadCleanFrame(stream);

	cuda::GpuMat gpuFrame = window->GetInFrame();
	time_t time = window->GetCurrentPosition();


	// Run all trackers on the new frame
	if (!set->GetResult(time, EventType::TET_BADFRAME))
	{
		for (auto& b : trackerBindings)
		{

			auto now = high_resolution_clock::now();
			if (!b->tracker->update(gpuFrame, stream))
			{
				set->AddEvent(EventType::TET_BADFRAME, time, b->target);
			}

			b->lastUpdateMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
		}
	}
	else
	{
		bool err = true;
	}

	stream.waitForCompletion();

	for (auto& b : trackerBindings)
	{
		b->state->SnapResult(*set, time);
	}

	set->timeEnd = time;
	calculator.Update(*set, time);


	AskDraw();
}