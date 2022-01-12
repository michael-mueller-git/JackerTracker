#pragma once

#include "States/States.h"
#include "States/StateMulti.h"

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
	bool IsDirty() { return dirty; };

protected:
	MultiState* multiState = nullptr;
	std::vector<StateBase*> stack;
	TrackingWindow& window;
	bool dirty = false;
};