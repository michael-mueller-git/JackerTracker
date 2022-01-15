#pragma once

#include "Model/Model.h"
#include "Model/TrackingSet.h"
#include "Model/TrackingTarget.h"
#include "Gui/GuiElement.h"
#include "Gui/GuiButton.h"

#include <functional>
#include <vector>
#include <string>
#include <opencv2/core.hpp>

typedef std::function<void()> StateFailedCallback;

class StateBase : public GuiElement {
public:
	StateBase(TrackingWindow* window)
		:window(window)
	{
		
	}

	virtual void EnterState(bool again = false) {};
	virtual void LeaveState() {};

	virtual void UpdateButtons(ButtonListOut out)
	{
		auto me = this;

		AddButton(out, "Cancel", [me](auto w) {
			me->Pop();
		}, OIS::KC_Q);
	};

	bool HandleInput(OIS::KeyCode c) {
		if (c == OIS::KC_Q)
		{
			Pop();
			return true;
		}
		else
		{
			return HandleStateInput(c);
		}
	};

	int GetLayerNum() { return GUI_LAYER_STATE; };
	virtual void Draw(cv::Mat& frame) { ; };
	virtual void Update() {};
	virtual bool HandleStateInput(OIS::KeyCode c) { return false; };
	virtual bool HandleMouse(int e, int x, int y, int f) { return false; };
	virtual std::string GetName() = 0;

	virtual bool ShouldPop() { return popMe; };
protected:

	void Pop() { popMe = true; };

	GuiButton& AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler, cv::Rect r, OIS::KeyCode hotKey);
	GuiButton& AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler, cv::Rect r);

	GuiButton& AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler)
	{
		return AddButton(out, text, handler, GuiButton::Next(out));
	}

	GuiButton& AddButton(ButtonListOut out, std::string text, std::function<void(TrackingWindow* window)> handler, OIS::KeyCode hotKey)
	{
		return AddButton(out, text, handler, GuiButton::Next(out), hotKey);
	}

	TrackingWindow* window;
	bool popMe = false;
};

class StateGlobal : public StateBase
{
public:
	StateGlobal(TrackingWindow* window)
		:StateBase(window)
	{

	}

	void UpdateButtons(ButtonListOut out);

	std::string GetName() { return "Global"; }
};