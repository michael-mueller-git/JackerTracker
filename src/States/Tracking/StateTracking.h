#pragma once

#include "States/States.h"
#include "States/StatePlayer.h"
#include "Tracking/Trackers.h"

class StateTracking : public StatePlayer
{
public:
	struct trackerBinding
	{
		trackerBinding(TrackingTarget* target, TrackerJTStruct& trackerStruct)
			:target(target), trackerStruct(trackerStruct)
		{
			state.reset(target->InitTracking(trackerStruct.trackingType));
			tracker.reset(trackerStruct.Create(*target, *state));
		};

		TrackerJTStruct trackerStruct;
		TrackingTarget* target;
		std::unique_ptr<TrackerJT> tracker;
		std::unique_ptr<TrackingStatus> state;
		int lastUpdateMs = 0;
		std::mutex mtx;
	};

	StateTracking(TrackingWindow* window, TrackingSet* set);

	void EnterState(bool again);
	void LeaveState();
	void UpdateButtons(ButtonListOut out);
	bool RemovePoints(cv::Rect r);
	void Update();

	void Draw(cv::Mat& frame);
	std::string GetName() { return "Tracking"; }

protected:
	void NextFrame() override;
	void LastFrame() override;

	TrackingSet* set;
	std::vector<std::unique_ptr<trackerBinding>> trackerBindings;
};