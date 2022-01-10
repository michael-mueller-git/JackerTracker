#pragma once

#include "Reader/VideoReader.h"

#include "Timebar.h"
#include "StateStack.h"
#include "Model/Project.h"

#include <opencv2/core/cuda.hpp>
#include <string>

class TrackingWindow {
public:
	TrackingWindow(std::string fName);
	~TrackingWindow();
	
	bool ReadCleanFrame(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	cv::cuda::GpuMat* GetInFrame() { return inFrame; };
	cv::Mat* GetOutFrame() { return &outFrame; };
	void DrawWindow(bool updateButtons = false);
	
	
	void Run();
	void UpdateTrackbar();

	TrackingSet* AddSet();
	void DeleteTrackingSet(TrackingSet* s);

	time_t GetCurrentPosition();
	time_t GetDuration();
	void SetPosition(time_t position, bool updateTrackbar = true);

	bool IsPlaying();
	void SetPlaying(bool playing);


	const std::string windowName = "TrackingWindow";
	const std::string spectrumWindowName = "AudioWindow";

	StateStack stack;
	Project project;
	Timebar timebar;
	int maxFPS = 120;

protected:
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool buttonUpdateRequested = false;
	bool drawRequested = false;
	void ReallyDrawWindow(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	void RunOnce();

	bool pressingButton = false;
	bool isPlaying = false;

	cv::cuda::GpuMat* inFrame;
	cv::Mat outFrame;
	cv::Ptr<VideoReader> videoReader;
	std::vector<GuiButton> buttons;

	char lastKey = 0;
	float videoScale = 1;
};