#pragma once

#include "CoreMinimal.h"
#include "MechRoutine.h"
#include "MechRoutines.generated.h"

/**
 * EQUILIBRE : routine passive (tout dans PROCESS).
 * Stratégie de cheville tant que le point de capture reste dans le polygone d'appui,
 * sinon fait un pas pour rattraper le déséquilibre. Pousser le gyroscope principal
 * déséquilibre le mecha : la routine compense en marchant.
 */
UCLASS()
class MECHBATTLE_API UMechBalanceRoutine : public UMechRoutine
{
	GENERATED_BODY()

public:
	UMechBalanceRoutine();

	/** Gain de rappel du point de capture vers le centre de l'appui. */
	UPROPERTY(EditAnywhere, Category = "Equilibre")
	float CaptureGain = 1.0f;

	/** Distance hors appui du point de capture déclenchant un pas (cm). */
	UPROPERTY(EditAnywhere, Category = "Equilibre")
	float StepTrigger = 10.f;

	UPROPERTY(EditAnywhere, Category = "Equilibre")
	float StepDuration = 0.7f;

	/** Temps minimal en double appui entre deux pas (s). */
	UPROPERTY(EditAnywhere, Category = "Equilibre")
	float MinDoubleSupport = 0.05f;

	/** Décalage latéral du pied posé par rapport au point de capture (cm). */
	UPROPERTY(EditAnywhere, Category = "Equilibre")
	float LateralNudge = 35.f;

protected:
	virtual void Process(AMech* Mech, float DeltaSeconds) override;
};

/**
 * VISEE_TETE : lit le joystick (position absolue) et oriente la tête.
 * La position du stick correspond directement à l'orientation de la tête.
 */
UCLASS()
class MECHBATTLE_API UMechHeadAimRoutine : public UMechRoutine
{
	GENERATED_BODY()

public:
	UMechHeadAimRoutine();

	UPROPERTY(EditAnywhere, Category = "Tete")
	float MaxYaw = 90.f;

	UPROPERTY(EditAnywhere, Category = "Tete")
	float MaxPitch = 45.f;

	UPROPERTY(EditAnywhere, Category = "Tete")
	float MaxRoll = 30.f;

protected:
	virtual void Process(AMech* Mech, float DeltaSeconds) override;
};
