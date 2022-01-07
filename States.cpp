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

}

void StateEditSet::EnterState()
{
	window->timebar.SelectTrackingSet(set);
	UpdateButtons();
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

void StateEditSet::UpdateButtons()
{
	StateBase::UpdateButtons();
	auto me = this;

	AddButton("Add tracking here", 'a');
	AddButton("Delete tracking", '-');
	AddButton("Start tracking", 'l');
	AddButton("Test tracking", 'p');

	int y = 220;

	Rect r;
	GuiButton* b;

	for (int i = 0; i < set->targets.size(); i++)
	{
		TrackingTarget* t = &set->targets.at(i);

		r = Rect(
			380,
			y,
			230,
			40
		);

		b = &AddButton(
			t->GetName(),
			[me, t]() {
				me->window->stack.PushState(new StateEditTracker(me->window, t));
			},
			r
		);

		b->textColor = t->color;

		r = Rect(
			r.x + r.width + 20,
			y,
			40,
			40
		);

		b = &AddButton(
			"X",
			[me, i]() {
				auto& tv = me->set->targets;
				tv.erase(tv.begin() + i);
				me->UpdateButtons();
			},
			r
		);

		y += (40 + 20);
	}

	r = Rect(
		380,
		y,
		230,
		40
	);

	b = &AddButton(
		"Targets (+)",
		[me]() {
			me->HandleInput('+');
		},
		r
	);

	me->window->DrawWindow();
}

void StateEditSet::HandleMouse(int e, int x, int y, int f)
{
	if (e != EVENT_LBUTTONUP)
		return;

	window->timebar.HandleMouse(e, x, y, f);
	if (window->timebar.GetSelectedSet() != set)
		window->stack.ReplaceState(new StateEditSet(window, window->timebar.GetSelectedSet()));

}

void StateEditSet::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;
	auto s = me->set;

	if (c == 'a')
	{
		TrackingSet* s = window->AddSet();
		if (s != nullptr)
		{
			window->stack.ReplaceState(new StateEditSet(window, s));
		}
	}

	if (c == 'l')
	{
		window->stack.PushState(new StateTracking(window, set, false));
		return;
	}

	if (c == 'p')
	{
		window->stack.PushState(new StateTracking(window, set, true));
		return;
	}

	if (c == '+')
	{
		s->targets.emplace_back();
		TrackingTarget* t = &s->targets.back();

		if (s->targets.size() == 0)
			t->targetType = TARGET_TYPE::TYPE_FEMALE;
		else if (s->targets.size() == 1)
			t->targetType = TARGET_TYPE::TYPE_MALE;
		else if (s->targets.size() == 2)
			t->targetType = TARGET_TYPE::TYPE_BACKGROUND;
		else
			t->targetType = TARGET_TYPE::TYPE_UNKNOWN;

		window->stack.PushState(new StateEditTracker(window, t));
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

StateEditTracker::StateEditTracker(TrackingWindow* window, TrackingTarget* target)
	:StateBase(window), target(target)
{

}

void StateEditTracker::UpdateButtons()
{
	StateBase::UpdateButtons();
	auto me = this;

	AddButton("Set rect", 'r');
}

void StateEditTracker::EnterState()
{
	UpdateButtons();
}

void StateEditTracker::RemovePoints(Rect r)
{
	vector<Point2f> newPoints;

	copy_if(
		target->intialPoints.begin(),
		target->intialPoints.end(),
		back_inserter(newPoints),
		[r](Point2f p)
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

void StateEditTracker::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;

	if (c == 'r')
	{
		window->stack.PushState(new StateSelectRoi(window, "Select the feature (cancel to remove)",
			[me](Rect2f& rect)
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
	}
}

void StateEditTracker::HandleMouse(int e, int x, int y, int f)
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

// StateTracking

StateTracking::StateTracking(TrackingWindow* window, TrackingSet* set, bool test)
	:StatePlaying(window), set(set), test(test)
{
	AddButton("Stop here", 'd');
}

void StateTracking::EnterState()
{
	window->timebar.SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	FRAME_CACHE->Update(gpuFrame, FrameVariant::GPU_RGBA);

	trackerBindings.reserve(set->targets.size() * 5);

	for (auto& t : set->targets)
	{
		t.results.clear();
		TrackerJT* tracker;
		TrackingTarget::TrackingState* state;

		if (!test)
		{
			if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
			{
				// GpuTrackerPoints
				state = t.InitTracking();
				tracker = new GpuTrackerPoints(t, *state);

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();
			}
			else if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
			{
				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, legacy::upgradeTrackingAPI(legacy::TrackerMedianFlow::create()), "TrackerMedianFlow");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();
			}
		}
		else
		{

			if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
			{
				// GpuTrackerPoints
				state = t.InitTracking();
				tracker = new GpuTrackerPoints(t, *state);

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();
			}

			if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
			{
				// TrackerCSRT
				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, TrackerCSRT::create(), "TrackerCSRT");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();


				// TrackerMIL
				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, TrackerMIL::create(), "TrackerMIL");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();

				// TrackerMedianFlow
				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, legacy::upgradeTrackingAPI(legacy::TrackerMedianFlow::create()), "TrackerMedianFlow");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();

				// GpuTrackerGOTURN
				/*
				state = t.InitTracking();
				tracker = new GpuTrackerGOTURN(t, *state);

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();
				*/

				// TrackerDaSiamRPN
				TrackerDaSiamRPN::Params p;
				p.backend = dnn::DNN_BACKEND_CUDA;
				p.target = dnn::DNN_TARGET_CUDA;

				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, TrackerDaSiamRPN::create(p), "TrackerDaSiamRPN");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();


				// TrackerKCF
				state = t.InitTracking();
				tracker = new TrackerOpenCV(t, *state, TrackerKCF::create(), "TrackerKCF");

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();

				/*
				TrackerKCF::Params p;

				state = t.InitTracking();
				//tracker = new TrackerOpenCV(t, *state, new mycv::GpuTrackerKCFImpl(p), "TackerKCFImplParallel");
				tracker = new mycv::GpuTrackerKCFImpl(p, t, *state);

				trackerBindings.emplace_back(&t, state, tracker);
				trackerBindings.back().tracker->init();
				*/

			}
		}
		
	}

	FRAME_CACHE->Clear();

	window->SetPlaying(true);
}

void StateTracking::LeaveState()
{
	window->timebar.SelectTrackingSet(set);
	FRAME_CACHE->Clear();
}

void StateTracking::HandleInput(char c)
{
	if (c == ' ')
	{
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}

	if (c == 'd')
	{
		window->stack.ReplaceState(new StateTrackResult(window, set, "Done"));
	}
}

void StateTracking::AddGui(Mat& frame)
{
	if (playing)
		putText(frame, "Press space to pause", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 100;

	for (auto& b : trackerBindings)
	{
		b.state->Draw(frame);
		putText(frame, format("%s: %dms", b.tracker->GetName(), b.lastUpdateMs), Point(400, y), FONT_HERSHEY_SIMPLEX, 0.6, b.state->color, 2);
		y += 20;
	}

	set->calculator.Draw(frame, window->GetCurrentPosition());
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

	cuda::Stream stream;

	// Read and color the new frame
	window->ReadCleanFrame(stream);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	FRAME_CACHE->Update(gpuFrame, FrameVariant::GPU_RGBA);
	FRAME_CACHE->GetVariant(FrameVariant::LOCAL_RGB, stream);
	FRAME_CACHE->GetVariant(FrameVariant::GPU_RGB, stream);

	// Run all trackers on the new frame
	for (auto& b : trackerBindings)
	{
		now = high_resolution_clock::now();
		b.tracker->update(stream);
		b.lastUpdateMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
	}
	stream.waitForCompletion();

	time_t time = window->GetCurrentPosition();
	for (auto& b : trackerBindings)
	{
		b.state->SnapResult(time);
	}
	set->calculator.Update(*set, time);

	window->DrawWindow();
}

// StateTrackResult

void StateTrackResult::EnterState()
{
	
}

void StateTrackResult::AddGui(Mat& frame)
{
	putText(frame, "Tracking result: " + result, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press q to return to edit", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}