#include "StateTestTrackers.h"

class StateConfigTracker : public StateTestTrackers
{
public:
	StateConfigTracker(TrackingWindow* window, TrackingSet* set, TrackingTarget* target);
	std::string GetName() { return "ConfigTracker"; }

	void Update();
	void EnterState(bool again);
	void LeaveState();
	void Restart();

protected:
	void* params = nullptr;
	TrackingTarget* target;
};