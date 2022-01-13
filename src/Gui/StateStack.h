#pragma once

#include "States/States.h"
#include "States/StateMulti.h"

#include <memory>

class StateStack
{
public:
	StateStack(TrackingWindow& window)
		:window(window)
	{
	};

	void PushState(StateBase* s);
	void ReplaceState(StateBase* s);
	void PopState();
	StateBase* GetState();

protected:
	std::unique_ptr<MultiState> multiState;
	std::vector<std::unique_ptr<StateBase>> stack;
	TrackingWindow& window;
};