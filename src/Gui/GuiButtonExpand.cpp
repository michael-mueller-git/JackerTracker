#include "GuiButtonExpand.h"

using namespace cv;
using namespace std;
using namespace OIS;

GuiButtonExpand* GuiButtonExpand::openedButton = nullptr;

GuiButtonExpand::GuiButtonExpand(Rect rect, string text)
{
	this->rect = rect;
	this->text = text;
	this->hasHotkey = false;
}

GuiButtonExpand::GuiButtonExpand(Rect r, string t, KeyCode hk)
{
	this->rect = r;
	this->text = t;
	this->hotKey = hk;
	this->hasHotkey = true;
}

void GuiButtonExpand::Handle()
{
	if (openedButton != this)
		openedButton = this;
	else
		openedButton = nullptr;
}

void GuiButtonExpand::Draw(cv::Mat& frame)
{
	GuiButton::Draw(frame);

	if (openedButton != this)
		return;

	for (auto& b : buttons)
		b->Draw(frame);
}

bool GuiButtonExpand::HandleInput(OIS::KeyCode c)
{
	if (GuiButton::HandleInput(c))
		return true;

	if (openedButton != this)
		return false;

	for (auto& b : buttons)
		if (b->HandleInput(c))
			return true;

	return false;
}

bool GuiButtonExpand::HandleMouse(int e, int x, int y, int f)
{
	if (GuiButton::HandleMouse(e, x, y, f))
		return true;

	if (openedButton != this)
		return false;

	for (auto& b : buttons)
		if (b->HandleMouse(e, x, y, f))
			return true;

	return false;
}

GuiButton& GuiButtonExpand::AddButton(GuiButton* b)
{
	buttons.emplace_back(b);
	return *buttons.back();
}

Rect GuiButtonExpand::Next()
{
	int x = 0;

	if (buttons.size() == 0)
	{
		x = rect.x + rect.width;
	}
	else
	{
		for (auto& b : buttons)
		{
			x = max(x, b->rect.x + b->rect.width);
		}
	}

	return cv::Rect(
		x + 20,
		rect.y,
		180,
		40
	);
}