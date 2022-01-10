#pragma once

#include "States.h"
#include "StatePlayer.h"

#include <string>

class StateEditSet : public StatePlayer
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set);

	void EnterState();
	void LeaveState();

	bool HandleInput(char c);
	void AddGui(cv::Mat& frame);
	virtual void UpdateButtons(std::vector<GuiButton>& out) override;
	std::string GetName() const { return "EditSet"; }

protected:
	TrackingSet* set;
	bool modeOpen = false;
};