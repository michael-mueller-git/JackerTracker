#include "StateStack.h"
#include "TrackingWindow.h"

StateBase* StateStack::GetState() {
	if (stack.size() == 0)
		return nullptr;

	if (!multiState)
	{
		multiState = new MultiState(&window, stack.back());
		multiState->AddState(new StateGlobal(&window), true);
		multiState->AddState(&window.timebar, false);
	}

	dirty = false;

	return multiState;
};

void StateStack::PushState(StateBase* s)
{
	if (multiState)
	{
		delete multiState;
		multiState = nullptr;
	}

	dirty = true;

	stack.push_back(s);
	s->EnterState();
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

	auto s = stack.back();
	stack.pop_back();
	s->LeaveState();
	delete s;

	if (multiState)
	{
		delete multiState;
		multiState = nullptr;
	}

	dirty = true;

	if (stack.size() > 0)
	{
		s = stack.back();
		s->EnterState();
		window.DrawWindow(true);
	}
}

void StateStack::ReloadTop()
{
	auto s = GetState();
	if (!s)
		return;

	s->LeaveState();
	s->EnterState();
	window.DrawWindow(true);
}