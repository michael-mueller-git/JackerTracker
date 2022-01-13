#pragma once

#include "StateTracking.h"

#include <deque>

class StateTrackingThreaded : public StateTracking
{
public:
	StateTrackingThreaded(TrackingWindow* window, TrackingSet* set);

	void EnterState(bool again);
	void LeaveState();
	void Update();

	std::string GetName() { return "TrackingThreaded"; }

protected:
	void NextFrame() override;
	void RunThread();
	void SetPlaying(bool p) override;

	struct threadWork {
		threadWork(trackerBinding& binding, cv::cuda::GpuMat frame, time_t frameTime)
			:binding(binding), frame(frame), frameTime(frameTime)
		{

		}

		trackerBinding& binding;
		cv::cuda::GpuMat frame;
		time_t frameTime;

		int durationMs = 999;
		bool err = false;
	};

	std::vector<std::thread> threads;
	
	std::mutex newMtx;
	std::deque<threadWork> newWork;

	std::mutex doneMtx;
	std::deque<threadWork> doneWork;


	time_t lastTime = 0;

	const int numThreads = 4;
	const int maxWork = 20;
	int framesRdy = 0;
};