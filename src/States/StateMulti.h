#pragma once

#include "States.h"

#include <memory>

class MultiState : public StateBase
{
public:
	MultiState(TrackingWindow* window, StateBase& primaryState);

	void Reset(StateBase& primaryState);

	void AddState(StateBase& state, bool own)
	{
		states.push_back({ state , own });
	}

	void AddState(StateBase* state, bool own)
	{
		states.push_back({ *state , own });
	}

	void EnterState(bool again)
	{
		PrimaryState().EnterState(again);
	};

	void LeaveState()
	{
		PrimaryState().LeaveState();
	};

	void UpdateButtons(ButtonListOut out)
	{
		for (int s = states.size(); s > 0; s--)
		{
			states.at(s - 1).State().UpdateButtons(out);
		}
	}

	void Draw(cv::Mat& frame)
	{
		for (auto& s : states)
			s.State().Draw(frame);
	}

	void Update()
	{
		for (auto& s : states)
			s.State().Update();
	};

	bool HandleInput(OIS::KeyCode c)
	{
		for (auto& s : states)
			if (s.State().HandleInput(c))
				return true;

		return false;
	}

	bool HandleMouse(int e, int x, int y, int f)
	{
		for (auto& s : states)
			if (s.State().HandleMouse(e, x, y, f))
				return true;

		return false;
	}

	bool DrawRequested() override {
		for (auto& s : states)
			if (s.State().DrawRequested())
				return true;

		return false;
	}

	std::string GetName() override
	{
		return PrimaryState().GetName();
	}

	bool ShouldPop() override
	{
		return PrimaryState().ShouldPop();
	};

protected:
	StateBase& PrimaryState()
	{
		return states.at(0).State();
	}

	struct stateStruct {
		stateStruct(StateBase& stateRef, bool own)
			:own(own), stateRef(stateRef)
		{
			if (own)
				statePtr.reset(&stateRef);
		}

		StateBase& stateRef;
		std::unique_ptr<StateBase> statePtr;

		bool own = false;

		StateBase& State()
		{
			if (own)
				return *statePtr;
			else
				return stateRef;
		}
	};

	std::vector<stateStruct> states;
};