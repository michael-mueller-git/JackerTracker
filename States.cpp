#include "States.h"
#include "TrackingWindow.h"
#include "StatesUtils.h"
#include "Trackers.h"

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>

// StatePlayer

void StatePlayer::UpdateButtons(vector<GuiButton>& out)
{
	StateBase::UpdateButtons(out);
	AddButton(out, "Add tracking", '+');
}

void StatePlayer::UpdateFPS()
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
}

void StatePlayer::Update()
{
	if (!playing)
		return;

	UpdateFPS();
	SyncFps();

	window->ReadCleanFrame();
	window->UpdateTrackbar();
	window->DrawWindow();
}

void StatePlayer::AddGui(Mat& frame)
{
	if (playing)
	{
		putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
		putText(frame, "Press space to pause", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	}
	else
		putText(frame, "Press space to play", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	auto pos = window->GetCurrentPosition();
	for (auto& set : window->project.sets)
	{
		if (set.timeStart <= pos && set.timeEnd > pos)
		{
			if (&set != lastSet)
			{
				calculator.Reset();
				lastSet = &set;
			}

			calculator.Draw(set, frame, window->GetCurrentPosition(), false, true);
			break;
		}
	}
}

bool StatePlayer::HandleInput(char c)
{
	if (c == ' ')
	{
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow(true);

		if (playing)
		{
			lastUpdate = high_resolution_clock::now();
		}

		return true;
	}

	if (c == '+')
	{
		auto s = window->AddSet();
		window->stack.PushState(new StateEditSet(window, s));
		return true;
	}

	return false;
}

void StatePlayer::EnterState()
{
	window->SetPlaying(playing);
}

void StatePlayer::SyncFps()
{
	if (window->maxFPS == 0)
		return;

	auto n = high_resolution_clock::now();

	int deltaMs = duration_cast<chrono::milliseconds>(n - lastUpdate).count();
	int waitMs = 1000 / window->maxFPS;
	if (deltaMs < waitMs)
		this_thread::sleep_for(chrono::milliseconds(waitMs - deltaMs));

	lastUpdate = n;
}

// StateEditSet

StateEditSet::StateEditSet(TrackingWindow* window, TrackingSet* set)
	:StatePlayer(window), set(set)
{

}

void StateEditSet::EnterState()
{
	StatePlayer::EnterState();

	window->timebar.SelectTrackingSet(set);
}

void StateEditSet::LeaveState()
{
	window->timebar.SelectTrackingSet(nullptr);
}

void StateEditSet::AddGui(Mat& frame)
{
	StatePlayer::AddGui(frame);

	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	auto pos = window->GetCurrentPosition();
	bool drawState = false;
	if (pos == set->timeStart)
		set->Draw(frame);
	else
		drawState = true;

	calculator.Draw(*set, frame, window->GetCurrentPosition(), false, drawState);
}

void StateEditSet::UpdateButtons(vector<GuiButton>& out)
{
	StateBase::UpdateButtons(out);
	auto me = this;

	AddButton(out, "Add tracking here", 'a');
	AddButton(out, "Delete tracking", '-');
	AddButton(out, "Start tracking", 'l');

	int y = 220;

	Rect r;
	GuiButton* b;

	for (int i = 0; i < set->targets.size(); i++)
	{
		TrackingTarget* t = &set->targets.at(i);
		TrackingSet* s = set;

		r = Rect(
			380,
			y,
			230,
			40
		);

		b = &AddButton(
			out,
			t->GetName(),
			[me, s, t]() {
				me->window->stack.PushState(new StateEditTracker(me->window, s, t));
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
			out,
			"X",
			[me, i]() {
				auto& tv = me->set->targets;
				tv.erase(tv.begin() + i);
				
				for (int c = 0; c < tv.size(); c++)
					tv.at(c).UpdateColor(c);

				me->window->DrawWindow(true);
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
		out,
		"Targets (+)",
		[me]() {
			me->HandleInput('+');
		},
		r
	);

	me->window->DrawWindow();
}

bool StateEditSet::HandleInput(char c)
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
		return true;
	}

	if (c == 'l')
	{
		window->stack.PushState(new StateTracking(window, set));
		return true;
	}

	if (c == '+')
	{
		s->targets.emplace_back();
		TrackingTarget* t = &s->targets.back();
		t->UpdateColor(s->targets.size());

		if (s->targets.size() == 1)
			t->targetType = TARGET_TYPE::TYPE_FEMALE;
		else if (s->targets.size() == 2)
			t->targetType = TARGET_TYPE::TYPE_MALE;
		else if (s->targets.size() == 3)
			t->targetType = TARGET_TYPE::TYPE_BACKGROUND;
		else
			t->targetType = TARGET_TYPE::TYPE_UNKNOWN;

		window->stack.PushState(new StateEditTracker(window, set, t));

		return true;
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

		return true;
	}

	if (StatePlayer::HandleInput(c))
		return true;

	return false;
}

// StateAddTracker

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

// StateTracking

StateTracking::StateTracking(TrackingWindow* window, TrackingSet* set)
	:StatePlayer(window), set(set)
{
	
}

void StateTracking::EnterState()
{
	StatePlayer::EnterState();

	window->timebar.SelectTrackingSet(set);
	set->events.clear();
	calculator.Reset();

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	FRAME_CACHE->Update(gpuFrame, FrameVariant::GPU_RGBA);

	if (trackerBindings.size() > 0)
		trackerBindings.clear();

	trackerBindings.reserve(set->targets.size());

	for (auto& t : set->targets)
	{

		TrackerJTType type = t.preferredTracker;

		if (type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
			if (t.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
				type = TrackerJTType::GPU_POINTS_PYLSPRASE;
			else if(t.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
				type = TrackerJTType::CPU_RECT_MEDIAN_FLOW;

		TrackerJTStruct jts = GetTracker(type);
		if (!t.SupportsTrackingType(jts.trackingType))
			continue;

		trackerBindings.emplace_back(&t, jts);
		trackerBindings.back().tracker->init();
		trackerBindings.back().state->UpdateColor(trackerBindings.size());
	}

	if (trackerBindings.size() == 0)
	{
		Pop();
		return;
	}

	FRAME_CACHE->Clear();
	playing = true;
	window->SetPlaying(playing);
}

void StateTracking::LeaveState()
{
	window->timebar.SelectTrackingSet(set);
	FRAME_CACHE->Clear();
}

void StateTracking::UpdateButtons(vector<GuiButton>& out)
{
	StatePlayer::UpdateButtons(out);

	if (!playing)
	{
		AddButton(out, "Remove points", 'r');
	}
}

bool StateTracking::HandleInput(char c)
{
	if (StatePlayer::HandleInput(c))
		return true;

	auto me = this;

	if (!playing && c == 'r')
	{
		window->stack.PushState(new StateSelectRoi(window, "Select which points to remove",
			[me](Rect& r) {
				if (!me->RemovePoints(r))
					return;


			},
			// Failed
			[]() {; },
			[me](Mat& frame) {
				for (auto& b : me->trackerBindings)
				{
					b.state->Draw(frame);
				}
			}
		));

		return true;
	}

	return false;
}

bool StateTracking::RemovePoints(Rect r)
{
	bool ret = false;

	for (auto& tb : trackerBindings)
	{
		if (tb.trackerStruct.type != TRACKING_TYPE::TYPE_POINTS)
			continue;

		for (int i = tb.state->points.size(); i>0 ; i--)
		{
			auto& p = tb.state->points.at(i - 1);

			bool inside =
				p.point.x > r.x && p.point.x < r.x + r.width &&
				p.point.y > r.y && p.point.y < r.y + r.height;
			if (!inside)
				continue;

			tb.target->intialPoints.erase(tb.target->intialPoints.begin() + i - 1);
			ret = true;
		}
	}

	return ret;
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

	calculator.Draw(*set, frame, window->GetCurrentPosition());
}

void StateTracking::Update()
{
	if (!playing)
		return;

	UpdateFPS();

	cuda::Stream stream;

	window->ReadCleanFrame(stream);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	FRAME_CACHE->Update(gpuFrame, FrameVariant::GPU_RGBA);

	// Run all trackers on the new frame
	for (auto& b : trackerBindings)
	{
		auto now = high_resolution_clock::now();
		b.tracker->update(stream);
		b.lastUpdateMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
	}

	stream.waitForCompletion();

	time_t time = window->GetCurrentPosition();
	for (auto& b : trackerBindings)
	{
		b.state->SnapResult(*set, time);
	}

	set->timeEnd = time;
	calculator.Update(*set, time);

	SyncFps();
	window->DrawWindow();
}

// StateTestTrackers

StateTestTrackers::StateTestTrackers(TrackingWindow* window, TrackingSet* set, TrackingTarget* target)
	:StateTracking(window, set), target(target)
{
	
}

void StateTestTrackers::UpdateButtons(vector<GuiButton>& out)
{
	StateTracking::UpdateButtons(out);

	if (!playing)
	{
		auto me = this;

		for (auto& b : trackerBindings)
		{	
			trackerBinding* bPtr = &b;

			GuiButton& btn = AddButton(out, "Pick " + b.trackerStruct.name, [me, bPtr]() {
				me->target->preferredTracker = bPtr->trackerStruct.type;
				me->Pop();
			});
			btn.textColor = b.state->color;
		}

	}
}

void StateTestTrackers::EnterState()
{
	window->timebar.SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	FRAME_CACHE->Update(gpuFrame, FrameVariant::GPU_RGBA);

	constexpr auto types = magic_enum::enum_entries<TrackerJTType>();
	auto me = this;

	if (trackerBindings.size() > 0)
	{
		trackerBindings.clear();
	}

	trackerBindings.reserve(types.size());

	for (auto& t : types)
	{
		auto s = GetTracker(t.first);
		if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
			continue;
		
		if (target->SupportsTrackingType(s.trackingType))
		{
			trackerBindings.emplace_back(target, s);
			trackerBindings.back().tracker->init();
			trackerBindings.back().state->UpdateColor(trackerBindings.size());
		}
	}

	if (trackerBindings.size() == 0)
	{
		Pop();
		return;
	}

	FRAME_CACHE->Clear();
	window->SetPlaying(true);
}