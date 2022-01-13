#include "StateMulti.h"
#include "Gui/TrackingWindow.h"

MultiState::MultiState(TrackingWindow* window, StateBase& primaryState)
	:StateBase(window)
{
	Reset(primaryState);
}

void MultiState::Reset(StateBase& primaryState)
{
	states.clear();

	AddState(primaryState, false);

	AddState(new StateGlobal(window), true);
	AddState(window->timebar, false);
}