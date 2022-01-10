#pragma once

#include <Windows.h>

#include "Main.h"
#include "Reader/VideoReader.h"
#include "StatesUtils.h"

class StateBase;
class TrackingWindow;

struct RGB {
	uchar blue;
	uchar green;
	uchar red;
	uchar alpha;
};

class Timebar : public StateBase
{
public:
	Timebar(TrackingWindow* window);
	void AddGui(Mat& frame);
	void UpdateButtons(vector<GuiButton>& out);

	void SelectTrackingSet(TrackingSet* s);
	TrackingSet* GetSelectedSet() { return selectedSet; }
	TrackingSet* GetNextSet();
	TrackingSet* GetPreviousSet();

	string GetName() const { return "Timebar"; }

protected:
	TrackingSet* selectedSet = nullptr;
};

class StateStack
{
public:
	StateStack(TrackingWindow& window) 
		:window(window)
	{
	};

	void ReloadTop();
	void PushState(StateBase* s);
	void ReplaceState(StateBase* s);
	void PopState();
	StateBase* GetState();
	bool IsDirty() { return dirty; };

protected:
	MultiState* multiState = nullptr;
	vector<StateBase*> stack;
	TrackingWindow& window;
	bool dirty = false;
};

class TrackingWindow {
public:
	TrackingWindow(string fName, track_time startTime);
	~TrackingWindow();
	
	bool ReadCleanFrame(cuda::Stream& stream = cuda::Stream::Null());

	cuda::GpuMat* GetInFrame() { return inFrame; };
	Mat* GetOutFrame() { return &outFrame; };
	void DrawWindow(bool updateButtons = false);
	
	
	void Run();
	void UpdateTrackbar();

	TrackingSet* AddSet();
	void DeleteTrackingSet(TrackingSet* s);

	track_time GetCurrentPosition();
	track_time GetDuration();
	void SetPosition(track_time position, bool updateTrackbar = true);

	bool IsPlaying();
	void SetPlaying(bool playing);
	//AVAudioFifo* GetAudioFifo();


	const string windowName = "TrackingWindow";
	const string spectrumWindowName = "AudioWindow";

	StateStack stack;
	Project project;
	Timebar timebar;
	int maxFPS = 120;

protected:
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool buttonUpdateRequested = false;
	bool drawRequested = false;
	void ReallyDrawWindow(cuda::Stream& stream = cuda::Stream::Null());

	void RunOnce();

	bool pressingButton = false;
	bool isPlaying = false;

	cuda::GpuMat* inFrame;
	Mat outFrame;
	Ptr<VideoReader> videoReader;
	vector<GuiButton> buttons;

	char lastKey = 0;
	float videoScale = 1;
};