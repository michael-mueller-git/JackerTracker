#pragma once

#include "Model/TrackingSet.h"
#include "Model/TrackingTarget.h"
#include "Gui/GuiButton.h"

#include <functional>
#include <vector>
#include <string>
#include <opencv2/core.hpp>

class TrackingWindow;
typedef std::function<void()> StateFailedCallback;

class StateBase {
public:
	StateBase(TrackingWindow* window)
		:window(window)
	{
		
	}

	virtual void EnterState(bool again = false) {};
	virtual void LeaveState() {};

	virtual void UpdateButtons(std::vector<GuiButton>& out)
	{
		auto me = this;

		AddButton(out, "Cancel (q)", [me]() {
			me->Pop();
		});
	};
	virtual void AddGui(cv::Mat& frame) { ; }
	virtual void Update() {};
	virtual bool HandleInput(int c) { return false; };
	virtual bool HandleMouse(int e, int x, int y, int f) { return false; };
	virtual std::string GetName() const = 0;

	virtual bool ShouldPop() { return popMe; };
	void Pop() { popMe = true; };
	GuiButton& AddButton(std::vector<GuiButton>& out, std::string text, std::function<void()> onClick, cv::Rect r)
	{
		GuiButton b;
		b.text = text;
		b.onClick = onClick;
		b.rect = r;

		out.push_back(b);

		return out.back();
	}
	GuiButton& AddButton(std::vector<GuiButton>& out, std::string text, std::function<void()> onClick)
	{
		int x = 40;
		int y = 220;

		for (auto& b : out)
			if (b.rect.x == x)
				y += (40 + 20);

		return AddButton(out, text, onClick, cv::Rect(
			x,
			y,
			230,
			40
		));
	}

	GuiButton& AddButton(std::vector<GuiButton>& out, std::string text, char c)
	{
		auto me = this;

		return AddButton(out, text + " (" + c + ")", [me, c]() {
			me->HandleInput(c);
		});
	}

protected:
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

	void UpdateButtons(std::vector<GuiButton>& out)
	{
		AddButton(out, "Save", 's');
	}

	bool HandleInput(int c) override;

	std::string GetName() const { return "Global"; }
};