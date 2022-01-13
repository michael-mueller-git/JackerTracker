#pragma once

#include "StateTracking.h"

class StateTestTrackers : public StateTracking
{
public:
	StateTestTrackers(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);
	std::string GetName() { return "TestTrackers"; }

	void UpdateButtons(ButtonListOut out);

	void Draw(cv::Mat& frame);
	void EnterState(bool again);

protected:
	void NextFrame() override;

	TrackingTarget* target;
};