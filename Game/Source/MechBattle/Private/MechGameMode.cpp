#include "MechGameMode.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Mech.h"
#include "MechPlayerController.h"

AMechGameMode::AMechGameMode()
{
	DefaultPawnClass = AMech::StaticClass();
	PlayerControllerClass = AMechPlayerController::StaticClass();
}

void AMechGameMode::StartPlay()
{
	SpawnTestGround();
	Super::StartPlay();
}

void AMechGameMode::SpawnTestGround()
{
	UWorld* World = GetWorld();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Grid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (!World || !Cube)
	{
		return;
	}

	// Se cale juste sous le sol existant de la map (s'il y en a un) pour l'étendre sans le masquer.
	float FloorZ = 0.f;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, FVector(0, 0, 100000), FVector(0, 0, -100000), ECC_Visibility))
	{
		FloorZ = Hit.ImpactPoint.Z;
	}

	constexpr float Thickness = 100.f;
	const FVector Location(0, 0, FloorZ - 2.f - Thickness * 0.5f);
	AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
	if (!Ground)
	{
		return;
	}

	UStaticMeshComponent* Mesh = Ground->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Cube);
	Mesh->SetWorldScale3D(FVector(GroundHalfSize * 2.f / 100.f, GroundHalfSize * 2.f / 100.f, Thickness / 100.f));
	if (Grid)
	{
		Mesh->SetMaterial(0, Grid);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}
