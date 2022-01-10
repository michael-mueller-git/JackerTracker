#pragma once

#include "States.h"
#include "Model/Calculator.h"

#include <chrono>

class StatePlayer : public StateBase
{
public:
	using StateBase::StateBase;

	virtual void Update();
	void EnterState();
	void UpdateButtons(std::vector<GuiButton>& out);

	void UpdateFPS();
	virtual bool HandleInput(char c);
	virtual void AddGui(cv::Mat& frame);
	void SyncFps();

	virtual std::string GetName() const { return "Playing"; }

protected:
	bool playing = false;
	std::chrono::steady_clock::time_point timer;
	unsigned int timerFrames;
	int drawFps;
	std::chrono::time_point<std::chrono::steady_clock> lastUpdate;
	TrackingSet* lastSet = nullptr;
	TrackingCalculator calculator;
};