#pragma once

#include "Trackers.h"
#include "States.h"

typedef function<void(Rect&)> SelectRoiCallback;
typedef function<void(Mat&)> AppendGuiFunction;
class StateSelectRoi : public StateBase
{
public:
	StateSelectRoi(TrackingWindow* window, string title, SelectRoiCallback callback, StateFailedCallback failedCallback)
		:StateBase(window), callback(callback), failedCallback(failedCallback), title(title)
		, p1(-1, -1), p2(-1, -1)
	{

	}

	StateSelectRoi(TrackingWindow* window, string title, SelectRoiCallback callback, StateFailedCallback failedCallback, AppendGuiFunction appendGuiFunction)
		:StateBase(window), callback(callback), failedCallback(failedCallback), appendGuiFunction(appendGuiFunction), appendGui(true), title(title)
		, p1(-1, -1), p2(-1, -1)
	{

	}

	void LeaveState()
	{
		if (!returned)
			failedCallback();
	}

	bool HandleMouse(int e, int x, int y, int f);
	void AddGui(Mat& frame);

	string GetName() const { return "SelectRoi"; }

protected:
	string title;
	bool returned = false;
	SelectRoiCallback callback;
	StateFailedCallback failedCallback;
	AppendGuiFunction appendGuiFunction;
	bool appendGui = false;

	Point p1;
	Point p2;
};

typedef function<void()> ConfirmCallback;
class StateConfirm : public StateBase
{
public:
	StateConfirm(TrackingWindow* window, string title, ConfirmCallback callback, StateFailedCallback failedCallback);

	void UpdateButtons(vector<GuiButton>& out);
	bool HandleInput(char c);
	void AddGui(Mat& frame);
	void LeaveState();

	string GetName() const { return "Confirm"; }

protected:
	string title;
	ConfirmCallback callback;
	StateFailedCallback failedCallback;
	bool confirmed = false;
};

class MultiState : public StateBase
{
public:
	MultiState(TrackingWindow* window, StateBase* primaryState)
		:StateBase(window)
	{
		AddState(primaryState, false);
	}

	~MultiState()
	{
		if (states.size() > 1)
			for (auto& s : states)
				if (s.own)
					delete s.state;
	}

	void AddState(StateBase* state, bool own)
	{
		states.push_back({ state , own});
	}

	void EnterState()
	{
		states.at(0).state->EnterState();
	};

	void LeaveState()
	{
		states.at(0).state->LeaveState();
	};

	void UpdateButtons(vector<GuiButton>& out)
	{
		for (int s = states.size(); s > 0; s--)
		{
			states.at(s - 1).state->UpdateButtons(out);
		}
	}

	void AddGui(Mat& frame)
	{
		for (auto s : states)
			s.state->AddGui(frame);
	}

	void Update()
	{
		for (auto s : states)
			s.state->Update();
	};

	bool HandleInput(char c)
	{
		for (auto s : states)
			if (s.state->HandleInput(c))
				return true;

		return false;
	}

	bool HandleMouse(int e, int x, int y, int f)
	{
		for (auto s : states)
			if (s.state->HandleMouse(e, x, y, f))
				return true;

		return false;
	}

	string GetName() const override
	{
		return states.at(0).state->GetName();
	}

	bool ShouldPop() override
	{
		return states.at(0).state->ShouldPop();
	};
	

protected:
	struct stateStruct {
		StateBase* state;
		bool own;
	};
	vector<stateStruct> states;
};

class StateGlobal : public StateBase
{
public:
	StateGlobal(TrackingWindow* window)
		:StateBase(window)
	{

	}

	void UpdateButtons(vector<GuiButton>& out)
	{
		AddButton(out, "Save", 's');
	}

	bool HandleInput(char c) override;

	string GetName() const { return "Global"; }
};