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

	void SelectTrackingSet(TrackingSetPtr s);
	TrackingSetPtr GetSelectedSet() { return selectedSet; }
	TrackingSetPtr GetNextSet();
	TrackingSetPtr GetPreviousSet();

	std::string GetName() { return "Timebar"; }

protected:
	TrackingSetPtr selectedSet = nullptr;
};

class TimebarButton : public GuiButton
{
public:
	TimebarButton(TrackingWindow* w, TrackingSetPtr set, cv::Rect bar);
	bool Highlighted() override;
protected:
	void Handle() override;

	TrackingSetPtr set = nullptr;
	TrackingWindow* w;
};