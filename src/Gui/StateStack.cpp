#include "StateStack.h"
#include "TrackingWindow.h"

StateBase* StateStack::GetState() {
	if (stack.size() == 0)
		return nullptr;

	return &(*multiState);
};

void StateStack::PushState(StateBase* s)
{
	bool first = stack.size() == 0;

	stack.emplace_back(s);
	stack.back()->EnterState();

	if (first)
	{
		multiState.reset(new MultiState(&window, *stack.back()));
	}
	else
	{
		multiState->Reset(*stack.back());
	}

	window.DrawWindow(true);
}

void StateStack::ReplaceState(StateBase* s)
{
	assert(stack.size() > 0);

	PopState();
	PushState(s);
}

void StateStack::PopState()
{
	assert(stack.size() > 0);

	auto& s = stack.back();
	s->LeaveState();

	if (stack.size() == 1)
		delete multiState.release();

	stack.pop_back();

	if (stack.size() > 0)
	{
		multiState->Reset(*stack.back());
		stack.back()->EnterState(true);
	}

	window.DrawWindow(true);
}