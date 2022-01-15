#include "TrackingSet.h"
#include "TrackingEvent.h"

#include <thread>
#include <magic_enum.hpp>

using namespace cv;
using namespace std;

TrackingSetPtr TrackingSet::Unserialize(json& s)
{
	TrackingSetPtr set = make_shared<TrackingSet>(s["time_start"], s["time_end"]);

	if (s.contains("tracking_mode"))
	{
		auto mode = magic_enum::enum_cast<TrackingMode>((string)s["tracking_mode"]);
		if (mode.has_value())
			set->trackingMode = mode.value();
	}

	if (s["events"].is_array())
	{
		set->events = EventList::Unserialize(s["events"]);
	}

	for (auto& t : s["targets"])
	{
		set->targets.push_back(TrackingTarget::Unserialize(t));
		set->targets.back().UpdateColor(set->targets.size());
	}

	return set;
}

void TrackingSet::Serialize(json& set)
{
	set["time_start"] = timeStart;
	set["time_end"] = timeEnd;
	set["targets"] = json::array();
	set["tracking_mode"] = magic_enum::enum_name(trackingMode);

	events->Serialize(set["events"]);

	for (auto& t : targets)
	{
		json& target = set["targets"][set["targets"].size()];
		t.Serialize(target);
	}
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
