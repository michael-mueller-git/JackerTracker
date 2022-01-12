#include "StateConfirm.h"

#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

StateConfirm::StateConfirm(TrackingWindow* window, string title, ConfirmCallback callback, StateFailedCallback failedCallback)
	:StateBase(window), callback(callback), failedCallback(failedCallback)
{

}

void StateConfirm::UpdateButtons(vector<GuiButton>& out)
{
	AddButton(out, "Yes", 'y');
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

bool StateConfirm::HandleInput(int c)
{
	if (c == 'y')
	{
		confirmed = true;
		Pop();
		return true;
	}

	return false;
}