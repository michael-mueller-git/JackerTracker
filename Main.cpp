#include "Main.h"
#include "TrackingWindow.h"

#include <filesystem>
#include <fstream>
#include <cstring>
#include "magic_enum.hpp"

Scalar myColors[] = {
	{197, 100, 195},
	{90, 56, 200},
	{67, 163, 255},
	{143, 176, 59},
	{206, 29, 255},
	{133, 108, 252},
	{166, 252, 153},
	{120, 128, 21},
	{93, 236, 178},
	{196, 108, 43},
	{74, 110, 255},
	{20, 249, 29},
	{208, 173, 162}
};

Scalar PickColor(int index)
{
	if (index > sizeof(myColors))
		index = index % sizeof(myColors);

	return myColors[index];
}

string TargetTypeToString(TARGET_TYPE t)
{
	switch (t) {
	case TYPE_MALE:
		return "Male";
	case TYPE_FEMALE:
		return "Female";
	case TYPE_BACKGROUND:
		return "Background";
	default:
		return "Unknown";
	}
}

// TrackingEvent

TrackingEvent TrackingEvent::Unserialize(json& j)
{
	auto jType = magic_enum::enum_cast<Type>((string)j["type"]);
	if (!jType.has_value())
		throw "No type";

	TrackingEvent e(jType.value(), j["time"]);
	if (j["point"].is_object())
		e.point = Point2f(j["point"]["x"], j["point"]["y"]);
	if (j["rect"].is_object())
		e.rect = Rect(j["rect"]["x"], j["rect"]["y"], j["rect"]["width"], j["rect"]["height"]);

	if (j["points"].is_array())
	{
		e.points.reserve(j["points"].size());
		for (auto& p : j["points"])
		{
			PointState ps;
			ps.point = Point2f(p["x"], p["y"]);
			ps.active = p["active"];
			e.points.push_back(ps);
		}
	}


	e.size = j["size"];
	e.position = j["position"];
	e.minPosition = j["min_position"];
	e.maxPosition = j["max_position"];
	e.targetGuid = j["target"];

	return e;
}

void TrackingEvent::Serialize(json& j)
{
	j["type"] = magic_enum::enum_name(type);
	j["time"] = time;
	j["point"]["x"] = point.x;
	j["point"]["y"] = point.y;
	j["size"] = size;
	j["position"] = position;
	j["min_position"] = minPosition;
	j["max_position"] = maxPosition;
	j["target"] = targetGuid;

	j["rect"]["x"] = rect.x;
	j["rect"]["y"] = rect.y;
	j["rect"]["width"] = rect.width;
	j["rect"]["height"] = rect.height;
	j["points"] = json::array();

	for (auto& p : points)
	{
		auto& pj = j["points"][points.size()];
		pj["x"] = p.point.x;
		pj["y"] = p.point.y;
		pj["active"] = p.active;
	}
}

// TrackingCalculator

TrackingCalculator::TrackingCalculator()
{

}

bool TrackingCalculator::Update(TrackingSet& set, time_t t)
{
	TrackingTarget* backgroundTarget = set.GetTarget(TARGET_TYPE::TYPE_BACKGROUND);
	if (backgroundTarget)
	{
		auto backgroundResult = backgroundTarget->GetResult(set, t, TrackingEvent::Type::TET_SIZE);
		if (backgroundResult)
		{
			if (sizeStart == 0)
			{
				sizeStart = backgroundResult->size;
			}
			else
			{
				float newScale = sizeStart / backgroundResult->size;
				if (scale != newScale)
				{
					scale = newScale;
					set.events.emplace_back(TrackingEvent::TET_SCALE, t);
					set.events.back().size = scale;
					set.events.back().targetGuid = backgroundTarget->GetGuid();
				}
				
			}
		}
	}

	TrackingTarget* maleTarget = set.GetTarget(TARGET_TYPE::TYPE_MALE);
	TrackingTarget* femaleTarget = set.GetTarget(TARGET_TYPE::TYPE_FEMALE);
	if (maleTarget == nullptr || femaleTarget == nullptr)
	{
		lockedOn = false;
		return false;
	}

	auto maleResult = maleTarget->GetResult(set, t, TrackingEvent::Type::TET_POINT);
	auto femaleResult = femaleTarget->GetResult(set, t, TrackingEvent::Type::TET_POINT);
	if (!maleResult || !femaleResult)
	{
		lockedOn = false;
		return false;
	}

	malePoint = maleResult->point;
	femalePoint = femaleResult->point;

	float distance = (float)norm(malePoint - femalePoint) * scale;
	
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
		set.events.emplace_back(TrackingEvent::TET_POSITION, t);
		set.events.back().position = position;
	}
	else if (d > 0 && !up)
	{
		up = true;

		set.events.emplace_back(TrackingEvent::TET_POSITION, t);
		set.events.back().position = position;
	}

	position = newPosition;
	lockedOn = true;
}

void TrackingCalculator::Draw(TrackingSet& set, Mat& frame, time_t t, bool livePosition, bool drawState)
{
	if(lockedOn)
		line(frame, malePoint, femalePoint, Scalar(255, 255, 255), 2);

	int barHeight = 300;

	int barBottom = frame.size().height - barHeight - 30;
	rectangle(
		frame,
		Rect(30, barBottom, 30, barHeight),
		Scalar(0, 0, 255),
		2
	);

	std::vector<TrackingEvent> eventsFiltered;

	time_t timeFrom = max(0LL, t - 10000);
	time_t timeTo = t + 10000;

	copy_if(
		set.events.begin(),
		set.events.end(),
		back_inserter(eventsFiltered),
		[timeFrom, timeTo](auto& e) {
			return e.time >= timeFrom && e.time <= timeTo;
		}
	);

	TrackingEvent* e1 = nullptr;
	TrackingEvent* e2 = nullptr;

	if (!livePosition)
	{
		for (int i = 0; i < eventsFiltered.size(); i++)
		{
			auto& e = eventsFiltered.at(i);

			if (e.type != TrackingEvent::TET_POSITION)
				continue;

			if (e.time > t)
			{
				if (!e1)
					break;

				e2 = &e;
				break;
			}
			else
			{
				e1 = &e;
			}
		}
		
		if (e1 && e2)
		{
			position = mapValue<time_t, float>(t, e1->time, e2->time, e1->position, e2->position);
		}
		else
			position = 0;
	}

	int y = mapValue<float, int>(position, 0, 1, barHeight, 0);
	rectangle(
		frame,
		Rect(30, barBottom - y + barHeight, 30, y),
		Scalar(0, 255, 0),
		-1
	);

	int x = mapValue<time_t, int>(t, timeFrom, timeTo, 0, frame.cols);
	line(frame, Point(x, frame.rows - 150), Point(x, frame.rows), Scalar(0, 0, 0), 2);

	int graphTop = frame.rows - 150;
	int graphBottom = frame.rows;

	line(frame, Point(0, graphTop), Point(frame.cols, graphTop), Scalar(0, 0, 0), 1);
	line(frame, Point(0, graphBottom), Point(frame.cols, graphBottom), Scalar(0, 0, 0), 1);

	Point lastPointPosition(-1, -1);
	int stateIndex = 0;
	time_t stateTime = 0;

	for (auto& e : eventsFiltered)
	{
		if (e.type == TrackingEvent::TET_POSITION)
		{

			x = mapValue<int, int>(e.time - timeFrom, 0, timeTo - timeFrom, 0, frame.cols);
			y = mapValue<float, int>(e.position, 0, 1, graphTop, graphBottom);
			Point p(x, y);
			
			if (lastPointPosition.x != -1)
			{
				line(frame, p, lastPointPosition, Scalar(255, 0, 0), 2);
			}

			circle(frame, p, 3, Scalar(255, 255, 255), 2);

			lastPointPosition = p;
		}

		if (drawState)
		{
			if (e.type == TrackingEvent::TET_STATE)
			{
				if (stateTime == 0)
				{
					if (e.time >= t)
						stateTime = e.time;
				}

				if (stateTime != 0 && e.time == stateTime)
				{
					e.state.UpdateColor(stateIndex);
					e.state.Draw(frame);
					stateIndex++;
				}
			}
		}
	}
}

// TrackingState

TrackingState* TrackingTarget::InitTracking(TRACKING_TYPE t)
{
	TrackingState* trackingState = new TrackingState(*this, t);

	trackingState->active = true;
	trackingState->points.clear();
	trackingState->rect = Rect(0, 0, 0, 0);

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		for (auto& p : intialPoints)
		{
			PointState ps;
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

void TrackingStateBase::Draw(Mat& frame)
{
	circle(frame, center, 4, Scalar(0, 0, 0), 3);
	
	if (targetType == TARGET_TYPE::TYPE_BACKGROUND && size != 0)
	{
		circle(frame, center, size / 2, color, 2);
	}

	if (trackingType == TRACKING_TYPE::TYPE_POINTS)
	{
		for (auto& p : points)
		{
			if (p.active)
				circle(frame, p.point, 4, color, 3);
			else
				circle(frame, p.point, 4, Scalar(255, 0, 0), 3);
		}
	}

	if (trackingType == TRACKING_TYPE::TYPE_RECT)
	{
		if (active)
			rectangle(frame, rect, color, 3);
		else
			rectangle(frame, rect, Scalar(255, 0, 0), 3);
	}
}

void TrackingState::SnapResult(TrackingSet& set, track_time time)
{
	TrackingEvent e(TrackingEvent::TET_POINT, time);
	e.point = center;
	e.targetGuid = parent.GetGuid();
	set.events.push_back(e);

	if (size != 0)
	{
		TrackingEvent e(TrackingEvent::TET_SIZE, time);
		e.size = size;
		e.targetGuid = parent.GetGuid();
		set.events.push_back(e);
	}
	
	TrackingEvent e2(TrackingEvent::TET_STATE, time);
	TrackingStateBase& state = *reinterpret_cast<TrackingStateBase*>(this);
	
	e2.state = state;
	e2.targetGuid = parent.GetGuid();
	set.events.push_back(e2);
}

// TrackingTarget

TrackingEvent* TrackingTarget::GetResult(TrackingSet& set, time_t time, TrackingEvent::Type type)
{
	for (auto& e : set.events)
	{
		if (e.targetGuid != guid || e.time != time)
			continue;

		return &e;
	}

	return nullptr;
}

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
	string ret = TargetTypeToString(targetType);
	
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

TrackingEvent* TrackingSet::GetResult(time_t time, TrackingEvent::Type type)
{
	for (auto& r : events)
	{
		if (!r.targetGuid.empty())
			continue;

		if (r.time > time)
			return nullptr;

		if (r.time == time && r.type == type)
			return &r;
	}

	return nullptr;
}

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

TrackingTarget TrackingTarget::Unserialize(json& t)
{
	if (!t.contains("guid") || !t["guid"].is_string() || t["guid"].size() == 0)
		throw "No guid";

	string g = t["guid"];
	TrackingTarget target(g);

	auto targetType = magic_enum::enum_cast<TARGET_TYPE>((string)t["target_type"]);
	if (targetType.has_value())
		target.targetType = targetType.value();

	auto trackingType = magic_enum::enum_cast<TRACKING_TYPE>((string)t["tracking_type"]);
	if (trackingType.has_value())
		target.trackingType = trackingType.value();

	auto preferredTracker = magic_enum::enum_cast<TrackerJTType>((string)t["preferred_tracker"]);
	if (preferredTracker.has_value())
		target.preferredTracker = preferredTracker.value();

	if (target.SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		for (auto& p : t["points"])
			target.intialPoints.push_back(Point2f(p["x"], p["y"]));
	}
	if (target.SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
	{
		target.initialRect = Rect(
			(float)t["rect"]["x"],
			(float)t["rect"]["y"],
			(float)t["rect"]["width"],
			(float)t["rect"]["height"]
		);
	}

	return target;
}

void TrackingTarget::Serialize(json& target)
{
	target["guid"] = guid;

	target["target_type"] = magic_enum::enum_name(targetType);
	target["tracking_type"] = magic_enum::enum_name(trackingType);
	target["preferred_tracker"] = magic_enum::enum_name(preferredTracker);

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_POINTS))
	{
		target["points"] = json::array();

		for (auto& p : intialPoints)
		{
			json& point = target["points"][target["points"].size()];
			point["x"] = p.x;
			point["y"] = p.y;
		}
	}

	if (SupportsTrackingType(TRACKING_TYPE::TYPE_RECT))
	{
		target["rect"]["x"] = initialRect.x;
		target["rect"]["y"] = initialRect.y;
		target["rect"]["width"] = initialRect.width;
		target["rect"]["height"] = initialRect.height;
	}
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


			if (s["events"].is_array())
			{
				for (auto& r : s["results"])
				{
					set.events.emplace_back(TrackingEvent::Unserialize(r));
				}
			}

			
			for (auto& t : s["targets"])
			{
				set.targets.push_back(TrackingTarget::Unserialize(t));
				set.targets.back().UpdateColor(set.targets.size());
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
	j["actions"] = json::array();

	for (auto& s : sets)
	{
		json& set = j["sets"][j["sets"].size()];
		set["time_start"] = s.timeStart;
		set["time_end"] = s.timeEnd;
		set["targets"] = json::array();

		set["events"] = json::array();
		for (auto& e : s.events)
		{
			if (e.type == TrackingEvent::Type::TET_STATE)
				continue;

			json& result = set["events"][set["events"].size()];
			e.Serialize(result);

			if (e.type == TrackingEvent::Type::TET_POSITION)
			{
				json& a = j["actions"][j["actions"].size()];
				a["at"] = e.time;
				a["pos"] = (int)(e.position * 100);
			}
		}

		for (auto& t : s.targets)
		{
			json& target = set["targets"][set["targets"].size()];
			t.Serialize(target);
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