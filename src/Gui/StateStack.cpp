#include "StateStack.h"
#include "TrackingWindow.h"

bool StateStack::HasState()
{
	return stack.size() > 0;
}

StateBase& StateStack::GetState() {
	assert(HasState());

	return *stack.back();
};

void StateStack::PushState(StateBase* s)
{
	bool first = stack.size() == 0;

	stack.emplace_back(s);
	stack.back()->EnterState();

	window.DrawWindow(true);
}

void StateStack::ReplaceState(StateBase* s)
{
	assert(stack.size() > 0);

	PopState(false);
	PushState(s);
}

void StateStack::PopState(bool enter)
{
	assert(stack.size() > 0);

	auto& s = stack.back();
	s->LeaveState();

	window.ClearElements();
	stack.pop_back();

	if (HasState() && enter)
		GetState().EnterState(true);

	window.DrawWindow(true);
}