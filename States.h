#pragma once

#include <Windows.h>

#include "Main.h"
#include "Trackers.h"

#include <chrono>
#include <string>
#include <functional>

#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

using namespace chrono;
using namespace std;

class TrackingWindow;
typedef function<void()> StateFailedCallback;

class StateBase {
public:
	StateBase(TrackingWindow* window)
		:window(window)
	{

	}

	virtual void EnterState() {};
	virtual void LeaveState() {};

	virtual void AddGui(Mat& frame) { ; }
	virtual void Update() {};
	virtual void HandleInput(char c) {};
	virtual void HandleMouse(int e, int x, int y, int f) {};
	virtual string GetName() const = 0;

	bool ShouldPop() { return popMe; };
	void Pop() { popMe = true; };

protected:
	TrackingWindow* window;
	bool popMe = false;
};



class StatePaused : public StateBase
{
public:
	using StateBase::StateBase;

	void HandleInput(char c);
	void AddGui(Mat& frame);

	string GetName() const { return "Paused"; }
};

class StatePlaying : public StateBase
{
public:
	using StateBase::StateBase;

	virtual void Update();

	void EnterState();

	virtual void HandleInput(char c);
	virtual void AddGui(Mat& frame);

	virtual string GetName() const { return "Playing"; }

protected:
	steady_clock::time_point timer;
	unsigned int timerFrames;
	int drawFps;
};

class StateEditSet : public StateBase
{
public:
	StateEditSet(TrackingWindow* window, TrackingSet* set)
		:StateBase(window),set(set)
	{

	}

	void EnterState();
	void LeaveState();

	void AddTargetPoints(vector<Point2f>& intialPoints);
	void AddTargetRect(Rect& initialRect);

	void HandleInput(char c);
	void AddGui(Mat& frame);
	string GetName() const { return "EditSet"; }

protected:
	TrackingSet* set;
};

typedef function<void(vector<Point2f>&)> AddTrackerCallback;
class StateAddTracker : public StateBase
{
public:
	StateAddTracker(TrackingWindow* window, AddTrackerCallback callback, StateFailedCallback failedCallback)
		:StateBase(window), callback(callback), failedCallback(failedCallback)
	{

	}

	void EnterState();

	void AddPoints(Rect r);
	void RemovePoints(Rect r);
	void RenderPoints(Mat& frame);
	
	void LeaveState()
	{
		if (!returned)
			failedCallback();
	}

	void HandleInput(char c);
	void HandleMouse(int e, int x, int y, int f);

	void AddGui(Mat& frame);
	string GetName() const { return "AddTracker"; }

protected:
	bool returned = false;
	AddTrackerCallback callback;
	StateFailedCallback failedCallback;
	vector<Point2f> intialPoints;
};



struct PointStatus
{
	bool status;
	Point2f lastPosition;
	int cudaIndex;
	int cudaIndexNew;
	const int originalIndex;
};

struct gpuFrameStruct
{
	cuda::GpuMat gpuPointsOld;
	cuda::GpuMat gpuPointsNew;
	cuda::GpuMat gpuPointsStatus;
	cuda::GpuMat gpuReduced;
	Point2f center;
	vector<PointStatus> pointStatus;
};

class StateTrackSetPyrLK : public StatePlaying
{
public:
	StateTrackSetPyrLK(TrackingWindow* window, TrackingSet* set)
		:StatePlaying(window), set(set)
	{

	}

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();

	void AddGui(Mat& frame);
	string GetName() const { return "TrackSetPyrLK"; }

protected:
	TrackingSet* set;

	cuda::GpuMat gpuFrameOld;
	cuda::GpuMat gpuFrameNew;
	map<TrackingTarget*, gpuFrameStruct> gpuMap;

	Ptr<cuda::SparsePyrLKOpticalFlow> opticalFlowTracker;

	bool initialized = false;
	bool playing = true;

	float startDistance = 0;
	float targetDistanceMin = 0;
	float targetDistanceMax = 0;
	float position = 0;

	int frameSkip = 2;
};


struct trackerStateStruct
{
	Ptr<Tracker> tracker;
	Ptr<TrackerGpu> trackerGpu;
	Rect initialRect;
	Rect trackingWindow;
	Point lastMove;
};
class StateTrackSetTracker : public StatePlaying
{
public:
	StateTrackSetTracker(TrackingWindow* window, TrackingSet* set)
		:StatePlaying(window), set(set)
	{

	}

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();
	Rect OffsetRect(Rect r, int width);

	void AddGui(Mat& frame);
	string GetName() const { return "TrackSetTracker"; }

protected:
	TrackingSet* set;
	map<TrackingTarget*, trackerStateStruct> trackerStates;
	int trackingRange = 0;
	int skipper = 0;

	bool playing = true;
};

class StateTrackGoturn : public StatePlaying
{
public:
	StateTrackGoturn(TrackingWindow* window, TrackingSet* set)
		:StatePlaying(window), set(set)
	{

	}

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();
	Rect OffsetRect(Rect r, int width);

	void AddGui(Mat& frame);
	string GetName() const { return "TrackGoturn"; }

protected:
	TrackingSet* set;
	map<TrackingTarget*, trackerStateStruct> trackerStates;
	
	int trackingRange = 100;
	dnn::Net net;
	cuda::GpuMat lastFrameGpu;

	bool playing = true;
};

struct trackGpuState
{
	Ptr<TrackerGpu> tracker;
	Rect initialRect;
	Rect trackingWindow;
	Point lastMove;
};
class StateTrackGpu : public StatePlaying
{
public:
	StateTrackGpu(TrackingWindow* window, TrackingSet* set)
		:StatePlaying(window), set(set)
	{

	}

	void HandleInput(char c);
	void EnterState();
	void LeaveState();
	void Update();
	Rect OffsetRect(Rect r, int width);

	void AddGui(Mat& frame);
	string GetName() const { return "TrackGpu"; }

protected:
	TrackingSet* set;
	map<TrackingTarget*, trackGpuState> trackerStates;
	int trackingRange = 0;
	int skipper = 0;

	bool playing = true;
};

class StateTrackResult : public StateBase
{
public:
	StateTrackResult(TrackingWindow* window, string result)
		:StateBase(window), result(result)
	{

	}

	void AddGui(Mat& frame);

	string GetName() const { return "TrackResult"; }
private:
	string result;
};