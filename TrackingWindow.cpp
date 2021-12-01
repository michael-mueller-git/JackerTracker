#include "TrackingWindow.h"
#include "States.h"
#include <opencv2/cudawarping.hpp>

TrackingWindow::TrackingWindow(string fName, track_time startTime)
	:cap(fName)
{
	namedWindow(windowName, 1);
	SetPosition(startTime);

	setMouseCallback(windowName, &OnClick, this);
	createTrackbar("Position", windowName, 0, 1000, &OnTrackbar, this);
	UpdateTrackbar();

	cap.read(inFrame);
	if (videoScale != 1)
	{
		cuda::GpuMat f(inFrame);

		int w = inFrame.size().width * videoScale;
		int h = inFrame.size().height * videoScale;

		Size s(w, h);

		cuda::GpuMat resized;
		cuda::resize(f, resized, s);

		
		resized.download(inFrame);
	}

	PushState(new StatePaused(this));
}

bool TrackingWindow::ReadFrame(bool next)
{
	auto s = GetState();
	if (!s)
		return false;

	if (next && !cap.read(inFrame) || inFrame.empty())
		return false;

	if (next && videoScale != 1)
	{
		cuda::GpuMat f(inFrame);

		int w = inFrame.size().width * videoScale;
		int h = inFrame.size().height * videoScale;

		Size s(w, h);

		cuda::GpuMat resized;
		cuda::resize(f, resized, s);


		resized.download(inFrame);
	}

	outFrame = inFrame.clone();

	DrawWindow();

	return true;
}

void TrackingWindow::DrawWindow()
{
	auto s = GetState();
	if (!s)
		return;

	s->AddGui(outFrame);
	DrawTimebar(outFrame);
	putText(outFrame, "State: " + s->GetName(), Point(30, 60), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	imshow(windowName, outFrame);
}

bool TrackingWindow::ReadCleanFrame()
{
	if (!cap.read(outFrame) || outFrame.empty())
		return false;

	if (videoScale != 1)
	{
		cuda::GpuMat f(outFrame);

		int w = outFrame.size().width * videoScale;
		int h = outFrame.size().height * videoScale;

		Size s(w, h);

		cuda::GpuMat resized;
		cuda::resize(f, resized, s);


		resized.download(outFrame);
	}

	return true;
}

void TrackingWindow::Run()
{
	while (GetState() != nullptr)
		RunOnce();
}

void TrackingWindow::RunOnce()
{
	auto s = GetState();
	if (!s)
		return;

	if (s->ShouldPop())
	{
		PopState();
		return;
	}

	s->Update();

	// Keyboard handling
	char k = waitKey(1);
	if (k == lastKey)
		return;

	if (k == 'q')
		PopState();
	else
		s->HandleInput(k);

	lastKey = k;
}

void TrackingWindow::DrawTimebar(Mat& frame)
{
	track_time d = GetDuration();
	track_time c = GetCurrentPosition();
	int w = cap.get(CAP_PROP_FRAME_WIDTH);

	rectangle(
		frame,
		Rect(
			20,
			20,
			w - 40,
			20
		),
		Scalar(255, 0, 0),
		2
	);

	int x;

	for (auto& s : sets)
	{
		x = mapValue(s.timeStart, 0, d, 20, w - 40);
		if (selectedSet == &s) 
			line(frame, Point(x, 20), Point(x, 40), Scalar(0, 255, 255), 10);
		else
			line(frame, Point(x, 20), Point(x, 40), Scalar(0, 255, 0), 6);
	}

	x = mapValue(c, 0, d, 20, w - 40);
	line(frame, Point(x, 20), Point(x, 40), Scalar(255, 0, 0), 3);
}

void TrackingWindow::PushState(StateBase* s)
{
	stateStack.push_back(s);
	s->EnterState();
	ReadFrame(false);
}

void TrackingWindow::ReplaceState(StateBase* s)
{
	assert(stateStack.size() > 0);

	PopState();
	PushState(s);
}

void TrackingWindow::PopState()
{
	assert(stateStack.size() > 0);

	auto s = stateStack.back();
	stateStack.pop_back();
	s->LeaveState();
	delete(s);
	
	if (stateStack.size() > 0)
		ReadFrame(false);
}

void TrackingWindow::OnTrackbar(int v, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->GetState();
	if (!s)
		return;

	if (s->GetName() != "Paused")
	{
		me->UpdateTrackbar();
		return;
	}

	track_time t = mapValue(v, 0, 1000, 0, me->GetDuration());
	me->SetPosition(t);
}

void TrackingWindow::OnClick(int e, int x, int y, int f, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->GetState();
	if (!s)
		return;

	s->HandleMouse(e, x, y, f);

	if (s->ShouldPop())
		me->PopState();
}

void TrackingWindow::SetPosition(track_time position)
{
	cap.set(CAP_PROP_POS_MSEC, position);
	cap.read(outFrame);
	ReadFrame(true);
}

track_time TrackingWindow::GetCurrentPosition()
{
	return cap.get(CAP_PROP_POS_MSEC);
}

track_time TrackingWindow::GetDuration()
{
	int fps = cap.get(CAP_PROP_FPS);
	int frame_count = int(cap.get(CAP_PROP_FRAME_COUNT));
	return (frame_count / fps) * 1000;
}

void TrackingWindow::UpdateTrackbar()
{
	int t = mapValue(GetCurrentPosition(), 0, GetDuration(), 0, 1000);
	setTrackbarPos("Position", windowName, t);
}

TrackingSet* TrackingWindow::AddSet()
{
	track_time t = GetCurrentPosition();
	auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return t <= set.timeStart; });
	
	//if (it != sets.end() && it->timeStart == t)
	//	return nullptr;
	
	it = sets.emplace(it, GetCurrentPosition(), GetDuration());
	return &(*it);
}

void TrackingWindow::SelectTrackingSet(TrackingSet* s)
{
	selectedSet = s;
	if(s)
		SetPosition(s->timeStart);
};

TrackingSet* TrackingWindow::GetNextSet()
{
	if (sets.size() == 0)
		return nullptr;

	auto s = GetSelectedSet();
	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index < sets.size() - 1)
			return &(sets.at(index + 1));
		else
			return nullptr;
	}
	else
	{
		auto t = GetCurrentPosition();
		auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return set.timeStart > t; });
		if (it == sets.end())
			return nullptr;

		return &(*it);
	}
}

TrackingSet* TrackingWindow::GetPreviousSet()
{
	if (sets.size() == 0)
		return nullptr;

	
	auto s = GetSelectedSet();
	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index > 0)
			return &(sets.at(index - 1));
		else
			return nullptr;
	}
	else
	{
	
		auto t = GetCurrentPosition();

		auto it = find_if(sets.rbegin(), sets.rend(), [t](TrackingSet& set) { return set.timeStart < t; });
		if (it == sets.rend())
			return nullptr;

		return &(*it);
	}
}

