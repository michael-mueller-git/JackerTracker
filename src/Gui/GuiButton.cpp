#include "GuiButton.h"
#include "Gui/TrackingWindow.h"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;
using namespace OIS;

Rect GuiButton::Next(ButtonListOut out)
{
	int x = 40;
	int y = 220;
	bool first = true;

	for (auto& b : out)
		if (b->rect.x == x)
		{
			y = max(y, b->rect.y + b->rect.height);
			first = false;
		}

	if (!first)
		y += 20;

	return cv::Rect(
		x,
		y,
		230,
		40
	);
}

GuiButton::GuiButton(Rect rect, function<void()> handler, string text, OIS::KeyCode hotKey)
	:textColor(0, 0, 0), rect(rect), handler(handler), text(text), hasHotkey(true), hotKey(hotKey)
{

}

GuiButton::GuiButton(Rect rect, function<void()> handler, string text)
	: textColor(0, 0, 0), rect(rect), handler(handler), text(text), hasHotkey(false)
{

}

void GuiButton::Draw(Mat& frame)
{
	string drawText = text;

	if (hasHotkey)
	{
		drawText.append(" (");
		drawText.append(TrackingWindow::inputKeyboard->getAsString(hotKey));
		drawText.append(")");
	}

	if (Highlighted())
		rectangle(frame, rect, Scalar(70, 70, 70), -1);
	else
		rectangle(frame, rect, Scalar(100, 100, 100), -1);

	if (Highlighted())
		rectangle(frame, rect, Scalar(20, 20, 20), 2);
	else
		rectangle(frame, rect, Scalar(160, 160, 160), 2);

	if (drawText.length() > 0)
	{
		auto size = getTextSize(drawText, FONT_HERSHEY_SIMPLEX, textScale, 2, nullptr);

		putText(
			frame,
			drawText,
			Point(
				rect.x + (rect.width / 2) - (size.width / 2),
				rect.y + (rect.height / 2)
			),
			FONT_HERSHEY_SIMPLEX,
			textScale,
			textColor,
			2
		);
	}
}


bool GuiButton::IsSelected(int x, int y)
{
	return (
		x > rect.x && x < rect.x + rect.width &&
		y > rect.y && y < rect.y + rect.height
	);
}

bool GuiButton::Highlighted()
{
	return hover;
}

bool GuiButton::HandleInput(OIS::KeyCode c)
{
	if (!hasHotkey)
		return false;

	if (c == hotKey)
	{
		handler();
		return true;
	}

	return false;
}

bool GuiButton::HandleMouse(int e, int x, int y, int f)
{
	bool newHover = IsSelected(x, y);
	if (hover != newHover)
	{
		hover = newHover;
		AskDraw();
	}

	if (hover && e == EVENT_LBUTTONUP)
	{
		Handle();
		return true;
	}

	return false;
}