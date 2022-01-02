#include "Main.h"
#include "TrackingWindow.h"
#include "VideoSource.h"

#include <filesystem>
#include <fstream>
#include <cstring>

// TrackingState

TrackingTarget::TrackingState* TrackingTarget::InitTracking()
{
	TrackingState* trackingState = new TrackingState(*this);

	trackingState->active = true;
	trackingState->points.clear();
	trackingState->rect = Rect(0, 0, 0, 0);

	if (trackingType == TRACKING_TYPE::TYPE_POINTS)
	{
		for (auto& p : intialPoints)
		{
			TrackingTarget::PointState ps;
			ps.active = true;
			ps.point = p;
			trackingState->points.push_back(ps);
		}
	}

	if (trackingType == TRACKING_TYPE::TYPE_RECT)
	{
		trackingState->rect = initialRect;
	}

	return trackingState;
}

void TrackingTarget::TrackingState::Draw(Mat& frame)
{
	circle(frame, center, 4, Scalar(0, 0, 0), 3);

	if (parent.trackingType == TRACKING_TYPE::TYPE_POINTS || parent.trackingType == TRACKING_TYPE::TYPE_BOTH)
	{
		for (auto& p : points)
		{
			if (p.active)
				circle(frame, p.point, 4, color, 3);
			else
				circle(frame, p.point, 4, Scalar(255, 0, 0), 3);
		}
	}

	if (parent.trackingType == TRACKING_TYPE::TYPE_RECT || parent.trackingType == TRACKING_TYPE::TYPE_BOTH)
	{
		if (active)
			rectangle(frame, rect, color, 3);
		else
			rectangle(frame, rect, Scalar(255, 0, 0), 3);
	}
}

// TrackingTarget

void TrackingTarget::Draw(Mat& frame)
{
	if (trackingType == TRACKING_TYPE::TYPE_POINTS || trackingType == TRACKING_TYPE::TYPE_BOTH)
	{
		for (auto& p : intialPoints)
		{
			circle(frame, p, 4, color, 3);
		}
	}

	if (trackingType == TRACKING_TYPE::TYPE_RECT || trackingType == TRACKING_TYPE::TYPE_BOTH)
	{
		rectangle(frame, initialRect, color, 3);
	}
}

string TrackingTarget::GetName()
{
	string ret = "";

	switch (targetType)
	{
	case TARGET_TYPE::TYPE_MALE:
		ret += "Male";
		break;
	case TARGET_TYPE::TYPE_FEMALE:
		ret += "Female";
		break;
	case TARGET_TYPE::TYPE_BACKGROUND:
		ret += "Background";
		break;
	default:
		ret += "Unknown";
		break;
	}

	switch (trackingType)
	{
	case TRACKING_TYPE::TYPE_POINTS:
		ret += " points";
		break;
	case TRACKING_TYPE::TYPE_RECT:
		ret += " rect";
		break;
	case TRACKING_TYPE::TYPE_BOTH:
		ret += " both";
		break;
	}

	return ret;
}

bool TrackingTarget::SupportsTrackingType(TRACKING_TYPE t)
{
	if (t == TRACKING_TYPE::TYPE_NONE)
		return false;

	if (t == trackingType || trackingType == TRACKING_TYPE::TYPE_BOTH)
		return true;

	return false;
}

void TrackingTarget::UpdateType()
{
	if (!initialRect.empty() && !intialPoints.empty())
		trackingType = TRACKING_TYPE::TYPE_BOTH;
	else if (!initialRect.empty())
		trackingType = TRACKING_TYPE::TYPE_RECT;
	else if (!intialPoints.empty())
		trackingType = TRACKING_TYPE::TYPE_POINTS;
}

// TrackingSet

void TrackingSet::Draw(Mat& frame)
{
	for (auto& s : targets)
	{
		s.Draw(frame);
	}
}

TrackingTarget* TrackingSet::GetTarget(TARGET_TYPE type)
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

Project::Project(string video)
	:video(video)
{
	Load();
}

Project::~Project()
{
	Save();
}


string Project::GetConfigPath()
{
	char binary[MAX_PATH];
	GetModuleFileNameA(NULL, binary, MAX_PATH);

	filesystem::path binaryPath = binary;
	filesystem::path videoPath = video;
	string configFile = binaryPath.parent_path().string() + "\\" + videoPath.filename().replace_extension().string() + ".json";

	return configFile;
}

void Project::Load()
{
	string file = GetConfigPath();
	std::ifstream i(file);
	if (i.fail())
		return;

	json j;
	i >> j;
	Load(j);
}

void Project::Load(json j)
{
	if (j["sets"].is_array() && j["sets"].size() > 0)
	{
		for (auto& s : j["sets"])
		{
			TrackingSet set(s["time_start"], s["time_end"]);
			
			for (auto& t : s["targets"])
			{
				TrackingTarget target;
				if (t["target_type"] == "male")
					target.targetType = TARGET_TYPE::TYPE_MALE;
				else if (t["target_type"] == "female")
					target.targetType = TARGET_TYPE::TYPE_FEMALE;
				else if (t["target_type"] == "background")
					target.targetType = TARGET_TYPE::TYPE_BACKGROUND;
				else
					continue;

				if(t["tracking_type"] == "both")
					target.trackingType = TRACKING_TYPE::TYPE_BOTH;
				else if (t["tracking_type"] == "rect")
					target.trackingType = TRACKING_TYPE::TYPE_RECT;
				else if (t["tracking_type"] == "points")
					target.trackingType = TRACKING_TYPE::TYPE_POINTS;


				if (target.trackingType == TRACKING_TYPE::TYPE_POINTS || target.trackingType == TRACKING_TYPE::TYPE_BOTH)
				{
					for (auto& p : t["points"])
						target.intialPoints.push_back(Point2f(p["x"], p["y"]));
				}
				else if (target.trackingType == TRACKING_TYPE::TYPE_RECT || target.trackingType == TRACKING_TYPE::TYPE_BOTH)
				{
					target.initialRect = Rect(
						(float)t["rect"]["x"],
						(float)t["rect"]["y"],
						(float)t["rect"]["width"],
						(float)t["rect"]["height"]
					);
				}
				else
					continue;

				set.targets.push_back(target);
			}

			sets.push_back(set);
		}
	}
}

void Project::Save()
{
	string file = GetConfigPath();
	std::ofstream o(file);
	if (o.fail())
		return;

	json j;
	Save(j);
	o << std::setw(4) << j << std::endl;
}

void Project::Save(json &j)
{
	j["sets"] = json::array();

	for (auto& s : sets)
	{
		json& set = j["sets"][j["sets"].size()];
		set["time_start"] = s.timeStart;
		set["time_end"] = s.timeEnd;
		set["targets"] = json::array();

		for (auto& t : s.targets)
		{
			json& target = set["targets"][set["targets"].size()];
			if (t.targetType == TARGET_TYPE::TYPE_MALE)
				target["target_type"] = "male";
			else if (t.targetType == TARGET_TYPE::TYPE_FEMALE)
				target["target_type"] = "female";
			else if (t.targetType == TARGET_TYPE::TYPE_BACKGROUND)
				target["target_type"] = "background";
			else
				continue;

			if (t.trackingType == TRACKING_TYPE::TYPE_BOTH) {
				target["tracking_type"] = "both";
			}
			else if (t.trackingType == TRACKING_TYPE::TYPE_RECT) {
				target["tracking_type"] = "rect";
			}
			else if (t.trackingType == TRACKING_TYPE::TYPE_POINTS) {
				target["tracking_type"] = "points";
			}

			if (t.trackingType == TRACKING_TYPE::TYPE_POINTS || t.trackingType == TRACKING_TYPE::TYPE_BOTH)
			{
				target["points"] = json::array();

				for (auto& p : t.intialPoints)
				{
					json& point = target["points"][target["points"].size()];
					point["x"] = p.x;
					point["y"] = p.y;
				}
			}
			else if (t.trackingType == TRACKING_TYPE::TYPE_RECT || t.trackingType == TRACKING_TYPE::TYPE_BOTH)
			{
				target["rect"]["x"] = t.initialRect.x;
				target["rect"]["y"] = t.initialRect.y;
				target["rect"]["width"] = t.initialRect.width;
				target["rect"]["height"] = t.initialRect.height;
			}
			else
				continue;
		}
	}
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

	return 0;
}