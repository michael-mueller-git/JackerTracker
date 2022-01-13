#include "StateTestTrackers.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/FrameCache.h"

#include <opencv2/imgproc.hpp>
#include <magic_enum.hpp>

using namespace cv;
using namespace std;
using namespace chrono;

StateTestTrackers::StateTestTrackers(TrackingWindow* window, TrackingSet* set, TrackingTarget* target)
	:StateTracking(window, set), target(target)
{

}

void StateTestTrackers::UpdateButtons(ButtonListOut out)
{
	StateTracking::UpdateButtons(out);

	if (!playing)
	{
		auto me = this;

		for (auto& b : trackerBindings)
		{
			std::unique_ptr<trackerBinding>* bptr = &b;

			GuiButton& btn = AddButton(out, "Pick " + b->trackerStruct.name, [me, bptr](auto w) {
				me->target->preferredTracker = bptr->get()->trackerStruct.type;
				me->Pop();
			});

			btn.textColor = b->state->color;
		}

	}
}

void StateTestTrackers::EnterState(bool again)
{
	window->timebar.SelectTrackingSet(set);

	cuda::GpuMat gpuFrame = window->GetInFrame();

	constexpr auto types = magic_enum::enum_entries<TrackerJTType>();
	auto me = this;

	if (trackerBindings.size() > 0)
	{
		trackerBindings.clear();
	}

	trackerBindings.reserve(types.size());

	for (auto& t : types)
	{
		auto s = GetTracker(t.first);
		if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
			continue;

		if (target->SupportsTrackingType(s.trackingType))
		{
			trackerBindings.emplace_back(make_unique<trackerBinding>(target, s));
			trackerBindings.back()->tracker->init(gpuFrame);
			trackerBindings.back()->state->UpdateColor(trackerBindings.size());
		}
	}

	if (trackerBindings.size() == 0)
	{
		Pop();
		return;
	}

	SetPlaying(true);
}

void StateTestTrackers::NextFrame()
{
	cuda::Stream stream;

	window->ReadCleanFrame(stream);

	cuda::GpuMat gpuFrame = window->GetInFrame();
	time_t time = window->GetCurrentPosition();

	for (auto& b : trackerBindings)
	{
		auto now = high_resolution_clock::now();
		b->tracker->update(gpuFrame, stream);
		b->lastUpdateMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
	}

	stream.waitForCompletion();
	AskDraw();
}

void StateTestTrackers::Draw(Mat& frame)
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
}