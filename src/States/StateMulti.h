#pragma once

#include "States.h"

class MultiState : public StateBase
{
public:
	MultiState(TrackingWindow* window, StateBase* primaryState)
		:StateBase(window)
	{
		AddState(primaryState, false);
	}

	~MultiState()
	{
		if (states.size() > 1)
			for (auto& s : states)
				if (s.own)
					delete s.state;
	}

	void AddState(StateBase* state, bool own)
	{
		states.push_back({ state , own });
	}

	void EnterState(bool again)
	{
		states.at(0).state->EnterState(again);
	};

	void LeaveState()
	{
		states.at(0).state->LeaveState();
	};

	void UpdateButtons(std::vector<GuiButton>& out)
	{
		for (int s = states.size(); s > 0; s--)
		{
			states.at(s - 1).state->UpdateButtons(out);
		}
	}

	void AddGui(cv::Mat& frame)
	{
		for (auto s : states)
			s.state->AddGui(frame);
	}

	void Update()
	{
		for (auto s : states)
			s.state->Update();
	};

	bool HandleInput(int c)
	{
		for (auto s : states)
			if (s.state->HandleInput(c))
				return true;

		return false;
	}

	bool HandleMouse(int e, int x, int y, int f)
	{
		for (auto s : states)
			if (s.state->HandleMouse(e, x, y, f))
				return true;

		return false;
	}

	std::string GetName() const override
	{
		return states.at(0).state->GetName();
	}

	bool ShouldPop() override
	{
		return states.at(0).state->ShouldPop();
	};


protected:
	struct stateStruct {
		StateBase* state;
		bool own;
	};
	std::vector<stateStruct> states;
};