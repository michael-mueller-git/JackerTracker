#pragma once

#include "Model/Model.h"

#include <string>
#include <chrono>

template<typename I, typename O>
O mapValue(I x, I in_min, I in_max, O out_min, O out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

std::string TargetTypeToString(TARGET_TYPE t);
std::string TrackingModeToString(TrackingMode m);
cv::Scalar PickColor(int index);