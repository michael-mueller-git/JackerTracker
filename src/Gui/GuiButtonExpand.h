#pragma once

#include "GuiButton.h"

class GuiButtonExpand : public GuiButton
{
public:
	GuiButtonExpand(cv::Rect rect, std::string text);
	GuiButtonExpand(cv::Rect rect, std::string text, OIS::KeyCode hotHey);

	void Draw(cv::Mat& frame);

	bool HandleInput(OIS::KeyCode c);
	bool HandleMouse(int e, int x, int y, int f);
	GuiButton& AddButton(GuiButton* b);

	cv::Rect Next();

protected:
	void Handle() override;

	bool open = false;
	static GuiButtonExpand* openedButton;
	ButtonList buttons;
};

