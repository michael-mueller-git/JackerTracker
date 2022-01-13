#include "StateEditTarget.h"
#include "States/StateSelectRoi.h"
#include "States/Tracking/StateTracking.h"
#include "States/Tracking/StateTestTrackers.h"

#include "Gui/GuiButtonExpand.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/Trackers.h"

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <magic_enum.hpp>

using namespace std;
using namespace cv;
using namespace OIS;

StateEditTarget::StateEditTarget(TrackingWindow* window, TrackingSet* set, TrackingTarget* target)
	:StateBase(window), set(set), target(target)
{

}

void StateEditTarget::EnterState(bool again)
{
	window->SetPosition(set->timeStart);
}

void StateEditTarget::UpdateButtons(ButtonListOut out)
{
	auto me = this;

	AddButton(out, "Done", [me](auto w) {
		me->Pop();
	}, KC_Q);

	AddButton(out, "Set rect", [me](auto w) {
		w->PushState(new StateSelectRoi(w, "Select the feature (cancel to remove)",
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
	}, KC_R);

	AddButton(out, "Set range", [me](auto w) {
		w->PushState(new StateSelectRoi(me->window, "Select the tracking range (cancel to remove)",
			[me](Rect& rect)
			{
				me->target->range = rect;

			},
			// Failed
				[me]()
			{
				me->target->range = Rect(0, 0, 0, 0);
			}
			));
	}, KC_R);


	GuiButtonExpand* trackerBtn = new GuiButtonExpand(GuiButton::Next(out), "Tracker: " + GetTracker(target->preferredTracker).name);
	for (auto& t : magic_enum::enum_entries<TrackerJTType>())
	{
		auto s = GetTracker(t.first);
		if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
			continue;

		auto& b = trackerBtn->AddButton(new GuiButton(trackerBtn->Next(), [me, t]() {
			me->target->preferredTracker = t.first;
			me->trackerOpen = false;
			me->window->DrawWindow(true);
		}, s.name));
	}

	out.emplace_back(trackerBtn);


	GuiButtonExpand* typeBtn = new GuiButtonExpand(GuiButton::Next(out), "Type: " + TargetTypeToString(target->targetType));
	for (auto& t : magic_enum::enum_entries<TARGET_TYPE>())
	{
		if (t.first == TARGET_TYPE::TYPE_UNKNOWN)
			continue;

		auto& b = typeBtn->AddButton(new GuiButton(typeBtn->Next(), [me, t]() {
			me->target->targetType = t.first;
			me->typeOpen = false;
			me->window->DrawWindow(true);
		}, TargetTypeToString(t.first)));
	}

	out.emplace_back(typeBtn);

	AddButton(out, "Test tracking", [me](auto w) {
		w->PushState(new StateTestTrackers(w, me->set, me->target));
	}, KC_F);
}

void StateEditTarget::RemovePoints(Rect r)
{
	vector<Point> newPoints;

	copy_if(
		target->initialPoints.begin(),
		target->initialPoints.end(),
		back_inserter(newPoints),
		[r](Point p)
		{
			bool inside =
				p.x > r.x && p.x < r.x + r.width &&
				p.y > r.y && p.y < r.y + r.height;

			return !inside;
		}
	);

	target->initialPoints = newPoints;
	target->UpdateType();

	AskDraw();
}

void StateEditTarget::AddPoints(Rect r)
{
	cuda::GpuMat gpuFrame = window->GetInFrame();
	auto c = gpuFrame.channels();

	cuda::GpuMat gpuFrameGray(gpuFrame.size(), gpuFrame.type());
	cuda::cvtColor(gpuFrame, gpuFrameGray, COLOR_BGRA2GRAY);

	cuda::GpuMat gpuPoints(gpuFrame.size(), gpuFrame.type());
	Ptr<cuda::CornersDetector> detector = cuda::createGoodFeaturesToTrackDetector(
		gpuFrameGray.type(),
		30,
		0.005,
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
			target->initialPoints.push_back(a);

		target->UpdateType();

		AskDraw();
	}

}

bool StateEditTarget::HandleMouse(int e, int x, int y, int f)
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
			target->initialPoints.emplace_back(Point2f(x, y));
		}

		draggingRect = false;
		draggingBtn = BUTTON_NONE;

		AskDraw();
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

		AskDraw();
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
			AskDraw();
		}
	}

	return false;
}

void StateEditTarget::Draw(Mat& frame)
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