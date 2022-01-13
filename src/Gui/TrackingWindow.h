#pragma once

#include "Reader/VideoReader.h"

#include "GuiElement.h"
#include "Timebar.h"
#include "StateStack.h"
#include "Model/Project.h"

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
	
	bool ReadCleanFrame(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	cv::cuda::GpuMat GetInFrame() { return inFrame; };
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
	bool keyPressed(const OIS::KeyEvent& arg) override;
	bool keyReleased(const OIS::KeyEvent& arg) override;

	void UpdateElements();

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
	static void OnTrackbar(int v, void* p);
	static void OnClick(int e, int x, int y, int f, void* p);

	bool elementUpdateRequested = false;
	bool drawRequested = false;
	void ReallyDrawWindow(cv::cuda::Stream& stream = cv::cuda::Stream::Null());

	void RunOnce();

	bool trackbarUpdating = false;
	bool pressingButton = false;
	bool isPlaying = false;

	cv::cuda::GpuMat resizeBuffer;
	cv::cuda::GpuMat inFrame;
	cv::Mat outFrame;

	OIS::InputManager* inputManager = nullptr;
	cv::Ptr<VideoReader> videoReader;

	ButtonList buttons;
	std::vector<std::reference_wrapper<GuiElement>> guiElements;

	float videoScale = 1;
};