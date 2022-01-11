#pragma once

#include "States.h"
#include "StatePlayer.h"
#include "Model/TrackingEvent.h"

#include <string>

class StateEditSet : public StatePlayerImpl
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set);

	void EnterState(bool again);
	void LeaveState();

	bool HandleInput(int c);
	void AddGui(cv::Mat& frame);
	virtual void UpdateButtons(std::vector<GuiButton>& out) override;
	std::string GetName() const { return "EditSet"; }

protected:
	TrackingSet* set;
	bool modeOpen = false;
};

class StateEditRange : public StateBase
{
public:
	StateEditRange(TrackingWindow* window, TrackingSet* set, time_t time);

	void AddGui(cv::Mat& frame);
	void EnterState(bool again);
	void LeaveState();
	void Update();
	void Recalc();

	std::string GetName() const { return "EditRange"; }

protected:
	TrackingSet* set;
	time_t time;
	TrackingCalculator calculator;
	TrackingEvent* rangeEvent;
};