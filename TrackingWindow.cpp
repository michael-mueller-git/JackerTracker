#include "TrackingWindow.h"
#include "States.h"
#include <opencv2/cudawarping.hpp>
#include <opencv2/plot.hpp>

// Timebar

Timebar::Timebar(TrackingWindow& window)
	:window(window)
{

}

map<TrackingSet*, GuiButton> Timebar::GetButtons()
{
	map<TrackingSet*, GuiButton> ret;

	Rect barRect(
		20,
		20,
		window.GetOutFrame()->cols - 40,
		20
	);

	
	for (auto& s : window.project.sets)
	{
		int x = mapValue<track_time, int>(
			s.timeStart,
			0,
			window.GetDuration(),
			barRect.x,
			barRect.x + barRect.width
		);

		Rect setRect(
			x - 5,
			barRect.y - 2,
			10,
			barRect.height + 4
		);

		GuiButton b;
		b.text = to_string(s.targets.size());
		b.textScale = 0.4;
		b.rect = setRect;
		if (selectedSet == &s)
			b.hover = true;

		ret.insert(pair<TrackingSet*, GuiButton>(&s, b));
	}

	return ret;
}

void Timebar::AddGui(Mat& frame)
{
	int w = frame.cols;

	Rect barRect(
		20,
		20,
		w - 40,
		20
	);

	rectangle(frame, barRect, Scalar(100, 100, 100), -1);
	rectangle(frame, barRect, Scalar(160, 160, 160), 1);

	int x = mapValue<track_time, int>(
		window.GetCurrentPosition(),
		0,
		window.GetDuration(),
		barRect.x,
		barRect.x + barRect.width
	);

	line(
		frame,
		Point(
			x,
			barRect.y - 5
		),
		Point(
			x,
			barRect.y + barRect.height + 5
		),
		Scalar(0, 0, 0), 2
	);

	buttons = GetButtons();
	for (auto& sr : buttons)
	{
		sr.second.Draw(frame);
	}
}

void Timebar::SelectTrackingSet(TrackingSet* s)
{
	selectedSet = s;
	if (s)
	{
		if (s->timeStart != window.GetCurrentPosition())
			window.SetPosition(s->timeStart);
	}
};

bool Timebar::HandleMouse(int e, int x, int y, int f)
{
	if (e != EVENT_LBUTTONUP)
		return false;

	for (auto& sr : buttons)
	{
		if (sr.second.IsSelected(x, y))
		{
			SelectTrackingSet(sr.first);
			window.DrawWindow();
			return true;
		}
	}

	return false;
}

TrackingSet* Timebar::GetNextSet()
{
	if (window.project.sets.size() == 0)
		return nullptr;

	auto s = GetSelectedSet();
	auto& sets = window.project.sets;

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
		auto t = window.GetCurrentPosition();
		auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return set.timeStart > t; });
		if (it == sets.end())
			return nullptr;

		return &(*it);
	}
}

TrackingSet* Timebar::GetPreviousSet()
{
	if (window.project.sets.size() == 0)
		return nullptr;


	auto s = GetSelectedSet();
	auto& sets = window.project.sets;

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

		auto t = window.GetCurrentPosition();

		auto it = find_if(sets.rbegin(), sets.rend(), [t](TrackingSet& set) { return set.timeStart < t; });
		if (it == sets.rend())
			return nullptr;

		return &(*it);
	}
}

// StateStack

void StateStack::PushState(StateBase* s)
{
	stack.push_back(s);
	s->EnterState();
	window.DrawWindow();
}

void StateStack::ReplaceState(StateBase* s)
{
	assert(stack.size() > 0);

	PopState();
	PushState(s);
}

void StateStack::PopState()
{
	assert(stack.size() > 0);

	auto s = stack.back();
	stack.pop_back();
	s->LeaveState();
	delete(s);

	if (stack.size() > 0)
	{
		s = stack.back();
		s->EnterState();
		window.DrawWindow();
	}
}

// TrackingWindow

TrackingWindow::TrackingWindow(string fName, track_time startTime)
	:project(fName), stack(*this), timebar(*this)
{
	namedWindow(windowName, 1);

	videoReader = VideoReader::create(fName);
	videoReader->Seek(startTime);
	ReadCleanFrame();

	setMouseCallback(windowName, &OnClick, this);
	createTrackbar("Position", windowName, 0, 1000, &OnTrackbar, this);
	UpdateTrackbar();

	stack.PushState(new StatePaused(this));

	if (project.sets.size() > 0)
	{
		timebar.SelectTrackingSet(&project.sets.at(0));
		stack.PushState(new StateEditSet(this, timebar.GetSelectedSet()));
	}
}

TrackingWindow::~TrackingWindow()
{

}

void TrackingWindow::SetPlaying(bool p)
{
	isPlaying = p;
}

bool TrackingWindow::IsPlaying()
{
	return isPlaying;
}

void TrackingWindow::DrawWindow()
{
	drawRequested = true;
}

void TrackingWindow::ReallyDrawWindow(cuda::Stream& stream)
{
	auto s = stack.GetState();
	if (!s)
		return;

	if (!inFrame)
		return;

	inFrame->download(outFrame, stream);

	s->AddGui(outFrame);
	for (auto& b : s->buttons)
	{
		b.Draw(outFrame);
	}

	timebar.AddGui(outFrame);
	putText(outFrame, "State: " + s->GetName(), Point(30, 60), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (videoScale != 1)
	{
		Size size(
			outFrame.size().width * videoScale,
			outFrame.size().height * videoScale
		);

		cuda::GpuMat gpuOutFrame(outFrame);
		cuda::GpuMat gpuResizedFrame(size, gpuOutFrame.type());
		cuda::resize(gpuOutFrame, gpuResizedFrame, size);
		gpuResizedFrame.download(outFrame);
	}

	imshow(windowName, outFrame);
}

bool TrackingWindow::ReadCleanFrame(cuda::Stream& stream)
{
	inFrame = videoReader->NextFrame(stream);
	if (!inFrame)
	{
		return false;
	}

	// Play audio

	if (videoScale != 1)
	{
		int w = inFrame->size().width * videoScale;
		int h = inFrame->size().height * videoScale;

		Size s(w, h);

		cuda::resize(*inFrame, *inFrame, s);
	}

	return true;
}

void TrackingWindow::Run()
{
	while (stack.GetState() != nullptr)
		RunOnce();
}

void TrackingWindow::RunOnce()
{
	auto s = stack.GetState();
	if (!s)
		return;

	if (s->ShouldPop())
	{
		stack.PopState();
		return;
	}

	s->Update();
	if (drawRequested)
	{
		ReallyDrawWindow();
		drawRequested = false;
	}

	// Keyboard handling
	char k = waitKey(1);
	if (k == lastKey)
		return;

	if (k == 'q')
		stack.PopState();
	else
		s->HandleInput(k);

	lastKey = k;
}

void TrackingWindow::OnTrackbar(int v, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->stack.GetState();
	if (!s)
		return;

	if (s->GetName() != "Paused")
	{
		me->UpdateTrackbar();
		return;
	}

	track_time t = mapValue<int, track_time>(v, 0, 1000, 0, me->GetDuration());
	me->SetPosition(t);
}

void TrackingWindow::OnClick(int e, int x, int y, int f, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->stack.GetState();
	if (!s)
		return;

	Size outSize = me->GetInFrame()->size();
	Size inSize = me->GetOutFrame()->size();

	x = mapValue<int, int>(x, 0, inSize.width, 0, outSize.width);
	y = mapValue<int, int>(y, 0, inSize.height, 0, outSize.height);

	bool handled = false;

	if (e == EVENT_MOUSEMOVE)
	{
		for (auto& b : s->buttons)
		{
			bool hover = b.IsSelected(x, y);

			if (hover != b.hover)
			{
				b.hover = hover;
				me->drawRequested = true;
			}
		}
	}
	else if (e == EVENT_LBUTTONDOWN)
	{
		for (auto& b : s->buttons)
		{
			if (b.IsSelected(x, y))
			{
				me->pressingButton = true;
				handled = true;
			}
		}
	}
	else if (e == EVENT_LBUTTONUP)
	{
		if (me->pressingButton)
		{
			for (auto& b : s->buttons)
			{
				if (b.IsSelected(x, y))
				{
					b.onClick();
					me->drawRequested = true;
					handled = true;
				}
			}
		}

		me->pressingButton = false;
	}

	if (s->ShouldPop())
	{
		me->stack.PopState();
		handled = true;
	}

	if(!handled)
		s->HandleMouse(e, x, y, f);
}

void TrackingWindow::SetPosition(track_time position)
{
	if (!videoReader->Seek(position))
		cout << "Seek failed\n";

	cuda::Stream stream;

	ReadCleanFrame(stream);
	ReadCleanFrame(stream);

	ReallyDrawWindow(stream);
}

track_time TrackingWindow::GetCurrentPosition()
{
	return videoReader->GetPosition();
}

track_time TrackingWindow::GetDuration()
{
	return videoReader->GetDuration();
}

void TrackingWindow::UpdateTrackbar()
{
	int t = mapValue<track_time, int>(GetCurrentPosition(), 0, GetDuration(), 0, 1000);
	setTrackbarPos("Position", windowName, t);
}

TrackingSet* TrackingWindow::AddSet()
{
	track_time t = GetCurrentPosition();
	//Jump to keyframe
	SetPosition(t);

	auto it = find_if(project.sets.begin(), project.sets.end(), [t](TrackingSet& set) { return t <= set.timeStart; });
	
	//if (it != sets.end() && it->timeStart == t)
	//	return nullptr;
	
	it = project.sets.emplace(it, GetCurrentPosition(), GetDuration());
	return &(*it);
}

void TrackingWindow::DeleteTrackingSet(TrackingSet* s)
{
	auto it = find_if(project.sets.begin(), project.sets.end(), [s](TrackingSet& set) { return &set == s; });
	if (it == project.sets.end())
		return;

	if (timebar.GetSelectedSet() == s)
	{
		timebar.SelectTrackingSet(nullptr);
	}

	project.sets.erase(it);
}