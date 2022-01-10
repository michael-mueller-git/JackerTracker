#include "StateEditTracker.h"
#include "StateSelectRoi.h"
#include "StateTracking.h"

#include "Gui/TrackingWindow.h"
#include "Tracking/Trackers.h"

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <magic_enum.hpp>

using namespace std;
using namespace cv;

StateEditTracker::StateEditTracker(TrackingWindow* window, TrackingSet* set, TrackingTarget* target)
	:StateBase(window), set(set), target(target)
{

}

void StateEditTracker::UpdateButtons(vector<GuiButton>& out)
{
	auto me = this;

	AddButton(out, "Done (q)", [me]() {
		me->Pop();
		});

	AddButton(out, "Set rect", 'r');
	auto t = GetTracker(target->preferredTracker);

	auto& trackerBtn = AddButton(out, "Tracker: " + t.name, [me]() {
		if (me->trackerOpen)
		{
			me->trackerOpen = false;
			me->window->DrawWindow(true);
			return;
		}

		if (me->typeOpen)
			me->typeOpen = false;

		me->trackerOpen = true;
		me->window->DrawWindow(true);
		});

	if (trackerOpen)
	{
		constexpr auto types = magic_enum::enum_entries<TrackerJTType>();

		int x = trackerBtn.rect.x + trackerBtn.rect.width + 20;

		for (auto& t : types)
		{
			auto s = GetTracker(t.first);
			if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
				continue;

			Rect r = trackerBtn.rect;
			r.width = 180;
			r.x = x;
			x += r.width + 20;

			GuiButton newBtn;
			newBtn.rect = r;
			newBtn.text = s.name;
			newBtn.textScale = 0.5;
			newBtn.onClick = [me, t]() {
				me->target->preferredTracker = t.first;
				me->trackerOpen = false;
				me->window->DrawWindow(true);
			};

			out.push_back(newBtn);
		}
	}

	auto& typeBtn = AddButton(out, "Type: " + TargetTypeToString(target->targetType), [me]() {
		if (me->typeOpen)
		{
			me->typeOpen = false;
			me->window->DrawWindow(true);
			return;
		}

		if (me->trackerOpen)
			me->trackerOpen = false;

		me->typeOpen = true;
		me->window->DrawWindow(true);
		});

	if (typeOpen)
	{
		constexpr auto types = magic_enum::enum_entries<TARGET_TYPE>();

		int x = typeBtn.rect.x + typeBtn.rect.width + 20;

		for (auto& t : types)
		{
			if (t.first == TARGET_TYPE::TYPE_UNKNOWN)
				continue;

			Rect r = typeBtn.rect;
			r.x = x;
			x += r.width + 20;

			GuiButton newBtn;
			newBtn.rect = r;
			newBtn.text = TargetTypeToString(t.first);
			newBtn.textScale = 0.5;
			newBtn.onClick = [me, t]() {
				me->target->targetType = t.first;
				me->typeOpen = false;
				me->window->DrawWindow(true);
			};

			out.push_back(newBtn);
		}
	}

	AddButton(out, "Test tracking", 'f');
}

void StateEditTracker::RemovePoints(Rect r)
{
	vector<Point> newPoints;

	copy_if(
		target->intialPoints.begin(),
		target->intialPoints.end(),
		back_inserter(newPoints),
		[r](Point p)
		{
			bool inside =
				p.x > r.x && p.x < r.x + r.width &&
				p.y > r.y && p.y < r.y + r.height;

			return !inside;
		}
	);

	target->intialPoints = newPoints;
	target->UpdateType();

	window->DrawWindow();
}

void StateEditTracker::AddPoints(Rect r)
{
	cuda::GpuMat* gpuFrame = window->GetInFrame();
	auto c = gpuFrame->channels();

	cuda::GpuMat gpuFrameGray(gpuFrame->size(), gpuFrame->type());
	cuda::cvtColor(*gpuFrame, gpuFrameGray, COLOR_BGRA2GRAY);

	cuda::GpuMat gpuPoints(gpuFrame->size(), gpuFrame->type());
	Ptr<cuda::CornersDetector> detector = cuda::createGoodFeaturesToTrackDetector(
		gpuFrameGray.type(),
		30,
		0.01,
		30
	);

	Mat mask = Mat::zeros(gpuFrameGray.rows, gpuFrameGray.cols, gpuFrameGray.type());
	rectangle(mask, r, Scalar(255, 255, 255), -1);
	cuda::GpuMat gpuMask(mask);

	detector->detect(gpuFrameGray, gpuPoints, gpuMask);
	if (!gpuPoints.empty())
	{
		vector<Point2f> addPoints;
		gpuPoints.download(addPoints);

		for (auto& a : addPoints)
			target->intialPoints.push_back(a);

		target->UpdateType();

		window->DrawWindow();
	}

}

bool StateEditTracker::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;

	if (c == 'r')
	{
		window->stack.PushState(new StateSelectRoi(window, "Select the feature (cancel to remove)",
			[me](Rect& rect)
			{
				me->target->initialRect = rect;
				me->target->UpdateType();

			},
			// Failed
				[me]()
			{
				me->target->initialRect = Rect2f(0, 0, 0, 0);
				me->target->UpdateType();
			}
			));

		return true;
	}
	else if (c == 'f')
	{
		window->stack.PushState(new StateTestTrackers(window, me->set, me->target));

		return true;
	}

	return false;
}

bool StateEditTracker::HandleMouse(int e, int x, int y, int f)
{
	if (e == EVENT_LBUTTONUP)
	{
		dragging = false;

		if (draggingRect)
		{
			Rect r(clickStartPosition, draggingPosition);
			AddPoints(r);
			if (target->initialRect.empty())
			{
				target->initialRect = r;
				target->UpdateType();
			}
		}
		else
		{
			target->intialPoints.emplace_back(Point2f(x, y));
		}

		draggingRect = false;
		draggingBtn = BUTTON_NONE;

		window->DrawWindow();
		return true;
	}

	if (e == EVENT_RBUTTONUP)
	{
		dragging = false;

		if (draggingRect)
		{
			RemovePoints(Rect(clickStartPosition, draggingPosition));
		}

		draggingRect = false;
		draggingBtn = BUTTON_NONE;

		window->DrawWindow();
		return true;
	}

	if (e == EVENT_LBUTTONDOWN)
	{
		clickStartPosition = Point2f(x, y);
		dragging = true;
		draggingBtn = BUTTON_LEFT;
		return true;
	}

	if (e == EVENT_RBUTTONDOWN)
	{
		clickStartPosition = Point2f(x, y);
		dragging = true;
		draggingBtn = BUTTON_RIGHT;
		return true;
	}

	if (e == EVENT_MOUSEMOVE)
	{
		if (dragging && !draggingRect)
		{
			double dist = norm(clickStartPosition - Point(x, y));
			if (dist > 5)
				draggingRect = true;
		}

		if (draggingRect)
		{
			draggingPosition = Point(x, y);
			window->DrawWindow();
		}
	}

	return false;
}

void StateEditTracker::AddGui(Mat& frame)
{
	putText(frame, "Drag left mouse to add points", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Drag right mouse to remove points", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Click add a point", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (draggingRect)
	{
		Scalar color(180, 0, 0);
		if (draggingBtn == DRAGGING_BUTTON::BUTTON_RIGHT)
			color = Scalar(0, 0, 255);

		rectangle(frame, Rect(clickStartPosition, draggingPosition), color, 2);
	}

	target->Draw(frame);
}