#pragma once

#include "Model/Model.h"
#include "States.h"

typedef std::function<void(cv::Rect&)> SelectRoiCallback;
typedef std::function<void(cv::Mat&)> AppendGuiFunction;
class StateSelectRoi : public StateBase
{
public:
	StateSelectRoi(TrackingWindow* window, std::string title, SelectRoiCallback callback, StateFailedCallback failedCallback)
		:StateBase(window), callback(callback), failedCallback(failedCallback), title(title)
		, p1(-1, -1), p2(-1, -1)
	{

	}

	StateSelectRoi(TrackingWindow* window, std::string title, SelectRoiCallback callback, StateFailedCallback failedCallback, AppendGuiFunction appendGuiFunction)
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
	void Draw(cv::Mat& frame);

	std::string GetName() { return "SelectRoi"; }

protected:
	std::string title;
	bool returned = false;
	SelectRoiCallback callback;
	StateFailedCallback failedCallback;
	AppendGuiFunction appendGuiFunction;
	bool appendGui = false;

	cv::Point p1;
	cv::Point p2;
};