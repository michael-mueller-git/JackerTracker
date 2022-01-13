#pragma once

#include "States/States.h"
#include "States/StatePlayer.h"
#include "Model/TrackingEvent.h"

#include <string>

class StateEditSet : public StatePlayerImpl
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set);

	void EnterState(bool again);
	void LeaveState();

	void Draw(cv::Mat& frame);
	virtual void UpdateButtons(ButtonListOut out) override;
	std::string GetName() { return "EditSet"; }

protected:
	void CopySet();

	TrackingSet* set;
	bool modeOpen = false;
};