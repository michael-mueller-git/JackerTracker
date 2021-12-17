#include "Main.h"
#include "TrackingWindow.h"
#include "VideoSource.h"

TrackingTarget::TrackingState& TrackingTarget::InitTracking()
{
	trackingState.active = true;
	trackingState.points.clear();
	trackingState.currentRect = Rect(0, 0, 0, 0);

	if (trackingType == TrackingTarget::TYPE_POINTS)
	{
		for (auto& p : intialPoints)
		{
			TrackingTarget::PointState ps;
			ps.active = true;
			ps.point = p;
			trackingState.points.push_back(ps);
		}
	}

	if (trackingType == TrackingTarget::TYPE_RECT)
	{
		trackingState.currentRect = initialRect;
	}

	return trackingState;
}

void TrackingTarget::Draw(Mat& frame)
{
	if (trackingType == TrackingTarget::TYPE_POINTS)
	{
		for (auto& p : intialPoints)
		{
			circle(frame, p, 4, color, 3);
		}
		return;
	}

	if (trackingType == TrackingTarget::TYPE_RECT)
	{
		rectangle(frame, initialRect, color, 3);
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

void drawGpuPoints(cuda::GpuMat& in, Mat& out, Scalar c)
{
	vector<Point2f> intialPoints(in.cols);
	in.download(intialPoints);
	for(auto & p : intialPoints)
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

#include "opencv2/cudacodec.hpp"

int main()
{
	cout << "Starting" << endl;

	const String fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\DIRTY TALKING SLUT RAM PLAYS WITH SEX MACHINE FANTASIZING ABOUT SUBARU_Yukki Amey_1080p.mp4";
	//const String fName = "C:\\dev\\opencvtest\\build\\Latex_Nurses_Scene_2.mp4";
	//const String fName = "H:\\P\\Vr\\Script\\Videos\\Fuck\\Best Throat Bulge Deepthroat Ever. I gave my Hubby ASIAN ESCORT as a gift.mp4";
	
	TrackingWindow win(
		fName,
		30 * 1000
	);
	win.Run();
	
	cout << "Done" << endl;
	return 0;
}