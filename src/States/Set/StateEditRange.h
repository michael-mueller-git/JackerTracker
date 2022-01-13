#pragma once

#include "States/States.h"
#include "Model/Calculator.h"

class StateEditRange : public StateBase
{
public:
	StateEditRange(TrackingWindow* window, TrackingSet* set, time_t time);

	void Draw(cv::Mat& frame);
	void EnterState(bool again);
	void LeaveState();
	void Update();
	void Recalc();

	std::string GetName() { return "EditRange"; }

protected:
	TrackingSet* set;
	time_t time;
	TrackingCalculator calculator;
	TrackingEvent* rangeEvent;
};