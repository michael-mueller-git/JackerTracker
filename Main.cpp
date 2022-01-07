#include "Main.h"
#include "TrackingWindow.h"

#include <filesystem>
#include <fstream>
#include <cstring>

// TrackingCalculator

TrackingCalculator::TrackingCalculator()
{

}

bool TrackingCalculator::Update(TrackingSet& set, time_t t)
{
	TrackingTarget* maleTarget = set.GetTarget(TARGET_TYPE::TYPE_MALE);
	TrackingTarget* femaleTarget = set.GetTarget(TARGET_TYPE::TYPE_FEMALE);
	if (maleTarget == nullptr || femaleTarget == nullptr)
		return false;

	if (maleTarget->results.count(t) == 0 || femaleTarget->results.count(t) == 0)
		return true;

	malePoint = maleTarget->results.at(t);
	femalePoint = femaleTarget->results.at(t);

	float distance = (float)norm(malePoint - femalePoint);
	
	if (distanceMin == 0)
		distanceMin = distance;
	else
		distanceMin = min(distanceMin, distance);

	if (distanceMax == 0)
		distanceMax = distance;
	else
		distanceMax = max(distanceMax, distance);

	float newPosition = mapValue<int, float>(distance, distanceMin, distanceMax, 0, 1);
	float d = position - newPosition;

	if (d < 0 && up)
	{
		up = false;
		events.emplace_back(TrackingEvent::TET_POSITION, t);
		events.back().position = position;
	}
	else if (d > 0 && !up)
	{
		up = true;

		events.emplace_back(TrackingEvent::TET_POSITION, t);
		events.back().position = position;
	}

	position = newPosition;
}

void TrackingCalculator::Draw(Mat& frame, time_t t)
{
	line(frame, malePoint, femalePoint, Scalar(255, 255, 255), 2);

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

	std::vector<TrackingEvent> eventsFiltered;
	time_t timeFrom = max(0LL, t - 2000);
	time_t timeTo = t + 2000;

	copy_if(
		events.begin(),
		events.end(),
		back_inserter(eventsFiltered),
		[timeFrom, timeTo](auto& e) {
			return e.time >= timeFrom && e.time <= timeTo;
		}
	);

	vector<int> xData;
	vector<float> yData;

	int xCenter = frame.cols / 2;

	Point lastPoint(-1, -1);
	for (auto& e : eventsFiltered)
	{
		if (e.type != TrackingEvent::TET_POSITION)
			continue;

		int x = mapValue<int, int>(e.time - timeFrom, 0, 4000, 0, frame.cols);
		int y = mapValue<float, int>(e.position, 0, 1, frame.rows - 300, frame.rows);
		Point p(x, y);

		if (lastPoint.x != -1)
		{
			line(frame, p, lastPoint, Scalar(255, 0, 0), 2);
		}

		lastPoint = p;
	}

	
}

// TrackingState

TrackingTarget::TrackingState* TrackingTarget::InitTracking()
{
	TrackingState* trackingState = new TrackingState(*this);

	trackingState->active = true;
	trackingState->points.clear();
	trackingState->rect = Rect(0, 0, 0, 0);

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		for (auto& p : intialPoints)
		{
			TrackingTarget::PointState ps;
			ps.active = true;
			ps.point = p;
			trackingState->points.push_back(ps);
		}
	}

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
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

void TrackingTarget::TrackingState::SnapResult(track_time time)
{
	parent.results.emplace(pair<time_t, Point2f>(time, center));
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
	if (targets.size() == 0)
		return nullptr;

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
			sets.emplace_back(s["time_start"], s["time_end"]);
			TrackingSet& set = sets.back();
			
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

				if (t["results"].is_array())
				{
					for (auto& r : t["results"])
					{
						target.results.emplace(pair<time_t, Point2f>(
							r["time"],
							Point2f(
								r["x"],
								r["y"]
							)
						));
					}
				}

				set.targets.push_back(target);
			}
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

			target["results"] = json::array();
			for (auto& r : t.results)
			{
				json& result = target["results"][target["results"].size()];
				result["time"] = r.first;
				result["x"] = r.second.x;
				result["y"] = r.second.y;
			}
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