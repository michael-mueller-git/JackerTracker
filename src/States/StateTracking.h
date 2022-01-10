#pragma once

#include "States.h"
#include "StatePlayer.h"
#include "Tracking//Trackers.h"

class StateTracking : public StatePlayer
{
public:
	struct trackerBinding
	{
		trackerBinding(TrackingTarget* target, TrackerJTStruct& trackerStruct)
			:target(target), trackerStruct(trackerStruct)
		{
			state = target->InitTracking(trackerStruct.trackingType);
			tracker = trackerStruct.Create(*target, *state);
		};

		~trackerBinding()
		{
			delete tracker;
			delete state;
		}

		TrackerJTStruct trackerStruct;
		TrackingTarget* target;
		TrackerJT* tracker;
		TrackingStatus* state;
		int lastUpdateMs = 0;
	};

	StateTracking(TrackingWindow* window, TrackingSet* set);

	void EnterState();
	void LeaveState();
	void UpdateButtons(std::vector<GuiButton>& out);
	bool HandleInput(char c);
	bool RemovePoints(cv::Rect r);
	void Update();

	void AddGui(cv::Mat& frame);
	std::string GetName() const { return "Tracking"; }

protected:
	TrackingSet* set;
	std::vector<trackerBinding> trackerBindings;
};

class StateTestTrackers : public StateTracking
{
public:
	StateTestTrackers(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);
	std::string GetName() const { return "TestTrackers"; }

	void UpdateButtons(std::vector<GuiButton>& out);
	void EnterState();

protected:
	TrackingTarget* target;
};