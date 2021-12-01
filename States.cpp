#include "States.h"
#include "TrackingWindow.h"

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

	window->ReadFrame(true);
	window->UpdateTrackbar();
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
	putText(frame, "Press s to start tracking", Point(30, 160), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, format("Num trackers: %i", set->targets.size()), Point(30, 180), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	int y = 200;
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
		//window->PushState(new StateTrackSetTracker(window, set));
		window->PushState(new StateTrackSetPyrLK(window, set));
		return;
	}

	if (c == 'p')
	{
		window->PushState(new StateAddTracker(window, [me](vector<Point2f> &points){

			me->AddTargetPoints(points);

			},
			// Failed
			[]() {}
		));
	}

	if (c == 'r')
	{
		window->PushState(new StateSelectRoi(window, "Select feature to track", [me](Rect rect) {

			me->AddTargetRect(rect);

			},
			// Failed
				[]() {}
			));
	}

}

void StateEditSet::AddTargetRect(Rect& rect)
{
	TrackingTarget trackingTarget;
	if (set->targets.size() == 0)
		trackingTarget.targetType = TrackingTarget::TYPE_FEMALE;
	else if (set->targets.size() == 1)
		trackingTarget.targetType = TrackingTarget::TYPE_MALE;
	else
		trackingTarget.targetType = TrackingTarget::TYPE_BACKGROUND;

	trackingTarget.trackingType = TrackingTarget::TYPE_RECT;
	trackingTarget.rect = rect;
	set->targets.push_back(trackingTarget);
	window->ReadFrame(false);
}

void StateEditSet::AddTargetPoints(vector<Point2f>& points)
{
	TrackingTarget trackingTarget;
	if (set->targets.size() == 0)
		trackingTarget.targetType = TrackingTarget::TYPE_FEMALE;
	else if (set->targets.size() == 1)
		trackingTarget.targetType = TrackingTarget::TYPE_MALE;
	else
		trackingTarget.targetType = TrackingTarget::TYPE_BACKGROUND;

	trackingTarget.trackingType = TrackingTarget::TYPE_POINTS;
	trackingTarget.points = points;
	set->targets.push_back(trackingTarget);
	window->ReadFrame(false);
}

void StateAddTracker::EnterState()
{
	auto window = this->window;
	auto me = this;

	window->PushState(new StateSelectRoi(window, "Select feature to track", [me](Rect roi){
			me->GetPoints(roi);
		},
		// Failed
		[me](){
			me->Pop();
		}
	));
}

void StateAddTracker::GetPoints(Rect r)
{
	cuda::GpuMat gpuFrame(*window->GetInFrame());
	cuda::GpuMat gpuFrameGray(gpuFrame);
	cuda::cvtColor(gpuFrame, gpuFrameGray, COLOR_BGR2GRAY);

	cuda::GpuMat gpuPoints(gpuFrame);
	Ptr<cuda::CornersDetector> detector = cuda::createGoodFeaturesToTrackDetector(
		gpuFrameGray.type(),
		30,
		0.05,
		30
	);

	Mat mask = Mat::zeros(gpuFrameGray.rows, gpuFrameGray.cols, gpuFrameGray.type());
	rectangle(mask, r, Scalar(255, 255, 255), -1);
	cuda::GpuMat gpuMask(mask);

	detector->detect(gpuFrameGray, gpuPoints, gpuMask);
	if (!gpuPoints.empty())
	{
		vector<Point2f> points;
		gpuPoints.download(points);

		callback(points);
		returned = true;
	}

	Pop();
}

void StateAddTracker::AddGui(Mat& frame)
{
	putText(frame, "Press q to cancel adding the tracker", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}

void StateSelectRoi::HandleMouse(int e, int x, int y, int f)
{
	if (p1.x == -1 && p1.y == -1 && e == EVENT_LBUTTONUP)
	{
		p1.x = x;
		p1.y = y;
		return;
	}

	if (p1.x != -1 && p1.y != -1)
	{
		p2.x = x;
		p2.y = y;

		if (e == EVENT_LBUTTONUP)
		{
			Rect2f r(p1, p2);
			callback(r);
			returned = true;
			Pop();
			return;
		}

		window->ReadFrame(false);
	}
}

void StateSelectRoi::AddGui(Mat& frame)
{
	putText(frame, "Press q to cancel selecting", Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, title, Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

	if (p1.x != -1 && p1.y != -1 && p2.x != -1 && p2.y != -1)
	{
		rectangle(frame, Rect2f(p1, p2), Scalar(255, 0, 0));
	}
}

void StateTrackSetPyrLK::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackSetPyrLK::EnterState()
{
	window->SelectTrackingSet(set);

	cuda::GpuMat gpuFrame(*window->GetInFrame());
	gpuFrameOld = cuda::GpuMat(gpuFrame);
	cuda::cvtColor(gpuFrame, gpuFrameOld, COLOR_BGR2GRAY);

	for (auto& t : set->targets)
	{
		gpuFrameStruct s;
		s.gpuPointsOld = cuda::GpuMat(t.points);

		for (int p=0; p<t.points.size(); p++)
		{
			s.pointStatus.push_back({
				true,
				t.points[p],
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

	cuda::GpuMat gpuFrame(*window->GetOutFrame());
	cuda::cvtColor(gpuFrame, gpuFrameNew, COLOR_BGR2GRAY);

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
	window->DrawWindow();
	gpuFrameOld = gpuFrameNew.clone();
}

void StateTrackSetPyrLK::HandleInput(char c)
{
	if (c == ' ')
		playing = !playing;
}

void StateTrackSetPyrLK::AddGui(Mat& frame)
{
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

		TrackingTarget* m = set->GetTarget(TrackingTarget::TYPE_MALE);
		TrackingTarget* f = set->GetTarget(TrackingTarget::TYPE_FEMALE);
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
			
			double dist = norm(centers.at(0) - centers.at(1));
			putText(frame, format("%i", (int)dist), mid, FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);

		}

		TrackingTarget* b = set->GetTarget(TrackingTarget::TYPE_BACKGROUND);
		if (b)
		{
			auto gpuMat = gpuMap.at(b).gpuPointsNew;
			
			vector<Point2f> contourPoints;
			gpuMat.download(contourPoints);
			
			/*
			vector<Point> contour;
			for (auto& p : contourPoints)
			{
				contour.emplace_back((int)p.x, (int)p.y);
			}

			vector<vector<Point>> contours = { contour };
			drawContours(frame, contours, 0, Scalar(255, 0, 0), 2);
			*/

			/*
			cuda::GpuMat gpuCenter;
			cuda::reduce(gpuMat, gpuCenter, 1, REDUCE_AVG);
			vector<Point2f> cvCenter;
			gpuCenter.download(cvCenter);
			Point2f mid = cvCenter.at(0);
			*/

			Point2f c;
			float d;
			minEnclosingCircle(contourPoints, c, d);

			putText(frame, format("%i", (int)d), c, FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
		}
	}
}

void StateTrackSetTracker::EnterState()
{
	window->SelectTrackingSet(set);
	auto frame = window->GetInFrame();

	for (auto& t : set->targets)
	{
		trackerStateStruct s;
		s.rect = t.rect;
		s.tracker = TrackerGOTURN::create();
		s.tracker->init(*frame, t.rect);

		trackerStates.insert(pair<TrackingTarget*, trackerStateStruct>(&t, s));
	}
}

void StateTrackSetTracker::LeaveState()
{
	window->SelectTrackingSet(set);
}

void StateTrackSetTracker::HandleInput(char c)
{
	if (c == ' ')
		playing = !playing;
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
		rectangle(frame, kv.second.rect, Scalar(0, 255, 0), 2);
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
	window->UpdateTrackbar();

	auto frame = window->GetOutFrame();

	for (auto& kv : trackerStates)
	{
		kv.second.tracker->update(*frame, kv.second.rect);
	}

	window->DrawWindow();
}

void StateTrackResult::AddGui(Mat& frame)
{
	putText(frame, "Tracking result: " + result, Point(30, 100), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
	putText(frame, "Press q to return to edit", Point(30, 120), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
}