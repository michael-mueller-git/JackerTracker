#pragma once

#include "States/States.h"
#include "States/StatePlayer.h"
#include "Model/TrackingEvent.h"
#include "Model/TrackingRunner.h"

#include <string>

class StateEditSet : public StatePlayerImpl
{
public:
	StateEditSet(TrackingWindow* window, TrackingSetPtr set);

	void EnterState(bool again);
	void LeaveState();
	void Update();

	void Draw(cv::Mat& frame);
	virtual void UpdateButtons(ButtonListOut out) override;
	std::string GetName() { return "EditSet"; }

protected:
	void CopySet();
	void SetPreview(bool b);

	TrackingRunner runner;
	TrackingSetPtr set;
	bool modeOpen = false;
	bool updatePreview = false;
};