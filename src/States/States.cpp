#include "States.h"
#include "Gui/TrackingWindow.h"

bool StateGlobal::HandleInput(int c)
{
	if (c == 's')
	{
		window->project.Save();
		return true;
	}

	return false;
}