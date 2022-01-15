#pragma once

#include "States/States.h"
#include "States/StatePlayer.h"
#include "Tracking/Trackers.h"
#include "Model/TrackingRunner.h"

class StateTracking : public StatePlayer
{
public:
	StateTracking(TrackingWindow* window, TrackingSetPtr set);

	void EnterState(bool again);
	void UpdateButtons(ButtonListOut out);
	bool RemovePoints(cv::Rect r);
	void Update();
	void SetPlaying(bool b);

	void Draw(cv::Mat& frame);
	std::string GetName() { return "Tracking"; }

protected:
	void NextFrame() override;
	void LastFrame() override {} ;

	TrackingSetPtr set;
	TrackingRunner runner;
};