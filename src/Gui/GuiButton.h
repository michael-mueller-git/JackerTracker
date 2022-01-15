#pragma once

#include "Model/Model.h"
#include "GuiElement.h"

#include <opencv2/core.hpp>
#include <string>
#include <functional>
#include <memory>

class GuiButton;

typedef std::vector<std::unique_ptr<GuiButton>> ButtonList;
typedef ButtonList& ButtonListOut;

class GuiButton : public GuiElement
{
public:
	GuiButton(cv::Rect rect, std::function<void()> handler, std::string text);
	GuiButton(cv::Rect rect, std::function<void()> handler, std::string text, OIS::KeyCode hotHey);

	int GetLayerNum() { return GUI_LAYER_BUTTONS; };
	virtual void Draw(cv::Mat& frame);
	bool IsSelected(int x, int y);
	virtual bool Highlighted();

	virtual bool HandleInput(OIS::KeyCode c);
	virtual bool HandleMouse(int e, int x, int y, int f);

	cv::Scalar textColor;
	float textScale = 0.6;
	cv::Rect rect;

	static cv::Rect Next(ButtonListOut out);

protected:
	GuiButton() { ; }
	virtual void Handle() { handler(); };

	bool hasHotkey = false;
	OIS::KeyCode hotKey;

	std::string text;

	bool hover = false;
	std::function<void()> handler;
};