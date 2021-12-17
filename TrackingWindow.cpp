#include "TrackingWindow.h"
#include "States.h"
#include <opencv2/cudawarping.hpp>
#include <opencv2/plot.hpp>

#include "miniaudio.h"
ma_device audioDevice;

#include <fftw3.h>

fftw_plan fft;

void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	TrackingWindow* window = reinterpret_cast<TrackingWindow*>(pDevice->pUserData);

	if(window->IsPlaying())
		av_audio_fifo_read(window->GetAudioFifo(), &pOutput, frameCount);

	(void)pInput;
}

void TrackingWindow::SetPlaying(bool playing)
{
	if (playing)
	{
		av_audio_fifo_reset(audioFifoOut);
	}
	isPlaying = playing;
}

bool TrackingWindow::IsPlaying()
{
	return isPlaying;
}

AVAudioFifo* TrackingWindow::GetAudioFifo()
{
	return audioFifoOut;
}

TrackingWindow::TrackingWindow(string fName, track_time startTime)
{
	/*
	spectrumLength = spectrumHeight * (22050 / 10 / spectrumHeight + 1);
	for (int i = 0; ; ++i)
	{
		if (is_good_speclen(spectrumLength + i))
		{
			spectrumLength += i;
			break;
		}
		if (spectrumLength - i >= spectrumHeight && is_good_speclen(spectrumLength - i))
		{
			spectrumLength -= i;
			break;
		}
	}

	spectrumWindowSize = spectrumLength / 2;
	spectrumWindowSizeMax = spectrumLength * 2;
	*/
	
	//spec = create_spectrum(spectrumLength, NUTTALL);
	spectrumDisplay = Mat(spectrumHeight, spectrumWidth, CV_8UC4);
	spectrumInput = Mat(1, spectrumWindowSize, CV_64F);

	namedWindow(windowName, 1);

	videoSource = new MyVideoSource(fName);
	
	videoSource->InitAudioDevice(audioDevice, audio_data_callback, this);
	audioFifoIn = videoSource->GetAudioFifo();
	audioFifoOut = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, videoSource->GetNumAudioChannels(), 1);

	videoSource->Seek(startTime);
	videoReader = cudacodec::createVideoReader(videoSource);
	ReadCleanFrame();

	setMouseCallback(windowName, &OnClick, this);
	createTrackbar("Position", windowName, 0, 1000, &OnTrackbar, this);
	UpdateTrackbar();

	namedWindow(spectrumWindowName, 1);
	createTrackbar("Max", spectrumWindowName, &spectrumValMax, 150);
	//createTrackbar("FreqMax", spectrumWindowName, &spectrumFreqMax, 22050);
	//createTrackbar("FreqDiv", spectrumWindowName, &spectrumFreqDiv, 200);
	//createTrackbar("WinSize", spectrumWindowName, &spectrumWindowSize, spectrumWindowSizeMax);
	//createTrackbar("WinOverl", spectrumWindowName, &spectrumWindowOverlap, spectrumWindowSizeMax);

	//createTrackbar("Range Min", spectrumWindowName, &spectrumRangeBottom, spectrumHeight);
	//createTrackbar("Range Max", spectrumWindowName, &spectrumRangeTop, spectrumHeight);
	//createTrackbar("RVal Min", spectrumWindowName, &spectrumRangeValueMin, spectrumHeight);
	//createTrackbar("RVal Max", spectrumWindowName, &spectrumRangeValueMax, spectrumHeight);

	PushState(new StatePaused(this));
}

void TrackingWindow::UpdateAudio()
{

	ma_uint32 size = min(10000, av_audio_fifo_size(audioFifoIn));
	void* audioData = malloc(sizeof(float) * size * 2);
	av_audio_fifo_read(audioFifoIn, &audioData, size);
	av_audio_fifo_write(audioFifoOut, &audioData, size);
	return;

	float* fltAudioData = (float*)audioData;

	for (int i = 0; i < size; i++)
	{
		if (spectrumSamplePosition >= spectrumWindowSize)
		{
			if (spectrumWindowOverlap > 0)
			{
				spectrumInput(Rect(
					spectrumWindowSize - spectrumWindowOverlap,
					0,
					spectrumWindowOverlap,
					1
				))
					.copyTo(spectrumInput(Rect(
						0,
						0,
						spectrumWindowOverlap,
						1
					)));
			}

			spectrumSamplePosition = spectrumWindowOverlap;

			vector<double> han;
			for (int i = 0; i <spectrumWindowSize; i++)
			{
				double v = 0.5 * (1.0 - cos(2.0 * M_PI * i / (spectrumWindowSize - 1)));
				han.push_back(v);
			}

			Mat tmp;
			Mat inputHanned;

			multiply(spectrumInput, han, inputHanned);

			/*
			Ptr<plot::Plot2d> plot = plot::Plot2d::create(inputHanned);
			plot->setShowText(false);
			plot->setPlotSize(1800, 400);
			plot->render(tmp);
			imshow("Input", tmp);
			*/

			Mat dftOut(1, spectrumWindowSize, CV_64FC2);
			
			//fft = fftw_plan_r2r_1d(spectrumWindowSize, (double*)inputHanned.data, (double*)dftOut.data, FFTW_R2HC, FFTW_MEASURE | FFTW_PRESERVE_INPUT);
			//fftw_execute(fft);
			dft(inputHanned, dftOut, DFT_COMPLEX_OUTPUT);


			Mat pts[2] = { Mat(dftOut.size(), dftOut.type()), Mat(dftOut.size(), dftOut.type()) };
			split(dftOut, pts);
			Mat dftMag(1, spectrumWindowSize, CV_64F);
			
			magnitude(pts[0], pts[1], dftMag);
			//log(dftMag, dftMag);
			//normalize(dftMag, dftMag, 0, 1, NORM_MINMAX);
			dftMag = dftMag(Rect(0, 0, spectrumFreqMax, 1));
			resize(dftMag, dftMag, Size(spectrumHeight, 1));

			

			vector<double> vals;
			for (int h=0; h < spectrumHeight; h++)
			{
				vals.push_back(dftMag.at<double>(h));
			}

			Mat tmpMat(spectrumDisplay.size(), spectrumDisplay.type());

			
			spectrumDisplay(Rect(0, 0, spectrumWidth - 1, spectrumHeight))
				.copyTo(tmpMat(Rect(1, 0, spectrumWidth - 1, spectrumHeight)));

			for (int h = 0; h < spectrumHeight; h++)
			{
				double val = min((double)spectrumValMax, dftMag.at<double>(h));
				if (val < spectrumValMin)
					val = 0;

				RGB* c = tmpMat.ptr<RGB>(spectrumHeight - h - 1, 0);
				int color = mapValue<double, int>(val, 0, spectrumValMax, 0, 254);
				c->red = color;
				c->green = color;
				c->blue = 150;
			}

			spectrumDisplay = tmpMat.clone();

			/*
			//double min, max;
			//minMaxLoc(dftMag, &min, &max);
			
			Ptr<plot::Plot2d> plot2 = plot::Plot2d::create(vals);
			Mat plotFlip;
			plot2->setShowText(false);
			plot2->setPlotSize(100, spectrumHeight);
			plot2->setMaxY(spectrumValMax);
			plot2->render(tmp);
			flip(tmp, plotFlip, 0);

			imshow("Dft", plotFlip);
			*/
			
		}
		else
		{
			//double mixedSample = (double)(fltAudioData[i * 2] + fltAudioData[(i * 2) + 1]) / 2;
			//spectrumInput.at<double>(spectrumSamplePosition) = mixedSample;
			spectrumInput.at<double>(spectrumSamplePosition) = fltAudioData[i * 2];
			spectrumSamplePosition++;
		}
	}

	/*
	float* fltAudioData = (float*)audioData;
	double* spectrumDataPtr = spec->time_domain;
	int spectrumDataLen = 2 * spectrumLength + 1;

	for (int i = 0; i < size; i++)
	{
		if (spectrumSamplePosition >= spectrumWindowSize)
		{
			if (spectrumWindowOverlap > 0)
			{
				spectrumSamplePosition = spectrumWindowOverlap;
				int overlapStart = max(0, spectrumWindowSize - spectrumWindowOverlap);

				for (int o = 0; o < spectrumWindowOverlap; o++)
				{
					spectrumDataPtr[o] = spectrumDataPtr[overlapStart + o];
				}

				spectrumSamplePosition = spectrumWindowOverlap;
			}
			else
			{
				spectrumSamplePosition = 0;
			}



			//memcpy(spectrumDataPtr, spectrumInput.data, sizeof(double) * spectrumWindowSizeMax);
			float* spectrumMagnitude = (float*)calloc(spectrumHeight, sizeof(float));

			calc_magnitude_spectrum(spec);
			interp_spec(
				spectrumMagnitude,
				spectrumHeight,
				spec->mag_spec,
				spectrumLength,
				spectrumFreqMin,
				spectrumFreqMax,
				22050
			);

			double LINEAR_SPEC_FLOOR = pow(10.0, SPEC_FLOOR_DB / 20.0);
			unsigned char colour[3] = { 0, 0, 0 };

			Mat tmpMat(spectrumDisplay.size(), spectrumDisplay.type());

			spectrumDisplay(Rect(0, 0, spectrumWidth - 1, spectrumHeight))
				.copyTo(tmpMat(Rect(1, 0, spectrumWidth - 1, spectrumHeight)));

			vector<float> rangeMagnitudes;

			for (int h = 0; h < spectrumHeight; h++)
			{
				int div = max(1, spectrumFreqDiv);
				float magnitude = spectrumMagnitude[h] /= div;
				magnitude = (magnitude < LINEAR_SPEC_FLOOR) ? SPEC_FLOOR_DB : 20.0 * log10(magnitude);

				int y = spectrumHeight - h - 1;

				if (y >= spectrumRangeBottom && y <= spectrumRangeTop)
					rangeMagnitudes.push_back(magnitude);

				get_colour_map_value(magnitude, SPEC_FLOOR_DB, colour, false);
				RGB* c = tmpMat.ptr<RGB>(y, 0);

				c->red = colour[0];
				c->green = colour[1];
				c->blue = colour[2];
			}

			if (rangeMagnitudes.size() > 0)
			{
				Scalar t = mean(rangeMagnitudes);
				int avgY = mapValue<float, int>(
					t[0],
					mapValue<int, float>(spectrumRangeBottom, 0, spectrumHeight, SPEC_FLOOR_DB, 0),
					mapValue<int, float>(spectrumRangeTop, 0, spectrumHeight, SPEC_FLOOR_DB, 0),
					0,
					spectrumHeight
					);

				if (avgY >= 0 && avgY <= spectrumHeight)
				{
					RGB* c = tmpMat.ptr<RGB>(avgY, 0);
					c->red = 255; c->green = 255; c->blue = 255;
				}
			}

			spectrumDisplay = tmpMat.clone();

			//Mat tmpMat(1, spectrumHeight, CV_32F, spectrumMagnitude);

			
			Mat yData(1, spectrumLength, CV_64F);

			for (int i = 0; i < spectrumLength; i++)
			{
				yData.at<double>(i) = spectrumInput.at<double>(i);
			}

			Ptr<plot::Plot2d> plot = plot::Plot2d::create(yData);
			Mat plotImage;
			plot->setShowText(false);
			plot->setPlotSize(1800, 400);
			plot->render(plotImage);

			imshow("Plot", plotImage);
			
			
			memset(spectrumDataPtr, 0, spectrumDataLen);
			free(spectrumMagnitude);
		}
		else
		{
			double mixedSample = (double)(fltAudioData[i * 2] + fltAudioData[(i * 2) + 1]) / 2;
			spectrumInput.at<double>(spectrumSamplePosition) = mixedSample;

			spectrumDataPtr[spectrumSamplePosition] = mixedSample;

			spectrumSamplePosition++;
		}
	}

	*/

	free(audioData);
}


TrackingWindow::~TrackingWindow()
{
	//delete(videoReader);
	//delete(videoSource);
	ma_device_uninit(&audioDevice);
}

void TrackingWindow::DrawWindow()
{
	auto s = GetState();
	if (!s)
		return;

	lastPlayedFrame = GetCurrentPosition();

	inFrame.download(outFrame);

	s->AddGui(outFrame);
	DrawTimebar();
	UpdateAudio();
	putText(outFrame, "State: " + s->GetName(), Point(30, 60), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	cuda::GpuMat specFrame(spectrumDisplay);
	cuda::GpuMat specFrameResized;
	cuda::resize(specFrame, specFrameResized, Size(1900, 600));
	Mat specCopy;
	specFrameResized.download(specCopy);
	imshow(spectrumWindowName, specCopy);

	if (videoScale != 1)
	{
		Size size(
			outFrame.size().width * videoScale,
			outFrame.size().height * videoScale
		);

		cuda::GpuMat gpuOutFrame(outFrame);
		cuda::GpuMat gpuResizedFrame(size, gpuOutFrame.type());
		cuda::resize(gpuOutFrame, gpuResizedFrame, size);
		gpuResizedFrame.download(outFrame);
	}

	imshow(windowName, outFrame);
}

bool TrackingWindow::ReadCleanFrame()
{
	bool ret = videoReader->nextFrame(inFrame) && !inFrame.empty();
	if (!ret)
		return false;

	// Play audio

	if (videoScale != 1)
	{
		int w = inFrame.size().width * videoScale;
		int h = inFrame.size().height * videoScale;

		Size s(w, h);

		cuda::GpuMat resized;
		cuda::resize(inFrame, resized, s);

		inFrame = resized;
	}

	return true;
}

void TrackingWindow::Run()
{
	while (GetState() != nullptr)
		RunOnce();
}

void TrackingWindow::RunOnce()
{
	auto s = GetState();
	if (!s)
		return;

	if (s->ShouldPop())
	{
		PopState();
		return;
	}

	s->Update();

	// Keyboard handling
	char k = waitKey(1);
	if (k == lastKey)
		return;

	if (k == 'q')
		PopState();
	else
		s->HandleInput(k);

	lastKey = k;
}

void TrackingWindow::DrawTimebar()
{
	track_time d = GetDuration();
	track_time c = GetCurrentPosition();
	int w = videoReader->format().width;

	rectangle(
		outFrame,
		Rect(
			20,
			20,
			w - 40,
			20
		),
		Scalar(255, 0, 0),
		2
	);

	int x;

	for (auto& s : sets)
	{
		x = mapValue<track_time, int>(s.timeStart, 0, d, 20, w - 40);
		if (selectedSet == &s) 
			line(outFrame, Point(x, 20), Point(x, 40), Scalar(0, 255, 255), 10);
		else
			line(outFrame, Point(x, 20), Point(x, 40), Scalar(0, 255, 0), 6);
	}

	x = mapValue<track_time, int>(c, 0, d, 20, w - 40);
	line(outFrame, Point(x, 20), Point(x, 40), Scalar(255, 0, 0), 3);
}

void TrackingWindow::PushState(StateBase* s)
{
	stateStack.push_back(s);
	s->EnterState();
	DrawWindow();
}

void TrackingWindow::ReplaceState(StateBase* s)
{
	assert(stateStack.size() > 0);

	PopState();
	PushState(s);
}

void TrackingWindow::PopState()
{
	assert(stateStack.size() > 0);

	auto s = stateStack.back();
	stateStack.pop_back();
	s->LeaveState();
	delete(s);

	SetPlaying(false);
	
	if (stateStack.size() > 0)
		DrawWindow();
}

void TrackingWindow::OnTrackbar(int v, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->GetState();
	if (!s)
		return;

	if (s->GetName() != "Paused")
	{
		me->UpdateTrackbar();
		return;
	}

	track_time t = mapValue<int, track_time>(v, 0, 1000, 0, me->GetDuration());
	me->SetPosition(t);
}

void TrackingWindow::OnClick(int e, int x, int y, int f, void* p)
{
	TrackingWindow* me = (TrackingWindow*)p;
	auto s = me->GetState();
	if (!s)
		return;

	Size outSize = me->GetInFrame()->size();
	Size inSize = me->GetOutFrame()->size();


	x = mapValue<int, int>(x, 0, inSize.width, 0, outSize.width);
	y = mapValue<int, int>(y, 0, inSize.height, 0, outSize.height);

	s->HandleMouse(e, x, y, f);

	if (s->ShouldPop())
		me->PopState();
}

void TrackingWindow::SetPosition(track_time position)
{
	videoSource->Seek(position);
	videoReader->emptyBuffers();
	ReadCleanFrame();

	DrawWindow();
}

track_time TrackingWindow::GetCurrentPosition()
{
	return videoSource->GetPosition();
}

track_time TrackingWindow::GetDuration()
{
	return videoSource->GetDuration();
}

void TrackingWindow::UpdateTrackbar()
{
	int t = mapValue<track_time, int>(GetCurrentPosition(), 0, GetDuration(), 0, 1000);
	setTrackbarPos("Position", windowName, t);
}

TrackingSet* TrackingWindow::AddSet()
{
	track_time t = GetCurrentPosition();
	//Jump to keyframe
	SetPosition(t);

	auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return t <= set.timeStart; });
	
	//if (it != sets.end() && it->timeStart == t)
	//	return nullptr;
	
	it = sets.emplace(it, GetCurrentPosition(), GetDuration());
	return &(*it);
}

void TrackingWindow::SelectTrackingSet(TrackingSet* s)
{
	selectedSet = s;
	if (s)
	{
		if(s->timeStart != GetCurrentPosition())
			SetPosition(s->timeStart);
	}
};

TrackingSet* TrackingWindow::GetNextSet()
{
	if (sets.size() == 0)
		return nullptr;

	auto s = GetSelectedSet();
	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index < sets.size() - 1)
			return &(sets.at(index + 1));
		else
			return nullptr;
	}
	else
	{
		auto t = GetCurrentPosition();
		auto it = find_if(sets.begin(), sets.end(), [t](TrackingSet& set) { return set.timeStart > t; });
		if (it == sets.end())
			return nullptr;

		return &(*it);
	}
}

TrackingSet* TrackingWindow::GetPreviousSet()
{
	if (sets.size() == 0)
		return nullptr;

	
	auto s = GetSelectedSet();
	if (s)
	{
		auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
		assert(it != sets.end());
		int index = distance(sets.begin(), it);
		if (index > 0)
			return &(sets.at(index - 1));
		else
			return nullptr;
	}
	else
	{
	
		auto t = GetCurrentPosition();

		auto it = find_if(sets.rbegin(), sets.rend(), [t](TrackingSet& set) { return set.timeStart < t; });
		if (it == sets.rend())
			return nullptr;

		return &(*it);
	}
}

void TrackingWindow::DeleteTrackingSet(TrackingSet* s)
{
	auto it = find_if(sets.begin(), sets.end(), [s](TrackingSet& set) { return &set == s; });
	if (it == sets.end())
		return;

	if (selectedSet == s)
	{
		selectedSet = nullptr;
	}

	sets.erase(it);
}