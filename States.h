#pragma once

#include <Windows.h>

#include "Main.h"
#include "Trackers.h"

#include <chrono>
#include <string>
#include <functional>

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudaoptflow.hpp>

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
		UpdateButtons();
	}

	virtual void EnterState() {};
	virtual void LeaveState() {};

	virtual void UpdateButtons() 
	{
		auto me = this;
		buttons.clear();

		AddButton("Cancel (q)", [me]() {
			me->Pop();
		});
	};
	virtual void AddGui(Mat& frame) { ; }
	virtual void Update() {};
	virtual void HandleInput(char c) {};
	virtual void HandleMouse(int e, int x, int y, int f) {};
	virtual string GetName() const = 0;

	bool ShouldPop() { return popMe; };
	void Pop() { popMe = true; };
	GuiButton& AddButton(string text, function<void()> onClick, Rect r)
	{
		GuiButton b;
		b.text = text;
		b.onClick = onClick;
		b.rect = r;

		buttons.push_back(b);
		return buttons.back();
	}
	GuiButton& AddButton(string text, function<void()> onClick)
	{
		return AddButton(text, onClick, Rect(
			40,
			220 + (buttons.size() * (40 + 20)),
			230,
			40
		));
	}

	GuiButton& AddButton(string text, char c)
	{
		auto me = this;

		return AddButton(text + " (" + c + ")", [me, c]() {
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
	virtual void UpdateButtons() override;
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
class StateEditTracker : public StateBase
{
public:
	StateEditTracker(TrackingWindow* window, TrackingTarget* target);

	void EnterState();

	void AddPoints(Rect r);
	void RemovePoints(Rect r);
	
	void HandleInput(char c);
	void HandleMouse(int e, int x, int y, int f);

	virtual void UpdateButtons() override;
	void AddGui(Mat& frame);
	string GetName() const { return "EditTracker"; }

protected:
	Point2f clickStartPosition;
	Point2f draggingPosition;

	bool dragging = false;
	bool draggingRect = false;
	DRAGGING_BUTTON draggingBtn = BUTTON_NONE;

	TrackingTarget* target;
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
		int lastUpdateMs = 0;
	};

	StateTracking(TrackingWindow* window, TrackingSet* set, bool test);

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();

	void AddGui(Mat& frame);
	string GetName() const { return "Tracking"; }

protected:
	bool test = false;
	TrackingSet* set;
	vector<trackerBinding> trackerBindings;

	bool playing = true;
};

class StateTrackResult : public StateBase
{
public:
	StateTrackResult(TrackingWindow* window, TrackingSet* set, string result)
		:StateBase(window), set(set), result(result)
	{

	}

	void EnterState();
	void AddGui(Mat& frame);

	string GetName() const { return "TrackResult"; }
protected:
	TrackingSet* set;
	string result;
};