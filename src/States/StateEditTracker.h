#pragma once

#include "States.h"

#include <vector>
#include <string>
#include <functional>

typedef std::function<void(TrackingTarget t)> AddTrackerCallback;
class StateEditTracker : public StateBase
{
public:
	StateEditTracker(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);

	void AddPoints(cv::Rect r);
	void RemovePoints(cv::Rect r);

	bool HandleInput(char c);
	bool HandleMouse(int e, int x, int y, int f);

	void UpdateButtons(std::vector<GuiButton>& out);
	void AddGui(cv::Mat& frame);
	std::string GetName() const { return "EditTracker"; }

protected:
	cv::Point clickStartPosition;
	cv::Point draggingPosition;

	bool typeOpen = false;
	bool trackerOpen = false;

	bool dragging = false;
	bool draggingRect = false;
	DRAGGING_BUTTON draggingBtn = BUTTON_NONE;

	TrackingSet* set;
	TrackingTarget* target;
};