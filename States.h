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
		
	}

	virtual void EnterState() {};
	virtual void LeaveState() {};

	virtual void UpdateButtons(vector<GuiButton>& out) 
	{
		auto me = this;

		AddButton(out, "Cancel (q)", [me]() {
			me->Pop();
		});
	};
	virtual void AddGui(Mat& frame) { ; }
	virtual void Update() {};
	virtual bool HandleInput(char c) { return false; };
	virtual bool HandleMouse(int e, int x, int y, int f) { return false; };
	virtual string GetName() const = 0;

	virtual bool ShouldPop() { return popMe; };
	void Pop() { popMe = true; };
	GuiButton& AddButton(vector<GuiButton>& out, string text, function<void()> onClick, Rect r)
	{
		GuiButton b;
		b.text = text;
		b.onClick = onClick;
		b.rect = r;

		out.push_back(b);

		return out.back();
	}
	GuiButton& AddButton(vector<GuiButton>& out, string text, function<void()> onClick)
	{
		int x = 40;
		int y = 220;

		for (auto& b : out)
			if (b.rect.x == x)
				y += (40 + 20);

		return AddButton(out, text, onClick, Rect(
			x,
			y,
			230,
			40
		));
	}

	GuiButton& AddButton(vector<GuiButton>& out, string text, char c)
	{
		auto me = this;

		return AddButton(out, text + " (" + c + ")", [me, c]() {
			me->HandleInput(c);
		});
	}

protected:
	TrackingWindow* window;
	bool popMe = false;
};

class StatePlayer : public StateBase
{
public:
	using StateBase::StateBase;

	virtual void Update();
	void EnterState();
	void UpdateButtons(vector<GuiButton>& out);

	void UpdateFPS();
	virtual bool HandleInput(char c);
	virtual void AddGui(Mat& frame);
	void SyncFps();

	virtual string GetName() const { return "Playing"; }

protected:
	bool playing = false;
	steady_clock::time_point timer;
	unsigned int timerFrames;
	int drawFps;
	time_point<steady_clock> lastUpdate;
	TrackingSet* lastSet = nullptr;
	TrackingCalculator calculator;
};

class StateEditSet : public StatePlayer
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set);

	void EnterState();
	void LeaveState();

	bool HandleInput(char c);
	void AddGui(Mat& frame);
	virtual void UpdateButtons(vector<GuiButton>& out) override;
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
	StateEditTracker(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);

	void AddPoints(Rect r);
	void RemovePoints(Rect r);
	
	bool HandleInput(char c);
	bool HandleMouse(int e, int x, int y, int f);

	virtual void UpdateButtons(vector<GuiButton>& out) override;
	void AddGui(Mat& frame);
	string GetName() const { return "EditTracker"; }

protected:
	Point2f clickStartPosition;
	Point2f draggingPosition;

	bool typeOpen = false;
	bool trackerOpen = false;

	bool dragging = false;
	bool draggingRect = false;
	DRAGGING_BUTTON draggingBtn = BUTTON_NONE;

	TrackingSet* set;
	TrackingTarget* target;
};

class StateTracking : public StatePlayer
{
public:
	struct trackerBinding
	{
		trackerBinding(TrackingTarget* target, TrackerJTStruct& trackerStruct)
			:target(target), trackerStruct(trackerStruct)
		{
			state = target->InitTracking(trackerStruct.trackingType);
			tracker = trackerStruct.Create(*target, *state);
		};

		~trackerBinding()
		{
			delete tracker;
			delete state;
		}

		TrackerJTStruct trackerStruct;
		TrackingTarget* target;
		TrackerJT* tracker;
		TrackingState* state;
		int lastUpdateMs = 0;
	};

	StateTracking(TrackingWindow* window, TrackingSet* set);

	void EnterState();
	void LeaveState();
	void UpdateButtons(vector<GuiButton>& out);
	bool HandleInput(char c);
	bool RemovePoints(Rect r);
	void Update();

	void AddGui(Mat& frame);
	string GetName() const { return "Tracking"; }

protected:
	TrackingSet* set;
	vector<trackerBinding> trackerBindings;
};

class StateTestTrackers : public StateTracking
{
public:
	StateTestTrackers(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);
	string GetName() const { return "TestTrackers"; }
	
	void UpdateButtons(vector<GuiButton>& out);
	void EnterState();

protected:
	TrackingTarget* target;
};