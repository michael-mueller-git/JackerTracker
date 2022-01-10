#include "GuiButton.h"

#include <opencv2/imgproc.hpp>

using namespace cv;

void GuiButton::Draw(Mat& frame)
{
	if (hover)
		rectangle(frame, rect, Scalar(70, 70, 70), -1);
	else
		rectangle(frame, rect, Scalar(100, 100, 100), -1);

	if (hover)
		rectangle(frame, rect, Scalar(20, 20, 20), 2);
	else
		rectangle(frame, rect, Scalar(160, 160, 160), 2);

	if (text.length() > 0)
	{
		auto size = getTextSize(text, FONT_HERSHEY_SIMPLEX, textScale, 2, nullptr);

		putText(
			frame,
			text,
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