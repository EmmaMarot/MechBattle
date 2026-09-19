#include "MechRoutine.h"

void UMechRoutine::TickRoutine(AMech* Mech, float DeltaSeconds)
{
	if (bActive && !bRunning)
	{
		bRunning = true;
		Init(Mech);
	}

	if (bActive && bRunning)
	{
		Process(Mech, DeltaSeconds);
	}
	else if (!bActive && bRunning)
	{
		bRunning = false;
		End(Mech);
	}
}
