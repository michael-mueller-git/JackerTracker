#include "TrackingTarget.h"
#include "TrackingSet.h"

#include <opencv2/imgproc.hpp>
#include <crossguid/guid.hpp>
#include <magic_enum.hpp>

using namespace std;
using namespace cv;

TrackingTarget::TrackingTarget()
{
	guid = xg::newGuid();
}

TrackingTarget::TrackingTarget(string guid)
	:guid(guid)
{
	
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