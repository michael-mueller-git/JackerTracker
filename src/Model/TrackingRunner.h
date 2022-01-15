#pragma once

#include "TrackingTarget.h"
#include "Tracking/Trackers.h"
#include "Model/Calculator.h"
#include "Reader/VideoReader.h"
#include <thread>
#include <opencv2/core/cuda.hpp>
#include <deque>

struct ThreadWork {
	ThreadWork(cv::cuda::GpuMat frame, time_t frameTime)
		:frame(frame), frameTime(frameTime)
	{

	}

	cv::cuda::GpuMat frame;
	time_t frameTime;

	int durationMs = 999;
	bool done = false;
	bool err = false;
};

typedef std::shared_ptr<ThreadWork> ThreadWorkPtr;

class TrackerBinding
{
public:
	TrackerBinding(TrackingSetPtr set, TrackingTarget* target, TrackerJTStruct& trackerStruct, bool saveResults);
	~TrackerBinding();

	void Start();
	void Join();

	std::mutex workMtx;

	std::deque<ThreadWorkPtr> work;
	
	std::unique_ptr<TrackingStatus> state;
	std::unique_ptr<TrackerJT> tracker;
	TrackerJTStruct trackerStruct;
	int lastUpdateMs = 0;

protected:
	void RunThread();
	bool running = false;
	std::thread myThread;

	TrackingTarget* target;
	TrackingSetPtr set;

	bool saveResults;
};

struct RunnerState
{
	int framesRdy = 0;
	time_t lastTime = 0;
	int lastWorkMs = 999;
};

struct FrameWork
{
	time_t time;
	std::vector<ThreadWorkPtr> work;
	std::chrono::steady_clock::time_point timeStart;
};

class TrackingRunner
{
public:
	TrackingRunner(TrackingWindow* w, TrackingSetPtr set, TrackingTarget* target, bool saveResults = false, bool allTrackerTypes = false, bool videoThread = false);
	~TrackingRunner();

	bool Setup();
	void AddTarget(TrackingTarget* target);
	void Update(bool blocking = false);
	void Draw(cv::Mat& frame);
	void SetRunning(bool r);

	RunnerState GetState(bool resetFramesRdy = false);

	std::vector<std::unique_ptr<TrackerBinding>> bindings;

protected:
	void PushWork(int frames = 20);
	void PopWork();

	std::map <time_t, FrameWork> workMap;
	cv::Ptr<VideoReader> videoReader = nullptr;
	
	TrackingSetPtr set;
	TrackingTarget* target;
	TrackingWindow* w;
	TrackingCalculator calculator;

	bool initialized = false;
	bool allTrackerTypes = false;
	bool saveResults = false;
	bool running = false;
	RunnerState state;

	const int maxWork = 40;
};
