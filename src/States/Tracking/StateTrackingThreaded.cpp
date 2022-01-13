#include "StateTrackingThreaded.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/FrameCache.h"

using namespace cv;
using namespace std;
using namespace chrono;

StateTrackingThreaded::StateTrackingThreaded(TrackingWindow* window, TrackingSet* set)
	:StateTracking(window, set)
{

}

void StateTrackingThreaded::EnterState(bool again)
{
	StateTracking::EnterState(again);


}

void StateTrackingThreaded::LeaveState()
{
	StateTracking::LeaveState();

	SetPlaying(false);
}

void StateTrackingThreaded::Update()
{
	if (!playing)
		return;

	// Push work
	if (newWork.size() < maxWork)
	{
		window->ReadCleanFrame();
		cuda::GpuMat gpuFrame = window->GetInFrame();
		time_t time = window->GetCurrentPosition();

		lock_guard<mutex> lock(newMtx);
		for (auto& b : trackerBindings)
		{
			newWork.emplace_front(*b, gpuFrame, time);
		}
	}

	// Pop work
	while (doneWork.size() > 0)
	{
		doneMtx.lock();
		threadWork w = doneWork.back();
		doneWork.pop_back();
		doneMtx.unlock();

		if (w.frameTime == lastTime)
			continue;

		lastTime = w.frameTime;

		set->timeEnd = lastTime;
		calculator.Update(*set, lastTime);
		framesRdy++;
	}

	if (framesRdy > 4)
	{
		AskDraw();
	}
}

void StateTrackingThreaded::NextFrame()
{

}

void StateTrackingThreaded::SetPlaying(bool p)
{
	StateTracking::SetPlaying(p);

	if (p)
	{
		for (int i = 0; i < numThreads; i++)
		{
			threads.emplace_back(&StateTrackingThreaded::RunThread, this);
		}
	}
	else
	{
		for (auto& t : threads) {
			if (!t.joinable())
				continue;

			t.join();
		}
	}
}

void StateTrackingThreaded::RunThread()
{
	while (playing)
	{
		newMtx.lock();
		threadWork w = newWork.back();
		newWork.pop_back();
		newMtx.unlock();
		
		lock_guard<mutex> lock(w.binding.mtx);

		auto now = high_resolution_clock::now();
		if (!w.binding.tracker->update(w.frame))
			w.err = true;

		w.durationMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
		w.binding.lastUpdateMs = w.durationMs;
		w.binding.state->SnapResult(*set, w.frameTime);

		{
			lock_guard<mutex> lock(doneMtx);
			doneWork.push_front(w);
		}
	}
}