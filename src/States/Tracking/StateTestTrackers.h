#pragma once

#include "States/StatePlayer.h"
#include "Model/TrackingRunner.h"

class StateTestTrackers : public StatePlayer
{
public:
	StateTestTrackers(TrackingWindow* window, TrackingSetPtr set, TrackingTarget* target);
	std::string GetName() { return "TestTrackers"; }

	void UpdateButtons(ButtonListOut out);
	void Update();
	void SetPlaying(bool b);
	void Draw(cv::Mat& frame);
	void EnterState(bool again);

protected:
	void NextFrame() override;
	void LastFrame() override {} ;

	TrackingTarget* target;
	TrackingSetPtr set;
	TrackingRunner runner;
};