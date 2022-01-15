#pragma once

#include "States/States.h"
#include "Model/Calculator.h"

class StateEditRange : public StateBase
{
public:
	StateEditRange(TrackingWindow* window, TrackingSetPtr set, time_t time);

	void Draw(cv::Mat& frame);
	void EnterState(bool again);
	void LeaveState();
	void Update();
	void Recalc();

	std::string GetName() { return "EditRange"; }

protected:
	TrackingSetPtr set;
	time_t time;
	TrackingCalculator calculator;
	EventPtr rangeEvent;
};