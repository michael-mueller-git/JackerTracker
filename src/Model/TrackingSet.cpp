#include "TrackingSet.h"
#include "TrackingEvent.h"

#include <magic_enum.hpp>

using namespace cv;
using namespace std;

TrackingSet TrackingSet::Unserialize(json& s)
{
	TrackingSet set(s["time_start"], s["time_end"]);

	if (s.contains("tracking_mode"))
	{
		auto mode = magic_enum::enum_cast<TrackingMode>((string)s["tracking_mode"]);
		if (mode.has_value())
			set.trackingMode = mode.value();
	}

	if (s["events"].is_array())
	{
		for (auto& r : s["events"])
		{
			set.events.emplace_back(TrackingEvent::Unserialize(r));
		}
	}


	for (auto& t : s["targets"])
	{
		set.targets.push_back(TrackingTarget::Unserialize(t));
		set.targets.back().UpdateColor(set.targets.size());
	}

	return set;
}

void TrackingSet::Serialize(json& set)
{
	set["time_start"] = timeStart;
	set["time_end"] = timeEnd;
	set["targets"] = json::array();
	set["tracking_mode"] = magic_enum::enum_name(trackingMode);

	set["events"] = json::array();
	for (auto& e : events)
	{
		if (e.type == EventType::TET_STATE)
			continue;

		json& result = set["events"][set["events"].size()];
		e.Serialize(result);
	}

	for (auto& t : targets)
	{
		json& target = set["targets"][set["targets"].size()];
		t.Serialize(target);
	}
}

TrackingEvent* TrackingSet::GetResult(time_t time, EventType type)
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