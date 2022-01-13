#include "TrackingWindow.h"
#include "States/StatePlayer.h"
#include "States/Set/StateEditSet.h"

#include <OISException.h>
#include <opencv2/cudawarping.hpp>
#include <opencv2/highgui.hpp>

#include <Windows.h>


using namespace std;
using namespace cv;
using namespace OIS;

OIS::Keyboard* TrackingWindow::inputKeyboard = nullptr;

TrackingWindow::TrackingWindow(string fName)
	:project(fName), stack(*this), timebar(this)
{
	namedWindow(windowName, 1);

	videoReader = VideoReader::create(fName);

	//HWND hWnd = cvGetWindowHandle(windowName.c_str());

	try {
		HWND hWnd = (HWND)FindWindow(NULL, windowName.c_str());

		inputManager = InputManager::createInputSystem((size_t)hWnd);
		inputKeyboard = (Keyboard*)inputManager->createInputObject(OISKeyboard, true);
		inputKeyboard->setEventCallback(this);
		inputKeyboard->capture();
	}
	catch (OIS::Exception e)
	{
		// Input setup error
	}

	setMouseCallback(windowName, &OnClick, this);
	createTrackbar("FPS", windowName, &project.maxFPS, 120);
	createTrackbar("Position", windowName, 0, 1000, &OnTrackbar, this);
	UpdateTrackbar();

	stack.PushState(new StatePlayerImpl(this));

	if (project.sets.size() > 0)
	{
		timebar.SelectTrackingSet(&project.sets.at(0));
		stack.PushState(new StateEditSet(this, timebar.GetSelectedSet()));
	}

	ReadCleanFrame();
}

TrackingWindow::~TrackingWindow()
{
	InputManager::destroyInputSystem(inputManager);
}

void TrackingWindow::SetPlaying(bool p)
{
	isPlaying = p;
}

bool TrackingWindow::IsPlaying()
{
	return isPlaying;
}

void TrackingWindow::DrawWindow(bool b)
{
	drawRequested = true;
	if (b)
		elementUpdateRequested = true;
}

void TrackingWindow::UpdateElements()
{
	guiElements.clear();
	buttons.clear();

	auto s = stack.GetState();
	if (!s)
		return;
	
	s->UpdateButtons(buttons);

	guiElements.emplace_back(*s);
	for (auto& b : buttons)
		guiElements.emplace_back(*b);
}

void TrackingWindow::ReallyDrawWindow(cuda::Stream& stream)
{
	auto s = stack.GetState();
	if (!s)
		return;

	if (inFrame.empty())
		return;

	inFrame.download(outFrame, stream);

	if (elementUpdateRequested)
	{
		UpdateElements();
		elementUpdateRequested = false;
	}

	for (auto& e : guiElements)
		e.get().DoDraw(outFrame);

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
	if (inFrame.empty())
		return false;

	// Play audio
	
	if (videoScale != 1)
	{
		int w = inFrame.size().width * videoScale;
		int h = inFrame.size().height * videoScale;

		Size s(w, h);

		cuda::resize(inFrame, resizeBuffer, s);
		inFrame = resizeBuffer;
	}

	return true;
}

void TrackingWindow::Run()
{
	while (stack.GetState() != nullptr)
		RunOnce();
}

bool TrackingWindow::keyPressed(const KeyEvent& arg)
{
	return false;
}

bool TrackingWindow::keyReleased(const KeyEvent& arg)
{
	for (auto& g : guiElements)
		if (g.get().HandleInput(arg.key))
			break;

	return false;
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

	for (auto& e : guiElements)
		if (e.get().DrawRequested())
			drawRequested = true;


	if (drawRequested)
	{
		ReallyDrawWindow();
		drawRequested = false;
	}

	inputKeyboard->capture();
	// Do cycle processing
	waitKey(1);
}

void TrackingWindow::OnTrackbar(int v, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;

	if (me->trackbarUpdating)
		return;

	auto s = me->stack.GetState();
	if (!s)
		return;

	if (me->isPlaying || s->GetName() == "Tracking")
	{
		me->UpdateTrackbar();
		return;
	}

	time_t t = mapValue<int, time_t>(v, 0, 1000, 0, me->GetDuration());
	me->SetPosition(t, false);
}

void TrackingWindow::OnClick(int e, int x, int y, int f, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->stack.GetState();
	if (!s)
		return;

	Size outSize = me->GetInFrame().size();
	Size inSize = me->GetOutFrame()->size();

	x = mapValue<int, int>(x, 0, inSize.width, 0, outSize.width);
	y = mapValue<int, int>(y, 0, inSize.height, 0, outSize.height);

	for (auto& g : me->guiElements)
		if (g.get().HandleMouse(e, x, y, f))
			return;
}

void TrackingWindow::SetPosition(time_t position, bool updateTrackbar)
{
	videoReader->Seek(position);

	cuda::Stream stream;

	ReadCleanFrame(stream);
	while(videoReader->GetPosition() < position)
		ReadCleanFrame(stream);

	if(updateTrackbar)
		UpdateTrackbar();

	DrawWindow();
}

time_t TrackingWindow::GetCurrentPosition()
{
	return videoReader->GetPosition();
}

time_t TrackingWindow::GetDuration()
{
	return videoReader->GetDuration();
}

void TrackingWindow::UpdateTrackbar()
{
	trackbarUpdating = true;
	int t = mapValue<time_t, int>(GetCurrentPosition(), 0, GetDuration(), 0, 1000);
	setTrackbarPos("Position", windowName, t);
	trackbarUpdating = false;
}

TrackingSet* TrackingWindow::AddSet()
{
	time_t t = GetCurrentPosition();
	//Jump to keyframe
	SetPosition(t);

	auto it = find_if(project.sets.begin(), project.sets.end(), [t](TrackingSet& set) { return t <= set.timeStart; });
	
	if (it != project.sets.end() && it->timeStart == t)
		return nullptr;
	
	it = project.sets.emplace(it, t, t);

	for (auto& s : project.sets)
	{
		if (s.timeStart <= t && s.timeEnd >= t)
		{
			s.ClearEvents([t](auto& e) {
				return e.time < t;
			});
			
			s.timeEnd = t;
		}
	}

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