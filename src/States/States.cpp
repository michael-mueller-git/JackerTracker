#include "States.h"
#include "Gui/TrackingWindow.h"

using namespace OIS;

GuiButton& StateBase::AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler, cv::Rect r)
{
	TrackingWindow* window = this->window;

	out.emplace_back(new GuiButton(r,
		[window, handler]() {
			handler(window);
		}
	, text));
	return *out.back();
}

GuiButton& StateBase::AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler, cv::Rect r, KeyCode hotKey)
{
	TrackingWindow* window = this->window;

	out.emplace_back(new GuiButton(r,
		[window, handler]() {
			handler(window);
		}
	, text, hotKey));
	return *out.back();
}

void StateGlobal::UpdateButtons(ButtonListOut out)
{
	AddButton(out, "Save", [](auto w) {
		w->project.Save();
	}, KC_S);
}