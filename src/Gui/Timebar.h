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
	void Draw(cv::Mat& frame);
	void UpdateButtons(ButtonListOut out);

	void SelectTrackingSet(TrackingSet* s);
	TrackingSet* GetSelectedSet() { return selectedSet; }
	TrackingSet* GetNextSet();
	TrackingSet* GetPreviousSet();

	std::string GetName() { return "Timebar"; }

protected:
	TrackingSet* selectedSet = nullptr;
};

class TimebarButton : public GuiButton
{
public:
	TimebarButton(TrackingWindow* w, TrackingSet* set, cv::Rect bar);
	bool Highlighted() override;
protected:
	void Handle() override;

	TrackingSet* set = nullptr;
	TrackingWindow* w;
};