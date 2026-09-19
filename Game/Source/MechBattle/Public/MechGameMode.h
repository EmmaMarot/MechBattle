#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MechGameMode.generated.h"

UCLASS()
class MECHBATTLE_API AMechGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMechGameMode();

	virtual void StartPlay() override;

	/** Demi-côté du terrain d'essai généré au lancement (cm). */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	float GroundHalfSize = 250000.f;

private:
	void SpawnTestGround();
};
