#pragma once

#include "States.h"

typedef function<void(Rect2f&)> SelectRoiCallback;
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

	void HandleMouse(int e, int x, int y, int f);
	void AddGui(Mat& frame);

	string GetName() const { return "SelectRoi"; }

protected:
	string title;
	bool returned = false;
	SelectRoiCallback callback;
	StateFailedCallback failedCallback;
	AppendGuiFunction appendGuiFunction;
	bool appendGui = false;

	Point2f p1;
	Point2f p2;
};

typedef function<void()> ConfirmCallback;
class StateConfirm : public StateBase
{
public:
	StateConfirm(TrackingWindow* window, string title, ConfirmCallback callback, StateFailedCallback failedCallback);

	void HandleInput(char c);
	void AddGui(Mat& frame);
	void LeaveState();

	string GetName() const { return "Confirm"; }

protected:
	string title;
	ConfirmCallback callback;
	StateFailedCallback failedCallback;
	bool confirmed = false;
};

