#pragma once

#include <Windows.h>

#include "Main.h"
#include "Reader/VideoReader.h"

class StateBase;
class TrackingWindow;

struct RGB {
	uchar blue;
	uchar green;
	uchar red;
	uchar alpha;
};

class Timebar
{
public:
	Timebar(TrackingWindow& window);
	void AddGui(Mat& frame);
	bool HandleMouse(int e, int x, int y, int f);

	void SelectTrackingSet(TrackingSet* s);
	TrackingSet* GetSelectedSet() { return selectedSet; }
	TrackingSet* GetNextSet();
	TrackingSet* GetPreviousSet();

protected:
	map<TrackingSet*, GuiButton> GetButtons();
	map<TrackingSet*, GuiButton> buttons;

	TrackingWindow& window;
	TrackingSet* selectedSet = nullptr;
};

class StateStack
{
public:
	StateStack(TrackingWindow& window) 
		:window(window)
	{
	};

	void PushState(StateBase* s);
	void ReplaceState(StateBase* s);
	void PopState();
	StateBase* GetState() { if (stack.size() == 0) return nullptr; return stack.back(); };

protected:
	vector<StateBase*> stack;
	TrackingWindow& window;
};

class TrackingWindow {
public:
	TrackingWindow(string fName, track_time startTime);
	~TrackingWindow();
	
	bool ReadCleanFrame(cuda::Stream& stream = cuda::Stream::Null());

	cuda::GpuMat* GetInFrame() { return inFrame; };
	Mat* GetOutFrame() { return &outFrame; };
	void DrawWindow();
	
	
	void Run();
	void UpdateTrackbar();

	TrackingSet* AddSet();
	void DeleteTrackingSet(TrackingSet* s);

	track_time GetCurrentPosition();
	track_time GetDuration();
	void SetPosition(track_time position);

	bool IsPlaying();
	void SetPlaying(bool playing);
	//AVAudioFifo* GetAudioFifo();


	const string windowName = "TrackingWindow";
	const string spectrumWindowName = "AudioWindow";

	StateStack stack;
	Project project;
	Timebar timebar;

protected:
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool drawRequested = false;
	void ReallyDrawWindow(cuda::Stream& stream = cuda::Stream::Null());

	void RunOnce();

	bool pressingButton = false;
	bool isPlaying = false;

	cuda::GpuMat* inFrame;
	Mat outFrame;
	Ptr<VideoReader> videoReader;

	char lastKey = 0;
	float videoScale = 1;
};