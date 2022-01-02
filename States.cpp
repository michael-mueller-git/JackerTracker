#include "States.h"
#include "TrackingWindow.h"
#include "StatesUtils.h"
#include "Trackers.h"

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>

// StatePaused

StatePaused::StatePaused(TrackingWindow* window)
	:StateBase(window)
{
	AddButton("Add tracking here", 'a');
}

void StatePaused::HandleInput(char c)
{
	if (c == ' ')
	{
		window->stack.ReplaceState(new StatePlaying(window));
		return;
	}

	if (c == 'a')
	{
		TrackingSet* s = window->AddSet();
		if (s != nullptr)
		{
			window->stack.PushState(new StateEditSet(window, s));
		}
	}
}

void StatePaused::HandleMouse(int e, int x, int y, int f)
{
	window->timebar.HandleMouse(e, x, y, f);
	if (window->timebar.GetSelectedSet() != nullptr)
	{
		window->stack.PushState(new StateEditSet(window, window->timebar.GetSelectedSet()));
	}
}

void StatePaused::AddGui(Mat& frame)
{
	putText(frame, "Press space to play", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StatePlaying::Update()
{
	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();
	window->UpdateTrackbar();
	window->DrawWindow();
}

void StatePlaying::AddGui(Mat& frame)
{
	putText(frame, "Press q to quit", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press space to pause", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StatePlaying::HandleInput(char c)
{
	if (c == ' ')
		window->stack.ReplaceState(new StatePaused(window));
}

void StatePlaying::EnterState()
{
	window->SetPlaying(true);
}

// StateEditSet

StateEditSet::StateEditSet(TrackingWindow* window, TrackingSet* set)
	:StateBase(window), set(set)
{
	AddButton("Delete tracking", '-');
	AddButton("Add target", 'a');
	AddButton("Start tracking", 'l');
}

void StateEditSet::EnterState()
{
	window->timebar.SelectTrackingSet(set);
}

void StateEditSet::LeaveState()
{
	window->timebar.SelectTrackingSet(nullptr);
}

void StateEditSet::AddGui(Mat& frame)
{	
	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 140;
	for (auto& t : set->targets)
	{
		putText(frame, " - " + t.GetName(), Point(30, y), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
		y += 20;
	}

	set->Draw(frame);
}

void StateEditSet::HandleMouse(int e, int x, int y, int f)
{
	window->timebar.HandleMouse(e, x, y, f);
}

void StateEditSet::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;
	auto s = me->set;

	if (c == 'l')
	{
		window->stack.PushState(new StateTracking(window, set));
		return;
	}

	if (c == 'a')
	{
		window->stack.PushState(new StateAddTracker(window, [s, me](TrackingTarget t){

			if (s->targets.size() == 0)
				t.targetType = TARGET_TYPE::TYPE_FEMALE;
			else if (s->targets.size() == 1)
				t.targetType = TARGET_TYPE::TYPE_MALE;
			else if (s->targets.size() == 2)
				t.targetType = TARGET_TYPE::TYPE_BACKGROUND;
			else
				t.targetType = TARGET_TYPE::TYPE_UNKNOWN;

			s->targets.push_back(t);

			},
			// Failed
			[]() {}
		));
	}

	if (c == '-')
	{
		window->stack.PushState(new StateConfirm(window, "Are you sure you want to delete this set?",
			// Yes
			[me, window, s]()
			{
				window->DeleteTrackingSet(s);
				me->Pop();
			},
			// No
				[]() {; }
			));
	}
}

// StateAddTracker

StateAddTracker::StateAddTracker(TrackingWindow* window, AddTrackerCallback callback, StateFailedCallback failedCallback)
	:StateBase(window), callback(callback), failedCallback(failedCallback)
{

}

void StateAddTracker::EnterState()
{
	AddButton("Done", 'd');
	AddButton("Set rect", 'r');
}

void StateAddTracker::RemovePoints(Rect r)
{
	vector<Point2f> newPoints;

	copy_if(
		newTarget.intialPoints.begin(),
		newTarget.intialPoints.end(),
		back_inserter(newPoints),
		[r](Point2f p)
		{
			bool inside =
				p.x > r.x && p.x < r.x + r.width &&
				p.y > r.y && p.y < r.y + r.height;

			return !inside;
		}
	);

	newTarget.intialPoints = newPoints;
	newTarget.UpdateType();

	window->DrawWindow();
}

void StateAddTracker::AddPoints(Rect r)
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
			newTarget.intialPoints.push_back(a);

		newTarget.UpdateType();

		window->DrawWindow();
	}
	
}

void StateAddTracker::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;
	TrackingTarget& t = me->newTarget;
	TrackingTarget* tp = &me->newTarget;

	if (c == 'd')
	{
		t.UpdateType();

		if (t.trackingType != TRACKING_TYPE::TYPE_NONE)
		{
			callback(t);
			returned = true;
		}
		Pop();
		return;
	}

	if (c == 'r')
	{
		window->stack.PushState(new StateSelectRoi(window, "Select the feature (cancel to remove)",
			[tp](Rect2f& rect)
			{
				tp->initialRect = rect;
				tp->UpdateType();

			},
			// Failed
			[tp]() 
			{
				tp->initialRect = Rect2f(0, 0, 0, 0);
				tp->UpdateType();
			}
		));
	}
}

void StateAddTracker::HandleMouse(int e, int x, int y, int f)
{
	if (e == EVENT_LBUTTONUP)
	{
		dragging = false;
		
		if (draggingRect)
		{
			AddPoints(Rect(clickStartPosition, draggingPosition));
		}
		else
		{
			newTarget.intialPoints.emplace_back(Point2f(x, y));
		}

		draggingRect = false;
		draggingBtn = BUTTON_NONE;

		window->DrawWindow();
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
	}

	if (e == EVENT_LBUTTONDOWN)
	{
		clickStartPosition = Point2f(x, y);
		dragging = true;
		draggingBtn = BUTTON_LEFT;
	}

	if (e == EVENT_RBUTTONDOWN)
	{
		clickStartPosition = Point2f(x, y);
		dragging = true;
		draggingBtn = BUTTON_RIGHT;
	}

	if(e == EVENT_MOUSEMOVE)
	{
		if (dragging && !draggingRect)
		{
			double dist = norm(clickStartPosition - Point2f(x, y));
			if (dist > 5)
				draggingRect = true;
		}

		if (draggingRect)
		{
			draggingPosition = Point2f(x, y);
			window->DrawWindow();
		}
	}
}

void StateAddTracker::AddGui(Mat& frame)
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

	newTarget.Draw(frame);
}

// StateTracking

void StateTracking::EnterState()
{
	window->timebar.SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2GRAY);
	trackerBindings.reserve(set->targets.size() * 5);

	for (auto& t : set->targets)
	{
		TrackerJT* tracker;
		TrackingTarget::TrackingState* state;

		if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
		{
			// GpuTrackerPoints
			state = t.InitTracking();
			tracker = new GpuTrackerPoints(t, *state);

			trackerBindings.emplace_back(&t, state, tracker);
			trackerBindings.back().tracker->init(grayScale);
		}

		if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
		{
			// TrackerCSRT
			state = t.InitTracking();
			tracker = new TrackerOpenCV(t, *state, TrackerCSRT::create(), "TrackerCSRT");

			trackerBindings.emplace_back(&t, state, tracker);
			trackerBindings.back().tracker->init(grayScale);
			

			// TrackerMIL
			state = t.InitTracking();
			tracker = new TrackerOpenCV(t, *state, TrackerMIL::create(), "TrackerMIL");

			trackerBindings.emplace_back(&t, state, tracker);
			trackerBindings.back().tracker->init(grayScale);
			

			// TrackerMedianFlow
			state = t.InitTracking();
			tracker = new TrackerOpenCV(t, *state, legacy::upgradeTrackingAPI(legacy::TrackerMedianFlow::create()), "TrackerMedianFlow");

			trackerBindings.emplace_back(&t, state, tracker);
			trackerBindings.back().tracker->init(grayScale);
		}
		
	}

	window->SetPlaying(true);
}

void StateTracking::LeaveState()
{
	window->timebar.SelectTrackingSet(set);
}

void StateTracking::HandleInput(char c)
{
	if (c == ' ')
	{
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}
}

void StateTracking::AddGui(Mat& frame)
{
	if (playing)
		putText(frame, "Press space to pause", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 140;

	for (auto& b : trackerBindings)
	{
		b.state->Draw(frame);
		putText(frame, format("%s, %dms", b.tracker->GetName(), b.lastUpdateMs), Point(30, y), FONT_HERSHEY_SIMPLEX, 0.8, b.state->color, 2);
		y += 20;
	}
}

void StateTracking::Update()
{
	if (!playing)
		return;

	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	// Read and color the new frame
	window->ReadCleanFrame();

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2GRAY);

	// Run all trackers on the new frame
	for (auto& b : trackerBindings)
	{
		now = high_resolution_clock::now();
		b.tracker->update(grayScale);
		b.lastUpdateMs = duration_cast<chrono::milliseconds>(now - b.lastUpdate).count();
		b.lastUpdate = now;
	}

	window->DrawWindow();
}

// StateTrackResult

void StateTrackResult::AddGui(Mat& frame)
{
	putText(frame, "Tracking result: " + result, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press q to return to edit", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}