#pragma once

#include "States/States.h"

#include <vector>
#include <string>
#include <functional>

typedef std::function<void(TrackingTarget t)> AddTrackerCallback;
class StateEditTarget : public StateBase
{
public:
	StateEditTarget(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);

	void EnterState(bool again);

	void AddPoints(cv::Rect r);
	void RemovePoints(cv::Rect r);

	bool HandleMouse(int e, int x, int y, int f);

	void UpdateButtons(ButtonListOut out);
	void Draw(cv::Mat& frame);
	std::string GetName() { return "EditTracker"; }

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