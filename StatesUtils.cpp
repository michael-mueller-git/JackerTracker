#include "StatesUtils.h"
#include "TrackingWindow.h"

StateConfirm::StateConfirm(TrackingWindow* window, string title, ConfirmCallback callback, StateFailedCallback failedCallback)
	:StateBase(window), callback(callback), failedCallback(failedCallback)
{
	AddButton("Yes", 'y');
}

void StateConfirm::AddGui(Mat& frame)
{
	putText(frame, title, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Yes: y", Point(30, 420), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StateConfirm::LeaveState()
{
	if (confirmed)
		callback();
	else
		failedCallback();
}

void StateConfirm::HandleInput(char c)
{
	if (c == 'y')
	{
		confirmed = true;
		Pop();
	}
}



void StateSelectRoi::HandleMouse(int e, int x, int y, int f)
{
	if (p1.x == -1 && p1.y == -1 && e == EVENT_LBUTTONUP)
	{
		p1.x = x;
		p1.y = y;
		return;
	}

	if (p1.x != -1 && p1.y != -1)
	{
		p2.x = x;
		p2.y = y;

		if (e == EVENT_LBUTTONUP)
		{
			Rect2f r(p1, p2);
			callback(r);
			returned = true;
			Pop();
			return;
		}

		window->DrawWindow();
	}
}

void StateSelectRoi::AddGui(Mat& frame)
{
	putText(frame, title, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (p1.x != -1 && p1.y != -1 && p2.x != -1 && p2.y != -1)
	{
		rectangle(frame, Rect2f(p1, p2), Scalar(255, 0, 0));
	}

	if (appendGui)
	{
		appendGuiFunction(frame);
	}
}