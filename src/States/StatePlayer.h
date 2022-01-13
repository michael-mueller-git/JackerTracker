#pragma once

#include "States.h"
#include "Model/Calculator.h"

#include <chrono>

class StatePlayer : public StateBase
{
public:
	StatePlayer(TrackingWindow* window);
	virtual void Update();

	void EnterState(bool again);
	void UpdateButtons(ButtonListOut out);

	
	virtual bool HandleStateInput(OIS::KeyCode c);
	virtual void Draw(cv::Mat& frame);
	
protected:
	virtual void NextFrame() = 0;
	virtual void LastFrame();
	void UpdateFPS();
	void SyncFps();
	virtual void SetPlaying(bool p);

	bool playing = false;
	std::chrono::steady_clock::time_point timer;
	unsigned int timerFrames;
	int drawFps;
	std::chrono::time_point<std::chrono::steady_clock> lastUpdate;
	TrackingSet* lastSet = nullptr;
	TrackingCalculator calculator;
};

class StatePlayerImpl : public StatePlayer
{
public:
	StatePlayerImpl(TrackingWindow* window);
	virtual std::string GetName() { return "Playing"; }
	virtual void NextFrame();
};