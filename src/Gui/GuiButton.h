#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <functional>

class GuiButton
{
public:
	GuiButton()
		:textColor(0, 0, 0)
	{

	}

	cv::Rect rect;
	std::string text;
	float textScale = 0.6;
	bool hover = false;
	bool customHover = false;
	std::function<void()> onClick;
	cv::Scalar textColor;

	bool IsSelected(int x, int y)
	{
		return (
			x > rect.x && x < rect.x + rect.width &&
			y > rect.y && y < rect.y + rect.height
			);
	}

	void Draw(cv::Mat& frame);
};