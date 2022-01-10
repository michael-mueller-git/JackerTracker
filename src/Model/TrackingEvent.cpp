#include "TrackingEvent.h"
#include "TrackingSet.h"

#include <magic_enum.hpp>

using namespace std;
using namespace cv;

TrackingEvent TrackingEvent::Unserialize(json& j)
{
	auto jType = magic_enum::enum_cast<EventType>((string)j["type"]);
	if (!jType.has_value())
		throw "No type";

	TrackingEvent e(jType.value(), j["time"]);
	if (j["point"].is_object())
		e.point = Point(j["point"]["x"], j["point"]["y"]);
	if (j["rect"].is_object())
		e.rect = Rect(j["rect"]["x"], j["rect"]["y"], j["rect"]["width"], j["rect"]["height"]);

	if (j["points"].is_array())
	{
		e.points.reserve(j["points"].size());
		for (auto& p : j["points"])
		{
			PointState ps;
			ps.point = Point(p["x"], p["y"]);
			ps.active = p["active"];
			e.points.push_back(ps);
		}
	}

	if (j.contains("size"))
		e.size = j["size"];

	if(j.contains("position"))
		e.position = j["position"];

	if(j.contains("min_position"))
		e.minPosition = j["min_position"];

	if(j.contains("max_position"))
		e.maxPosition = j["max_position"];

	if(j.contains("target"))
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

TrackingEvent* TrackingTarget::GetResult(TrackingSet& set, time_t time, EventType type)
{
	for (auto& e : set.events)
	{
		if (e.targetGuid != guid || e.time != time)
			continue;

		return &e;
	}

	return nullptr;
}