#pragma once

#include "Reader/VideoReader.h"

#include "GuiElement.h"
#include "StateStack.h"
#include "Model/Project.h"

#include "Timebar.h"

#include <OISInputManager.h>
#include <OISKeyboard.h>
#include <OISKeyboard.h>

#include <opencv2/core/cuda.hpp>
#include <string>
#include <memory>

class TrackingWindow : public OIS::KeyListener {
public:
	TrackingWindow(std::string fName);
	~TrackingWindow();
	
	cv::cuda::GpuMat GetInFrame() { return inFrame; };
	void SetInFrame(cv::cuda::GpuMat inFrame, bool lock);
	cv::cuda::GpuMat ReadCleanFrame(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	cv::Mat* GetOutFrame() { return &outFrame; };
	void DrawWindow(bool updateButtons = false);
	
	void Run();
	void UpdateTrackbar();

	TrackingSetPtr AddSet();
	void DeleteTrackingSet(TrackingSetPtr s);

	time_t GetCurrentPosition();
	time_t GetDuration();
	void SetPosition(time_t position, bool updateTrackbar = true);

	bool IsPlaying();
	void SetPlaying(bool playing);

	void UpdateElements();
	void ClearElements();

	void PushState(StateBase* state)
	{
		stack.PushState(state);
	}

	const std::string windowName = "TrackingWindow";
	const std::string spectrumWindowName = "AudioWindow";

	StateStack stack;
	Project project;
	Timebar timebar;

	static OIS::Keyboard* inputKeyboard;

protected:

	// Event handlers

	bool keyPressed(const OIS::KeyEvent& arg) override;
	bool keyReleased(const OIS::KeyEvent& arg) override;
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool elementUpdateRequested = false;
	bool drawRequested = false;
	void ReallyDrawWindow(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	void RunOnce();

	bool trackbarUpdating = false;
	bool pressingButton = false;
	bool isPlaying = false;
	bool inFrameLocked = false;

	cv::cuda::GpuMat resizeBuffer;
	cv::cuda::GpuMat inFrame;
	cv::Mat outFrame;

	OIS::InputManager* inputManager = nullptr;
	cv::Ptr<VideoReader> videoReader;

	StateGlobal stateGlobal;
	ButtonList buttons;
	std::vector<std::reference_wrapper<GuiElement>> guiElements;
	std::chrono::steady_clock::time_point lastSeek;
	time_t seekPos = 0;

	float videoScale = 1;
};