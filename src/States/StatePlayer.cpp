#include "StatePlayer.h"
#include "StateEditSet.h"
#include "Gui/TrackingWindow.h"

#include <chrono>
#include <thread>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace chrono;
using namespace cv;

void StatePlayer::UpdateButtons(vector<GuiButton>& out)
{
	StateBase::UpdateButtons(out);
	AddButton(out, "Add tracking", '+');
}

void StatePlayer::UpdateFPS()
{
	// Calc FPS
	auto now = steady_clock::now();
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
			lastUpdate = steady_clock::now();
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

	auto n = steady_clock::now();

	int deltaMs = duration_cast<chrono::milliseconds>(n - lastUpdate).count();
	int waitMs = 1000 / window->maxFPS;
	if (deltaMs < waitMs)
		this_thread::sleep_for(chrono::milliseconds(waitMs - deltaMs));

	lastUpdate = n;
}
