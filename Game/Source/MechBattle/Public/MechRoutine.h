#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MechRoutine.generated.h"

class AMech;

/**
 * Routine embarquée (voir Documentation/Routines.md).
 * Cycle de vie : INIT au lancement, PROCESS à chaque cycle tant qu'elle est active,
 * END une fois après la désactivation (le cycle en cours se termine d'abord).
 */
UCLASS(Abstract)
class MECHBATTLE_API UMechRoutine : public UObject
{
	GENERATED_BODY()

public:
	/** Nom affiché et utilisé dans le terminal (ex. EQUILIBRE). */
	UPROPERTY(EditAnywhere, Category = "Routine")
	FString RoutineName;

	/** Plus la valeur est basse, plus la routine est prioritaire. */
	UPROPERTY(EditAnywhere, Category = "Routine")
	int32 Priority = 10000;

	void Activate() { bActive = true; }
	void Deactivate() { bActive = false; }
	bool IsActive() const { return bActive; }
	bool IsRunning() const { return bRunning; }

	/** Appelé par le mecha à chaque frame : gère INIT / PROCESS / END. */
	void TickRoutine(AMech* Mech, float DeltaSeconds);

protected:
	virtual void Init(AMech* Mech) {}
	virtual void Process(AMech* Mech, float DeltaSeconds) {}
	virtual void End(AMech* Mech) {}

private:
	bool bActive = false;
	bool bRunning = false;
};
