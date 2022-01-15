#include "StatePlayer.h"
#include "Set/StateEditSet.h"
#include "Gui/TrackingWindow.h"

#include <chrono>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace chrono;
using namespace cv;
using namespace OIS;

// StatePlayer

StatePlayer::StatePlayer(TrackingWindow* window)
	:StateBase(window)
{

}

void StatePlayer::SetPlaying(bool p) 
{ 
	playing = p;
	window->SetPlaying(p); 
}

void StatePlayer::UpdateButtons(ButtonListOut out)
{
	StateBase::UpdateButtons(out);
	
	AddButton(out, "Add tracking", [](auto w) {
		w->PushState(new StateEditSet(w, w->AddSet()));
	}, KC_ADD);
}

void StatePlayer::UpdateFPS(int numFrames)
{
	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames += numFrames;
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

	NextFrame();
}

void StatePlayer::LastFrame()
{
	time_t time = window->GetCurrentPosition() - 4000;
	if (time >= 0)
		window->SetPosition(time);

	AskDraw();
}

void StatePlayer::Draw(Mat& frame)
{
	if (playing)
	{
		putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
		putText(frame, "Press space to pause", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	}
	else
		putText(frame, "Press space to play", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	auto pos = window->GetCurrentPosition();
	for (auto set : window->project.sets)
	{
		if (set->timeStart <= pos && set->timeEnd > pos)
		{
			if (set != lastSet)
			{
				calculator.Reset();
				lastSet = set;
			}

			calculator.Draw(set, frame, window->GetCurrentPosition(), false, true);
			break;
		}
	}
}

bool StatePlayer::HandleStateInput(OIS::KeyCode c)
{
	if (c == KC_SPACE)
	{
		SetPlaying(!playing);
		window->DrawWindow(true);

		if (playing)
		{
			lastUpdate = high_resolution_clock::now();
		}

		return true;
	}

	// Left
	if (c == KC_LEFT)
	{
		LastFrame();
		return true;
	}
	
	// Right
	if (c == KC_RIGHT)
	{
		NextFrame();
		return true;
	}

	return false;
}

void StatePlayer::EnterState(bool again)
{
	window->SetPlaying(playing);
}

void StatePlayer::SyncFps()
{
	if (window->project.maxFPS == 0)
		return;

	auto n = high_resolution_clock::now();

	int deltaMs = duration_cast<chrono::milliseconds>(n - lastUpdate).count();
	int waitMs = 1000 / window->project.maxFPS;
	if (deltaMs < waitMs)
		this_thread::sleep_for(chrono::milliseconds(waitMs - deltaMs));

	lastUpdate = n;
}

// StatePlayerImpl

StatePlayerImpl::StatePlayerImpl(TrackingWindow* window)
	:StatePlayer(window)
{

}

void StatePlayerImpl::NextFrame()
{
	window->ReadCleanFrame();
	window->UpdateTrackbar();
	AskDraw();
}