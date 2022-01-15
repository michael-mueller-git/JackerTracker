#pragma once

#include "States/States.h"

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
	void PopState(bool enter = true);
	StateBase& GetState();
	bool HasState();

protected:
	std::vector<std::unique_ptr<StateBase>> stack;
	TrackingWindow& window;
};