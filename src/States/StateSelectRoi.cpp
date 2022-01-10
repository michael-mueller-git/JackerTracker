#include "StateSelectRoi.h"
#include "Gui/TrackingWindow.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;

bool StateSelectRoi::HandleMouse(int e, int x, int y, int f)
{
	if (p1.x == -1 && p1.y == -1 && e == EVENT_LBUTTONDOWN)
	{
		p1.x = x;
		p1.y = y;
		return true;
	}

	if (p1.x != -1 && p1.y != -1)
	{
		p2.x = x;
		p2.y = y;

		if (e == EVENT_LBUTTONUP && (abs(p1.x - p2.x) + abs(p1.y - p2.y)) > 5)
		{
			Rect r(p1, p2);
			callback(r);
			returned = true;
			Pop();
			return true;
		}

		if (e == EVENT_RBUTTONUP)
		{
			failedCallback();
			Pop();
			return true;
		}

		window->DrawWindow();
		return true;
	}

	return false;
}

void StateSelectRoi::AddGui(Mat& frame)
{
	putText(frame, title, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (p1.x != -1 && p1.y != -1 && p2.x != -1 && p2.y != -1)
	{
		rectangle(frame, Rect2f(p1, p2), Scalar(255, 0, 0), 2);
	}

	if (appendGui)
	{
		appendGuiFunction(frame);
	}
}