#pragma once

#include "Main.h"

class StateBase;

class TrackingWindow {
public:
	TrackingWindow(string fName, track_time startTime);
	
	bool ReadFrame(bool next);
	bool ReadCleanFrame();

	Mat* GetInFrame() { return &inFrame; };
	Mat* GetOutFrame() { return &outFrame; };

	void DrawWindow();
	
	void Run();
	void UpdateTrackbar();

	void PushState(StateBase* s);
	void ReplaceState(StateBase* s);
	void PopState();
	StateBase* GetState() { if (stateStack.size() == 0) return nullptr; return stateStack.back(); };
	TrackingSet* AddSet();
	void SelectTrackingSet(TrackingSet* s);
	TrackingSet* GetSelectedSet() { return selectedSet; }
	TrackingSet* GetNextSet();
	TrackingSet* GetPreviousSet();

	const string windowName = "TrackingWindow";

protected:
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	void RunOnce();
	void DrawTimebar(Mat& frame);

	void SetPosition(track_time position);
	track_time GetCurrentPosition();
	track_time GetDuration();

	Mat inFrame;
	Mat outFrame;

	VideoCapture cap;
	vector<TrackingSet> sets;
	vector<StateBase*> stateStack;
	char lastKey = 0;
	TrackingSet* selectedSet = nullptr;

	float videoScale = 1;
};