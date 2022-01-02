#pragma once

#include <Windows.h>

#include "Main.h"
#include "Trackers.h"

#include <chrono>
#include <string>
#include <functional>

#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

using namespace chrono;
using namespace std;

class TrackingWindow;
typedef function<void()> StateFailedCallback;

class StateBase {
public:
	StateBase(TrackingWindow* window)
		:window(window)
	{
		auto me = this;
		AddButton("Cancel (q)", [me]() {
			me->Pop();
		});
	}

	virtual void EnterState() {};
	virtual void LeaveState() {};

	virtual void AddGui(Mat& frame) { ; }
	virtual void Update() {};
	virtual void HandleInput(char c) {};
	virtual void HandleMouse(int e, int x, int y, int f) {};
	virtual string GetName() const = 0;

	bool ShouldPop() { return popMe; };
	void Pop() { popMe = true; };
	void AddButton(string text, function<void()> onClick)
	{
		GuiButton b;
		b.text = text;
		b.onClick = onClick;

		b.rect = Rect(
			40,
			220 + (buttons.size() * (40 + 20)),
			230,
			40
		);

		buttons.push_back(b);
	}

	void AddButton(string text, char c)
	{
		auto me = this;

		AddButton(text + " (" + c + ")", [me, c]() {
			me->HandleInput(c);
		});
	}

	vector<GuiButton> buttons;

protected:
	TrackingWindow* window;
	bool popMe = false;
};



class StatePaused : public StateBase
{
public:
	StatePaused(TrackingWindow* window);

	void HandleInput(char c);
	void AddGui(Mat& frame);
	void HandleMouse(int e, int x, int y, int f);

	string GetName() const { return "Paused"; }
};

class StatePlaying : public StateBase
{
public:
	using StateBase::StateBase;

	virtual void Update();

	void EnterState();

	virtual void HandleInput(char c);
	virtual void AddGui(Mat& frame);

	virtual string GetName() const { return "Playing"; }

protected:
	steady_clock::time_point timer;
	unsigned int timerFrames;
	int drawFps;
};

class StateEditSet : public StateBase
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set);

	void EnterState();
	void LeaveState();

	void HandleInput(char c);
	void HandleMouse(int e, int x, int y, int f);
	void AddGui(Mat& frame);
	string GetName() const { return "EditSet"; }

protected:
	TrackingSet* set;
};

enum DRAGGING_BUTTON
{
	BUTTON_NONE,
	BUTTON_LEFT,
	BUTTON_RIGHT
};

typedef function<void(TrackingTarget t)> AddTrackerCallback;
class StateAddTracker : public StateBase
{
public:
	StateAddTracker(TrackingWindow* window, AddTrackerCallback callback, StateFailedCallback failedCallback);

	void EnterState();

	void AddPoints(Rect r);
	void RemovePoints(Rect r);
	
	void LeaveState()
	{
		if (!returned)
			failedCallback();
	}

	void HandleInput(char c);
	void HandleMouse(int e, int x, int y, int f);

	void AddGui(Mat& frame);
	string GetName() const { return "AddTracker"; }

protected:
	bool returned = false;
	AddTrackerCallback callback;
	StateFailedCallback failedCallback;

	Point2f clickStartPosition;
	Point2f draggingPosition;

	bool dragging = false;
	bool draggingRect = false;
	DRAGGING_BUTTON draggingBtn = BUTTON_NONE;

	TrackingTarget newTarget;
};

class StateTracking : public StatePlaying
{
public:
	struct trackerBinding
	{
		trackerBinding(TrackingTarget* target, TrackingTarget::TrackingState* state, TrackerJT* tracker)
			:target(target), state(state), tracker(tracker)
		{

		};

		~trackerBinding()
		{
			delete tracker;
			delete state;
		}

		TrackingTarget* target;
		TrackerJT* tracker;
		TrackingTarget::TrackingState* state;
		time_point<steady_clock> lastUpdate;
		int lastUpdateMs = 0;
	};

	StateTracking(TrackingWindow* window, TrackingSet* set)
		:StatePlaying(window), set(set)
	{

	}

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();

	void AddGui(Mat& frame);
	string GetName() const { return "Tracking"; }

protected:
	TrackingSet* set;
	vector<trackerBinding> trackerBindings;

	bool playing = true;
};

class StateTrackResult : public StateBase
{
public:
	StateTrackResult(TrackingWindow* window, string result)
		:StateBase(window), result(result)
	{

	}

	void AddGui(Mat& frame);

	string GetName() const { return "TrackResult"; }
private:
	string result;
};