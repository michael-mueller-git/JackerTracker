#pragma once

#include "States.h"

typedef std::function<void()> ConfirmCallback;
class StateConfirm : public StateBase
{
public:
	StateConfirm(TrackingWindow* window, std::string title, ConfirmCallback callback, StateFailedCallback failedCallback);

	void UpdateButtons(std::vector<GuiButton>& out);
	bool HandleInput(char c);
	void AddGui(cv::Mat& frame);
	void LeaveState();

	std::string GetName() const { return "Confirm"; }

protected:
	std::string title;
	ConfirmCallback callback;
	StateFailedCallback failedCallback;
	bool confirmed = false;
};