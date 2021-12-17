#include "States.h"
#include "TrackingWindow.h"
#include "StatesUtils.h"
#include "Trackers.h"

#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>

void StatePaused::HandleInput(char c)
{
	if (c == ' ')
	{
		window->ReplaceState(new StatePlaying(window));
		return;
	}

	if (c == '>')
	{
		auto s = window->GetNextSet();
		if(s)
			window->PushState(new StateEditSet(window, s));

		return;
	}

	if (c == '<')
	{
		auto s = window->GetPreviousSet();
		if (s)
			window->PushState(new StateEditSet(window, s));

		return;
	}

	if (c == 'a')
	{
		TrackingSet* s = window->AddSet();
		if (s != nullptr)
		{
			window->PushState(new StateEditSet(window, s));
		}
	}
}

void StatePaused::AddGui(Mat& frame)
{
	putText(frame, "Press q to quit", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press space to play", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press a to add a tracking set", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if(window->GetPreviousSet() != nullptr)
		putText(frame, "Press < to select the previous set", Point(30, 160), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (window->GetNextSet() != nullptr)
		putText(frame, "Press > to select the next set", Point(30, 180), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StatePlaying::Update()
{
	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();
	window->UpdateTrackbar();
	window->DrawWindow();
}

void StatePlaying::AddGui(Mat& frame)
{
	putText(frame, "Press q to quit", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, format("FPS=%d", drawFps), Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press space to pause", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StatePlaying::HandleInput(char c)
{
	if (c == ' ')
		window->ReplaceState(new StatePaused(window));
}

void StatePlaying::EnterState()
{
	window->SetPlaying(true);
}

// StateEditSet

void StateEditSet::EnterState()
{
	window->SelectTrackingSet(set);
}

void StateEditSet::LeaveState()
{
	window->SelectTrackingSet(nullptr);
}

void StateEditSet::AddGui(Mat& frame)
{
	putText(frame, "Press q to stop editing this set", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	
	putText(frame, "Press p to add a points tracker", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press r to add a rect tracker", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	
	putText(frame, "Press s to start tracking points", Point(30, 160), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press l to start tracking rects", Point(30, 180), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	
	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 200), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, "Press - delete this set", Point(30, 220), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 220;
	for (auto& t : set->targets)
	{
		putText(frame, " - " + t.GetName(), Point(30, y), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
		y += 20;
	}

	set->Draw(frame);
}

void StateEditSet::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;
	auto s = me->set;

	if (c == '>')
	{
		auto t = window->GetNextSet();
		if (t)
			window->ReplaceState(new StateEditSet(window, t));

		return;
	}

	if (c == '<')
	{
		auto t = window->GetPreviousSet();
		if (t)
			window->ReplaceState(new StateEditSet(window, t));

		return;
	}

	if (c == 's')
	{
		window->PushState(new StateTrackSetPyrLK(window, set));
		return;
	}

	if (c == 'l')
	{
		window->PushState(new StateTrackSetTracker(window, set));
		return;
	}

	if (c == 'g')
	{
		window->PushState(new StateTrackGoturn(window, set));
		return;
	}

	if (c == 'p')
	{
		window->PushState(new StateAddTracker(window, [me](vector<Point2f> &intialPoints){

			me->AddTargetPoints(intialPoints);

			},
			// Failed
			[]() {}
		));
	}

	if (c == 'r')
	{
		window->PushState(new StateSelectRoi(window, "Select feature to track", [me](Rect initialRect) {

			me->AddTargetRect(initialRect);

			},
			// Failed
				[]() {}
			));
	}

	if (c == '-')
	{
		window->PushState(new StateConfirm(window, "Are you sure you want to delete this set?",
			// Yes
			[me, window, s]()
			{
				window->DeleteTrackingSet(s);
				me->Pop();
			},
			// No
				[]() {; }
			));
	}
}

void StateEditSet::AddTargetRect(Rect& initialRect)
{
	TrackingTarget trackingTarget;
	if (set->targets.size() == 0)
		trackingTarget.targetType = TrackingTarget::TYPE_FEMALE;
	else if (set->targets.size() == 1)
		trackingTarget.targetType = TrackingTarget::TYPE_MALE;
	else
		trackingTarget.targetType = TrackingTarget::TYPE_BACKGROUND;

	trackingTarget.trackingType = TrackingTarget::TYPE_RECT;
	trackingTarget.initialRect = initialRect;
	set->targets.push_back(trackingTarget);
	
	window->DrawWindow();
}

void StateEditSet::AddTargetPoints(vector<Point2f>& intialPoints)
{
	TrackingTarget trackingTarget;
	if (set->targets.size() == 0)
		trackingTarget.targetType = TrackingTarget::TYPE_FEMALE;
	else if (set->targets.size() == 1)
		trackingTarget.targetType = TrackingTarget::TYPE_MALE;
	else
		trackingTarget.targetType = TrackingTarget::TYPE_BACKGROUND;

	trackingTarget.trackingType = TrackingTarget::TYPE_POINTS;
	trackingTarget.intialPoints = intialPoints;
	set->targets.push_back(trackingTarget);
	
	window->DrawWindow();
}

// StateAddTracker

void StateAddTracker::EnterState()
{
	
}

void StateAddTracker::RemovePoints(Rect r)
{
	vector<Point2f> newPoints;

	copy_if(
		intialPoints.begin(),
		intialPoints.end(),
		back_inserter(newPoints),
		[r](Point2f p)
		{
			bool inside =
				p.x > r.x && p.x < r.x + r.width &&
				p.y > r.y && p.y < r.y + r.height;

			return !inside;
		}
	);

	intialPoints = newPoints;
	window->DrawWindow();
}

void StateAddTracker::AddPoints(Rect r)
{
	cuda::GpuMat* gpuFrame = window->GetInFrame();
	auto c = gpuFrame->channels();

	cuda::GpuMat gpuFrameGray(gpuFrame->size(), gpuFrame->type());
	cuda::cvtColor(*gpuFrame, gpuFrameGray, COLOR_BGRA2GRAY);

	cuda::GpuMat gpuPoints(gpuFrame->size(), gpuFrame->type());
	Ptr<cuda::CornersDetector> detector = cuda::createGoodFeaturesToTrackDetector(
		gpuFrameGray.type(),
		30,
		0.01,
		30
	);

	Mat mask = Mat::zeros(gpuFrameGray.rows, gpuFrameGray.cols, gpuFrameGray.type());
	rectangle(mask, r, Scalar(255, 255, 255), -1);
	cuda::GpuMat gpuMask(mask);

	detector->detect(gpuFrameGray, gpuPoints, gpuMask);
	if (!gpuPoints.empty())
	{
		vector<Point2f> addPoints;
		gpuPoints.download(addPoints);

		for (auto& a : addPoints)
			intialPoints.push_back(a);

		window->DrawWindow();
	}
	
}

void StateAddTracker::HandleInput(char c)
{
	auto window = this->window;
	auto me = this;

	if (c == 'r')
	{
		window->PushState(new StateSelectRoi(window, "Select where to add points",
			[me](Rect roi) {
				me->AddPoints(roi);
			},
			// Failed
				[me]() { },
			// Append gui
				[me](Mat& frame)
			{
				me->RenderPoints(frame);
			}
		));

		return;
	}

	if (c == 'd')
	{
		window->PushState(new StateSelectRoi(window, "Select where to remove points",
			[me](Rect roi) {
				me->RemovePoints(roi);
			},
			// Failed
				[me]() { },
			// Append gui
				[me](Mat& frame)
			{
				me->RenderPoints(frame);
			}
		));

		return;
	}

	if (c == 'o')
	{
		if (intialPoints.size() > 0)
		{
			callback(intialPoints);
			returned = true;
		}
		Pop();
		return;
	}
}

void StateAddTracker::HandleMouse(int e, int x, int y, int f)
{
	if (e == EVENT_LBUTTONUP)
	{
		intialPoints.emplace_back(Point2f(x, y));
		window->DrawWindow();
	}
}

void StateAddTracker::AddGui(Mat& frame)
{
	putText(frame, "Press r to add points in a rectanlge", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press d remove points in a rectangle", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Click add a point", Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press o when done", Point(30, 160), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press q to cancel", Point(30, 180), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	RenderPoints(frame);
}

void StateAddTracker::RenderPoints(Mat& frame)
{
	for (auto& p : intialPoints)
	{
		circle(frame, p, 4, Scalar(255, 0, 0), 3);
	}
}

// StateTrackSetPyrLK

void StateTrackSetPyrLK::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackSetPyrLK::EnterState()
{
	window->SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	gpuFrameOld = gpuFrame->clone();
	cuda::cvtColor(*gpuFrame, gpuFrameOld, COLOR_BGRA2GRAY);

	for (auto& t : set->targets)
	{
		gpuFrameStruct s;
		s.gpuPointsOld = cuda::GpuMat(t.intialPoints);

		for (int p=0; p<t.intialPoints.size(); p++)
		{
			s.pointStatus.push_back({
				true,
				t.intialPoints[p],
				p,
				-1,
				p
			});
		}

		gpuMap.insert(pair<TrackingTarget*, gpuFrameStruct>(&t, s));
	}

	opticalFlowTracker = cuda::SparsePyrLKOpticalFlow::create(
		Size(15, 15),
		5,
		30
	);

	// Run a single update cycle to populate out gpuMap with frames
	Update();

	initialized = true;
}

void StateTrackSetPyrLK::Update()
{
	if (!playing)
		return;

	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();
	window->UpdateTrackbar();
	
	cuda::cvtColor(*window->GetInFrame(), gpuFrameNew, COLOR_BGRA2GRAY);

	for (auto& kv : gpuMap)
	{
		opticalFlowTracker->calc(
			gpuFrameOld,
			gpuFrameNew,
			kv.second.gpuPointsOld,
			kv.second.gpuPointsNew,
			kv.second.gpuPointsStatus
		);

		cuda::reduce(kv.second.gpuPointsNew, kv.second.gpuReduced, 1, REDUCE_AVG);

		vector<uchar> status(kv.second.gpuPointsStatus.cols);
		kv.second.gpuPointsStatus.download(status);
		if (find(status.begin(), status.end(), 0) == status.end())
		{
			kv.second.gpuPointsOld = kv.second.gpuPointsNew.clone();
			continue;
		}

		
		vector<Point2f> pointsNew(kv.second.gpuPointsNew.cols);
		kv.second.gpuPointsNew.download(pointsNew);
		vector<Point2f> pointsOld(kv.second.gpuPointsOld.cols);
		kv.second.gpuPointsOld.download(pointsOld);

		vector<Point2f> trackPointsNew;
		for (int p = 0; p < pointsOld.size(); p++)
		{
			auto it = find_if(
				kv.second.pointStatus.begin(),
				kv.second.pointStatus.end(),
				[p](PointStatus& ps) { return ps.cudaIndex == p; }
			);

			assert(it != kv.second.pointStatus.end());

			if (status[p] == 1)
			{
				it->cudaIndexNew = trackPointsNew.size();
				trackPointsNew.push_back(pointsNew[p]);
			}
			else
			{
				it->cudaIndexNew = -1;
				it->cudaIndex = -1;
				it->status = false;
				it->lastPosition = pointsOld[p];
			}
		}

		if (trackPointsNew.size() == 0)
		{
			window->ReplaceState(new StateTrackResult(window, "All points failed for: " + kv.first->GetName()));
			return;
		}

		kv.second.gpuPointsOld.upload(trackPointsNew);
		for (auto& ps : kv.second.pointStatus)
		{
			if (ps.cudaIndexNew != -1)
			{
				ps.cudaIndex = ps.cudaIndexNew;
				ps.cudaIndexNew = -1;
			}
		}
	}

	frameSkip--;
	if (frameSkip <= 0)
	{
		window->DrawWindow();

		frameSkip = 2;
	}

	gpuFrameOld = gpuFrameNew.clone();
}

void StateTrackSetPyrLK::HandleInput(char c)
{
	if (c == ' ')
	{
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}
}

void StateTrackSetPyrLK::AddGui(Mat& frame)
{
	if (!initialized)
		return;

	putText(frame, "Press q to stop tracking", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if(playing)
		putText(frame, "Press space to pause", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	for (auto& kv : gpuMap)
	{
		if (kv.second.gpuPointsNew.empty())
			continue;

		drawGpuPoints(kv.second.gpuPointsNew, frame, Scalar(0, 255, 0));
		for (auto& ps : kv.second.pointStatus)
		{
			if (ps.status)
				continue;

			circle(frame, ps.lastPosition, 5, Scalar(0, 0, 255), 4);
		}
	}

	TrackingTarget* m = set->GetTarget(TrackingTarget::TYPE_MALE);
	TrackingTarget* f = set->GetTarget(TrackingTarget::TYPE_FEMALE);

	float targetDistance = 0;

	if (m && f)
	{
		auto gpuM = gpuMap.at(m).gpuReduced;
		auto gpuF = gpuMap.at(f).gpuReduced;

		cuda::GpuMat gpuCenters(
			1,
			2,
			gpuM.type()
		);
		gpuM.copyTo(gpuCenters(Rect(0, 0, gpuM.cols, gpuM.rows)));
		gpuF.copyTo(gpuCenters(Rect(1, 0, gpuF.cols, gpuF.rows)));

		vector<Point2f> centers;
		gpuCenters.download(centers);

		line(frame, centers.at(0), centers.at(1), Scalar(255, 255, 255), 2);

		cuda::GpuMat gpuLineCenter;
		cuda::reduce(gpuCenters, gpuLineCenter, 1, REDUCE_AVG);
		vector<Point2f> cvCenter;
		gpuLineCenter.download(cvCenter);
		Point2f mid = cvCenter.at(0);
			
		targetDistance = (float)norm(centers.at(0) - centers.at(1));
		putText(frame, format("%i", (int)targetDistance), mid, FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	}

	TrackingTarget* b = set->GetTarget(TrackingTarget::TYPE_BACKGROUND);
	if (b)
	{
		auto gpuMat = gpuMap.at(b).gpuPointsNew;
			
		vector<Point2f> contourPoints;
		gpuMat.download(contourPoints);

		Point2f c;
		float r;
		minEnclosingCircle(contourPoints, c, r);
		circle(frame, c, r, Scalar(0, 255, 0));

		putText(frame, format("%.1f", r), c, FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

		if (startDistance == 0)
		{
			startDistance = r;
		}
		else if (targetDistance != 0)
		{
			float zoomFactor = startDistance / r;
			targetDistance *= zoomFactor;
		}
	}

	if (targetDistance > 0)
	{
		if (targetDistanceMin == 0)
			targetDistanceMin = targetDistance;
		else
			targetDistanceMin = min(targetDistanceMin, targetDistance);

		if (targetDistanceMax == 0)
			targetDistanceMax = targetDistance;
		else
			targetDistanceMax = max(targetDistanceMax, targetDistance);

		if (targetDistanceMin > 0 && targetDistanceMax > 0 && targetDistanceMin != targetDistanceMax)
		{
			position = mapValue<int, float>(targetDistance, targetDistanceMin, targetDistanceMax, 0, 1);

			int barHeight = 300;

			int barBottom = frame.size().height - barHeight - 30;
			rectangle(
				frame,
				Rect(30, barBottom, 30, barHeight),
				Scalar(0, 0, 255),
				2
			);

			float y = mapValue<float, int>(position, 0, 1, barHeight, 0);
			rectangle(
				frame,
				Rect(30, barBottom - y + barHeight, 30, y),
				Scalar(0, 255, 0),
				-1
			);
		}
	}
}

// StateTrackSetTracker

Rect StateTrackSetTracker::OffsetRect(Rect r, int width)
{
	auto f = window->GetInFrame();

	int x1 = max(0, r.x - width);
	int y1 = max(0, r.y - width);
	int x2 = min(f->cols, r.x + r.width + width);
	int y2 = min(f->rows, r.y + r.height + width);

	return Rect(
		Point(x1, y1),
		Point(x2, y2)
	);
}

void StateTrackSetTracker::EnterState()
{
	window->SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2BGR);
	
	Mat m;
	grayScale.download(m);
	
	for (auto& t : set->targets)
	{
		trackerStateStruct s;
		s.lastMove = Point(0, 0);

		//s.tracker = cv::legacy::upgradeTrackingAPI(legacy::TrackerMedianFlow::create());
		//s.tracker = TrackerCSRT::create();
		
		//TrackerDaSiamRPN::Params p;
		//p.backend = dnn::DNN_BACKEND_CUDA;
		//p.target = dnn::DNN_TARGET_CUDA;
		//s.tracker = TrackerDaSiamRPN::create(p);
		
		//s.tracker = TrackerKCF::create();
		//s.tracker = TrackerMIL::create();
		s.tracker = new GpuTrackerGOTURN();

		if (trackingRange != 0)
		{
			s.trackingWindow = OffsetRect(t.initialRect, trackingRange);
			
			s.initialRect = Rect(
				Point(t.initialRect.x - s.trackingWindow.x, t.initialRect.y - s.trackingWindow.y),
				Size(t.initialRect.width, t.initialRect.height)
			);

			s.tracker->init(m(s.trackingWindow), s.initialRect);
		}
		else
		{
			s.initialRect = t.initialRect;
			s.tracker->init(m, s.initialRect);
		}

		trackerStates.insert(pair<TrackingTarget*, trackerStateStruct>(&t, s));
	}

	window->SetPlaying(true);
}

void StateTrackSetTracker::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackSetTracker::HandleInput(char c)
{
	if (c == ' ')
	{
		skipper = 0;
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}
}

void StateTrackSetTracker::AddGui(Mat& frame)
{
	putText(frame, "Press q to stop tracking", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (playing)
		putText(frame, "Press space to pause", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	for (auto& kv : trackerStates)
	{
		if (trackingRange != 0)
		{
			rectangle(frame, kv.second.trackingWindow, Scalar(0, 255, 255), 2);
			rectangle(
				frame,
				Rect(
					Point(
						kv.second.trackingWindow.x + kv.second.initialRect.x,
						kv.second.trackingWindow.y + kv.second.initialRect.y
					),
					Size(
						kv.second.initialRect.width,
						kv.second.initialRect.height
					)
				),
				Scalar(0, 255, 0),
				2
			);
		}
		else
		{
			rectangle(frame, kv.second.initialRect, Scalar(0, 255, 255), 2);
		}
	}
}

void StateTrackSetTracker::Update()
{
	if (!playing)
		return;

	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();
	
	
	if (skipper > 0)
	{
		skipper--;
		//return;
	}
	else
	{
		skipper = 2;
	}
	
	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2BGR);
	//Mat m(grayScale);

	for (auto& kv : trackerStates)
	{
		
		if (trackingRange != 0)
		{
			Rect oldPos = kv.second.initialRect;
			kv.second.tracker->update(grayScale(kv.second.trackingWindow), kv.second.initialRect);
			
			kv.second.lastMove = Point(
				(kv.second.initialRect.x + (kv.second.initialRect.width / 2)) - (oldPos.x + (oldPos.width / 2)),
				(kv.second.initialRect.y + (kv.second.initialRect.height / 2)) - (oldPos.y + (oldPos.height / 2))
			);

			//kv.second.trackingWindow.x += kv.second.lastMove.x;
			//kv.second.trackingWindow.y += kv.second.lastMove.y;
			//kv.second.rect.x -= kv.second.lastMove.x;
			//kv.second.rect.y -= kv.second.lastMove.y;

		}
		else
		{
			kv.second.tracker->update(grayScale, kv.second.initialRect);
		}
	}

	window->DrawWindow();
}

// StateTrackGoturn

Rect StateTrackGoturn::OffsetRect(Rect r, int width)
{
	auto f = window->GetInFrame();

	int x1 = max(0, r.x - width);
	int y1 = max(0, r.y - width);
	int x2 = min(f->cols, r.x + r.width + width);
	int y2 = min(f->rows, r.y + r.height + width);

	return Rect(
		Point(x1, y1),
		Point(x2, y2)
	);
}

void StateTrackGoturn::EnterState()
{
	window->SelectTrackingSet(set);

	net = dnn::readNetFromCaffe("goturn.prototxt", "goturn.caffemodel");
	CV_Assert(!net.empty());

	net.setPreferableBackend(dnn::DNN_BACKEND_CUDA);
	net.setPreferableTarget(dnn::DNN_TARGET_CUDA);

	cuda::GpuMat greyGpu;
	cuda::cvtColor(*window->GetInFrame(), greyGpu, COLOR_BGRA2BGR);
	lastFrameGpu = greyGpu.clone();

	for (auto& t : set->targets)
	{
		trackerStateStruct s;
		s.initialRect = t.initialRect;

		trackerStates.insert(pair<TrackingTarget*, trackerStateStruct>(&t, s));
	}

	window->SetPlaying(true);
}

void StateTrackGoturn::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackGoturn::HandleInput(char c)
{
	if (c == ' ')
	{
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}
}

void StateTrackGoturn::AddGui(Mat& frame)
{
	putText(frame, "Press q to stop tracking", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (playing)
		putText(frame, "Press space to pause", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	for (auto& kv : trackerStates)
	{
		rectangle(frame, kv.second.trackingWindow, Scalar(0, 255, 255), 2);
		rectangle(frame, kv.second.initialRect, Scalar(0, 255, 0), 2);
	}
}

#include <opencv2/cudawarping.hpp>

void StateTrackGoturn::Update()
{
	if (!playing)
		return;

	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();
	cuda::GpuMat& frameGpu = *window->GetInFrame();
	cuda::GpuMat greyGpu;

	int INPUT_SIZE = 227;
	cuda::cvtColor(frameGpu, greyGpu, COLOR_BGRA2BGR);

	for (auto& kv : trackerStates)
	{
		kv.second.trackingWindow = OffsetRect(kv.second.initialRect, trackingRange);

		cuda::GpuMat resizedOldGpu, resizedNewGpu;

		cuda::resize(lastFrameGpu(kv.second.trackingWindow), resizedOldGpu, Size(INPUT_SIZE, INPUT_SIZE), 0, 0, 0);
		cuda::resize(greyGpu(kv.second.trackingWindow), resizedNewGpu, Size(INPUT_SIZE, INPUT_SIZE), 0, 0, 0);

		Mat resizedOld(resizedOldGpu);
		Mat resizedNew(resizedNewGpu);

		Mat targetBlob = dnn::blobFromImage(resizedOld, 1.0f, Size(), Scalar::all(128), false);
		Mat searchBlob = dnn::blobFromImage(resizedNew, 1.0f, Size(), Scalar::all(128), false);

		net.setInput(targetBlob, "data1");
		net.setInput(searchBlob, "data2");

		Mat resMat = net.forward("scale").reshape(1, 1);
		
		Point p1 = Point(
			kv.second.trackingWindow.x + mapValue<float, int>(resMat.at<float>(0), 0, INPUT_SIZE, 0, kv.second.trackingWindow.width),
			kv.second.trackingWindow.y + mapValue<float, int>(resMat.at<float>(1), 0, INPUT_SIZE, 0, kv.second.trackingWindow.height)
		);

		Point p2 = Point(
			kv.second.trackingWindow.x + mapValue<float, int>(resMat.at<float>(2), 0, INPUT_SIZE, 0, kv.second.trackingWindow.width),
			kv.second.trackingWindow.y + mapValue<float, int>(resMat.at<float>(3), 0, INPUT_SIZE, 0, kv.second.trackingWindow.height)
		);

		kv.second.initialRect = Rect(p1, p2);
	}

	lastFrameGpu = greyGpu.clone();
	window->DrawWindow();
}

// StateTrackGpu

Rect StateTrackGpu::OffsetRect(Rect r, int width)
{
	auto f = window->GetInFrame();

	int x1 = max(0, r.x - width);
	int y1 = max(0, r.y - width);
	int x2 = min(f->cols, r.x + r.width + width);
	int y2 = min(f->rows, r.y + r.height + width);

	return Rect(
		Point(x1, y1),
		Point(x2, y2)
	);
}

void StateTrackGpu::EnterState()
{
	window->SelectTrackingSet(set);

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2BGR);

	for (auto& t : set->targets)
	{
		trackGpuState s;
		s.lastMove = Point(0, 0);

		s.tracker = new GpuTrackerGOTURN();

		if (trackingRange != 0)
		{
			s.trackingWindow = OffsetRect(t.initialRect, trackingRange);

			s.initialRect = Rect(
				Point(t.initialRect.x - s.trackingWindow.x, t.initialRect.y - s.trackingWindow.y),
				Size(t.initialRect.width, t.initialRect.height)
			);

			s.tracker->initGpu(grayScale(s.trackingWindow), s.initialRect);
		}
		else
		{
			s.initialRect = t.initialRect;
			s.tracker->initGpu(grayScale, s.initialRect);
		}

		trackerStates.insert(pair<TrackingTarget*, trackGpuState>(&t, s));
	}

	window->SetPlaying(true);
}

void StateTrackGpu::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackSetTracker::HandleInput(char c)
{
	if (c == ' ')
	{
		skipper = 0;
		playing = !playing;
		window->SetPlaying(playing);
		window->DrawWindow();
	}
}

void StateTrackGpu::AddGui(Mat& frame)
{
	putText(frame, "Press q to stop tracking", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (playing)
		putText(frame, "Press space to pause", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	else
		putText(frame, "Press space to continue", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	putText(frame, format("FPS=%d", drawFps), Point(30, 140), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	for (auto& kv : trackerStates)
	{
		if (trackingRange != 0)
		{
			rectangle(frame, kv.second.trackingWindow, Scalar(0, 255, 255), 2);
			rectangle(
				frame,
				Rect(
					Point(
						kv.second.trackingWindow.x + kv.second.initialRect.x,
						kv.second.trackingWindow.y + kv.second.initialRect.y
					),
					Size(
						kv.second.initialRect.width,
						kv.second.initialRect.height
					)
				),
				Scalar(0, 255, 0),
				2
			);
		}
		else
		{
			rectangle(frame, kv.second.initialRect, Scalar(0, 255, 255), 2);
		}
	}
}

void StateTrackGpu::Update()
{
	if (!playing)
		return;

	// Calc FPS
	auto now = high_resolution_clock::now();
	int diffMs = duration_cast<chrono::milliseconds>(now - timer).count();
	timerFrames++;
	if (diffMs >= 200) {

		float perFrame = (diffMs / timerFrames);
		if (perFrame > 0) {
			drawFps = 1000 / perFrame;
		}

		timerFrames = 0;
		timer = now;
	}

	window->ReadCleanFrame();


	if (skipper > 0)
	{
		skipper--;
		//return;
	}
	else
	{
		skipper = 2;
	}

	cuda::GpuMat* gpuFrame = window->GetInFrame();
	cuda::GpuMat grayScale;
	cuda::cvtColor(*gpuFrame, grayScale, COLOR_BGRA2BGR);

	for (auto& kv : trackerStates)
	{

		if (trackingRange != 0)
		{
			Rect oldPos = kv.second.initialRect;
			kv.second.tracker->update(grayScale(kv.second.trackingWindow), kv.second.initialRect);

			kv.second.lastMove = Point(
				(kv.second.initialRect.x + (kv.second.initialRect.width / 2)) - (oldPos.x + (oldPos.width / 2)),
				(kv.second.initialRect.y + (kv.second.initialRect.height / 2)) - (oldPos.y + (oldPos.height / 2))
			);

			//kv.second.trackingWindow.x += kv.second.lastMove.x;
			//kv.second.trackingWindow.y += kv.second.lastMove.y;
			//kv.second.rect.x -= kv.second.lastMove.x;
			//kv.second.rect.y -= kv.second.lastMove.y;

		}
		else
		{
			kv.second.tracker->update(grayScale, kv.second.initialRect);
		}
	}

	window->DrawWindow();
}

// StateTrackResult

void StateTrackResult::AddGui(Mat& frame)
{
	putText(frame, "Tracking result: " + result, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press q to return to edit", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}