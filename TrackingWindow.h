#pragma once

#include <Windows.h>

#include "Main.h"
#include "VideoSource.h"
#include "opencv2/cudacodec.hpp"

#include "SpecUtil.h"

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
	map<TrackingSet*, Rect> GetSetRects();
	map<TrackingSet*, Rect> rects;
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
	
	bool ReadCleanFrame();

	cuda::GpuMat* GetInFrame() { return &inFrame; };
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
	AVAudioFifo* GetAudioFifo();


	const string windowName = "TrackingWindow";
	const string spectrumWindowName = "AudioWindow";

	StateStack stack;
	Project project;
	Timebar timebar;

protected:
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool drawRequested = false;
	void ReallyDrawWindow();

	void RunOnce();
	void UpdateAudio();

	
	bool isPlaying = false;
	track_time lastPlayedFrame = 0;

	cuda::GpuMat inFrame;
	Mat outFrame;
	

	Ptr<MyVideoSource> videoSource;
	Ptr<cudacodec::VideoReader> videoReader;
	AVAudioFifo* audioFifoIn;
	AVAudioFifo* audioFifoOut;
	spectrum* spec;

	//int spectrumLength = 0;
	int spectrumWindowSize = 2024;
	int spectrumWindowOverlap = 300;

	//int spectrumWindowSizeMax = 0;
	
	//int spectrumFreqDiv = 30;
	int spectrumFreqMax = 70;
	double spectrumValMin = 2;
	int spectrumValMax = 3;

	int spectrumHeight = 800;
	int spectrumWidth = 200;
	int spectrumSamplePosition = 0;

	//int spectrumRangeBottom = 0;
	//int spectrumRangeTop = spectrumHeight;
	//int spectrumRangeValueMin = 0;
	//int spectrumRangeValueMax = spectrumHeight;

	Mat spectrumDisplay;
	Mat spectrumInput;

	char lastKey = 0;

	float videoScale = 0.7;
};