#include "Main.h"
#include "TrackingWindow.h"

/*
		if (s == STATE_EDIT_SET)
		{
			lastState == STATE_PAUSED;

			assert(selectedSet != nullptr, "Set must be selected");
		}

		if (s == STATE_ADD_TRACKING_TARGET)
		{
			if (lastState == STATE_DRAW_ROI)
			{
				Point pMin(
					min(p1.x, p2.x),
					min(p1.y, p2.y)
				);

				Point pMax(
					max(p1.x, p2.x),
					max(p1.y, p2.y)
				);

				Rect roi = Rect(pMin, pMax);

				cuda::GpuMat gpuFrame(outFrame);
				cuda::GpuMat gpuFrameGray(outFrame);
				cuda::GpuMat gpuFramePoints(outFrame);
				cuda::cvtColor(gpuFrame, gpuFrameGray, COLOR_BGR2GRAY);

				Mat mask = Mat::zeros(gpuFrameGray.size(), gpuFrameGray.type());
				rectangle(mask, roi, Scalar(255, 255, 255), -1);
				cuda::GpuMat gpuMask(mask);

				Ptr<cuda::CornersDetector> detector = cuda::createGoodFeaturesToTrackDetector(
					gpuFrameGray.type(),
					200,
					0.03,
					50
				);
				detector->detect(gpuFrameGray, gpuFramePoints, gpuMask);
				gpuFramePoints.download(points);
			}
			else {
				SetState(STATE_DRAW_ROI);
			}
		}

		int w = cap.get(CAP_PROP_FRAME_WIDTH);

		rectangle(
			outFrame,
			Rect(20, 20, w - 40, 20),
			Scalar(255, 0, 0),
			2
		);

		int xNow = mapValue(CurrentPosition(), 0, GetDuration(), 20, w - 40);
		line(outFrame, Point(xNow, 20), Point(xNow, 40), Scalar(255, 0, 0), 4);

		for (auto& set : sets)
		{
			Scalar c(255, 0, 0);
			if (&set == selectedSet)
				c = Scalar(100, 0, 100);

			int xStart = mapValue(set.timeStart, 0, GetDuration(), 20, w - 40);
			line(outFrame, Point(xStart, 20), Point(xStart, 40), c, 2);
		}

		outFrameUpdate = true;

*/

void TrackingTarget::Draw(Mat& frame)
{
	if (trackingType == TrackingTarget::TYPE_POINTS)
	{
		for (auto& p : points)
		{
			circle(frame, p, 4, Scalar(0, 0, 255), 3);
		}
		return;
	}

	if (trackingType == TrackingTarget::TYPE_RECT)
	{
		rectangle(frame, rect, Scalar(0, 0, 255), 3);
	}
}

string TrackingTarget::GetName()
{
	string ret = "";

	switch (targetType)
	{
	case TrackingTarget::TYPE_MALE:
		ret += "Male";
		break;
	case TrackingTarget::TYPE_FEMALE:
		ret += "Female";
		break;
	case TrackingTarget::TYPE_BACKGROUND:
		ret += "Background";
		break;
	default:
		ret += "Unknown";
		break;
	}

	switch (trackingType)
	{
	case TrackingTarget::TYPE_POINTS:
		ret += " points";
	case TrackingTarget::TYPE_RECT:
		ret += " rect";
	}

	return ret;
}

void TrackingSet::Draw(Mat& frame)
{
	for (auto& s : targets)
	{
		s.Draw(frame);
	}
}

TrackingTarget* TrackingSet::GetTarget(TrackingTarget::TARGET_TYPE type)
{
	auto it = find_if(targets.begin(), targets.end(), [type](TrackingTarget& t) { return t.targetType == type; });
	if (it == targets.end())
		return nullptr;

	return &(*it);
}

long mapValue(long x, long in_min, long in_max, long out_min, long out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void drawGpuPoints(cuda::GpuMat& in, Mat& out, Scalar c)
{
	vector<Point2f> points(in.cols);
	in.download(points);
	for(auto & p : points)
	{
		circle(out, p, 5, c, 4);
	}
}

std::string getImageType(int number)
{
	// find type
	int imgTypeInt = number % 8;
	std::string imgTypeString;

	switch (imgTypeInt)
	{
	case 0:
		imgTypeString = "8U";
		break;
	case 1:
		imgTypeString = "8S";
		break;
	case 2:
		imgTypeString = "16U";
		break;
	case 3:
		imgTypeString = "16S";
		break;
	case 4:
		imgTypeString = "32S";
		break;
	case 5:
		imgTypeString = "32F";
		break;
	case 6:
		imgTypeString = "64F";
		break;
	default:
		break;
	}

	// find channel
	int channel = (number / 8) + 1;

	std::stringstream type;
	type << "CV_" << imgTypeString << "C" << channel;

	return type.str();
}

int main()
{
	cout << "Starting" << endl;

	const string fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\DIRTY TALKING SLUT RAM PLAYS WITH SEX MACHINE FANTASIZING ABOUT SUBARU_Yukki Amey_1080p.mp4";
	TrackingWindow win(
		fName,
		30 * 1000
	);
	win.Run();
	
	cout << "Done" << endl;
	return 0;
}