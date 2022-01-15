#include "StateConfigTracker.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/FrameCache.h"
#include "Tracking/GpuTrackerPoints.h"

#include <opencv2/highgui.hpp>

using namespace cv;
using namespace std;

StateConfigTracker::StateConfigTracker(TrackingWindow* window, TrackingSet* set, TrackingTarget* target)
	:StateTestTrackers(window, set, target)
{

}

void StateConfigTracker::EnterState(bool again)
{
	window->timebar.SelectTrackingSet(set);

	cuda::GpuMat gpuFrame = window->GetInFrame();

	auto me = this;

	if (trackerBindings.size() > 0)
	{
		trackerBindings.clear();
	}

	auto s = GetTracker(target->preferredTracker);
	if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
	{
		Pop();
		return;
	}

	namedWindow("Settings");
	if (s.type == TrackerJTType::GPU_POINTS_PYLSPRASE)
	{
		if (!again)
			params = new GpuTrackerPoints::Params();

		GpuTrackerPoints::Params* p = (GpuTrackerPoints::Params*)params;
		auto restart = [](int v, void* p) { ((StateConfigTracker*)p)->Restart(); };

		createTrackbar("Size", "Settings", &p->size, 30, restart, this);
		createTrackbar("Level", "Settings", &p->maxLevel, 20, restart, this);
		createTrackbar("Iters", "Settings", &p->iters, 200, restart, this);
	}
}

void StateConfigTracker::LeaveState()
{
	if (getWindowProperty("Settings", WND_PROP_VISIBLE))
		destroyWindow("Settings");

	if (params)
		delete params;
}

void StateConfigTracker::Update()
{
	StateTestTrackers::Update();

	if (!getWindowProperty("Settings", WND_PROP_VISIBLE))
		Pop();
}

void StateConfigTracker::Restart()
{

}