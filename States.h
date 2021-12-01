#pragma once

#include "Main.h"

#include <chrono>
#include <string>
#include <functional>

#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/tracking.hpp>

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

	void AddTargetPoints(vector<Point2f>& points);
	void AddTargetRect(Rect& rect);

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

	void GetPoints(Rect r);
	
	void LeaveState()
	{
		if (!returned)
			failedCallback();
	}

	void AddGui(Mat& frame);
	string GetName() const { return "AddTracker"; }

protected:
	bool returned = false;
	AddTrackerCallback callback;
	StateFailedCallback failedCallback;
};

typedef function<void(Rect2f&)> SelectRoiCallback;
class StateSelectRoi : public StateBase
{
public:
	StateSelectRoi(TrackingWindow* window, string title, SelectRoiCallback callback, StateFailedCallback failedCallback)
		:StateBase(window), callback(callback), failedCallback(failedCallback), title(title)
		,p1(-1, -1), p2(-1, -1)
	{

	}

	void LeaveState()
	{
		if (!returned)
			failedCallback();
	}

	void HandleMouse(int e, int x, int y, int f);
	void AddGui(Mat& frame);

	string GetName() const { return "SelectRoi"; }

protected:
	string title;
	bool returned = false;
	SelectRoiCallback callback;
	StateFailedCallback failedCallback;

	Point2f p1;
	Point2f p2;
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

	bool playing = true;
};


struct trackerStateStruct
{
	Ptr<Tracker> tracker;
	Rect rect;
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

	void AddGui(Mat& frame);
	string GetName() const { return "TrackSetTracker"; }

protected:
	TrackingSet* set;
	map<TrackingTarget*, trackerStateStruct> trackerStates;

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