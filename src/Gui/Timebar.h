#pragma once

#include "States/States.h"
#include "GuiButton.h"

#include <string>
#include <vector>
#include <opencv2/core.hpp>

class Timebar : public StateBase
{
public:
	Timebar(TrackingWindow* window);
	void AddGui(cv::Mat& frame);
	void UpdateButtons(std::vector<GuiButton>& out);

	void SelectTrackingSet(TrackingSet* s);
	TrackingSet* GetSelectedSet() { return selectedSet; }
	TrackingSet* GetNextSet();
	TrackingSet* GetPreviousSet();

	std::string GetName() const { return "Timebar"; }

protected:
	TrackingSet* selectedSet = nullptr;
};
