#include "States.h"
#include "Gui/TrackingWindow.h"

bool StateGlobal::HandleInput(char c)
{
	if (c == 's')
	{
		window->project.Save();
		return true;
	}

	return false;
}