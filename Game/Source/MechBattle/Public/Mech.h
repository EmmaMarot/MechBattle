#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Mech.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UMechRoutine;
class UMechTerminalScreenWidget;
class UMaterialInterface;

/** Blocs du mecha (voir Documentation/Structure.md). */
UENUM()
enum class EMechBlock : uint8
{
	Torso, Head, Backpack, Pelvis,
	ShoulderL, ShoulderR, ArmL, ArmR, ForearmL, ForearmR, HandL, HandR,
	ThighL, ThighR, ShinL, ShinR, FootL, FootR,
	Count UMETA(Hidden)
};

/** Caractéristiques en dur d'un bloc. */
USTRUCT()
struct FMechBlockDef
{
	GENERATED_BODY()

	/** Dimensions en cm (X avant, Y latéral, Z axe du bloc). */
	UPROPERTY(EditAnywhere, Category = "Bloc")
	FVector Size = FVector(100.0);

	UPROPERTY(EditAnywhere, Category = "Bloc")
	float MassKg = 1000.f;

	/** Centre de gravité local, défini en dur par la nature du bloc. */
	UPROPERTY(EditAnywhere, Category = "Bloc")
	FVector CenterOfGravity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Bloc")
	FLinearColor Color = FLinearColor::Gray;
};

/** Pied : posé au sol ou en phase de pas. Pos = centre de la semelle. */
struct FMechFoot
{
	FVector Pos = FVector::ZeroVector;
	FVector SwingStart = FVector::ZeroVector;
	FVector SwingTarget = FVector::ZeroVector;
	float SwingTime = 0.f;
	float SwingDuration = 0.7f;
	bool bSwing = false;
};

/** État d'équilibre lisible par les routines. Plan horizontal, en cm. Pied 0 = gauche, 1 = droit. */
struct FMechBalanceState
{
	FVector2D CoM = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	/** Accélération imposée par le gyroscope principal. */
	FVector2D ExternalAccel = FVector2D::ZeroVector;
	FVector2D SupportCenter = FVector2D::ZeroVector;
	TArray<FVector2D> SupportPolygon;
	FVector2D FootPos[2];
	bool bFootSwing[2] = { false, false };
	FVector2D Forward = FVector2D(1.0, 0.0);
	FVector2D Right = FVector2D(0.0, 1.0);
	/** Pulsation du pendule inversé : sqrt(g / hauteur du centre de gravité). */
	float Omega = 1.f;
	float CoMHeight = 0.f;
	float TimeSinceLanding = 0.f;
	int32 LastSwingFoot = INDEX_NONE;
	bool bAnySwing = false;
	bool bFallen = false;
};

/**
 * Premier mecha : assemblage de blocs rigides, équilibre de type pendule inversé,
 * gyroscope principal dans le torse et pas gérés par des routines.
 * Conformément à Routines.md, les routines pilotent directement la pose (rig) :
 * pas de ragdoll, la physique d'équilibre est calculée ici.
 */
UCLASS()
class MECHBATTLE_API AMech : public APawn
{
	GENERATED_BODY()

public:
	AMech();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ----- Entrées pilote (écrites par le contrôleur, normalisées -1..1) -----

	/** Gyroscope principal : X = droite, Y = avant. */
	void SetPilotInputs(const FVector2D& InGyro, const FVector& InHead);
	const FVector2D& GetGyroInput() const { return GyroInput; }
	/** Stick de tête : X = inclinaison droite, Y = avant, Z = torsion droite. */
	const FVector& GetHeadInput() const { return HeadInput; }

	// ----- API pour les routines ("verbes d'action") -----

	const FMechBalanceState& GetBalanceState() const { return Balance; }

	/** Point de pression visé sous les pieds (stratégie de cheville). Borné au polygone d'appui. */
	void SetZmpTarget(const FVector2D& Target);

	/** Lance un pas. La cible est bornée par les limites des articulations. */
	bool StartStep(int32 FootIndex, const FVector2D& Target, float Duration);

	/** Orientation de la tête relative au torse, atteinte à la vitesse du moteur de cou. */
	void SetHeadTarget(const FRotator& LocalRotation) { HeadTarget = LocalRotation; }

	/** Point le plus proche dans le polygone d'appui (0 si à l'intérieur). */
	FVector2D ClosestPointInSupport(const FVector2D& Point, float& OutDistance) const;

	/** Point le plus proche sur la semelle d'un pied. */
	FVector2D ClosestPointOnFoot(int32 FootIndex, const FVector2D& Point) const;

	// ----- Système -----

	void ResetStance();
	UMechRoutine* FindRoutine(const FString& Name) const;
	const TArray<TObjectPtr<UMechRoutine>>& GetRoutines() const { return Routines; }
	FString GetStatusText() const;
	UMechTerminalScreenWidget* GetTerminalScreen() const;

	bool bDebugDraw = false;

	// ----- Réglages -----

	UPROPERTY(EditAnywhere, Category = "Mech|Gyroscope")
	float MaxGyroAccel = 320.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Gyroscope")
	float MaxTorsoLean = 12.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Gyroscope")
	float GyroLeanRate = 25.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Tete")
	float NeckSpeed = 150.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float ThighLength = 450.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float ShinLength = 450.f;

	/** Hauteur hanche-cheville en position debout (genoux légèrement fléchis). */
	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float StandLegHeight = 830.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float HipHalfWidth = 95.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float AnkleHeight = 80.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float SwingHeight = 110.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float MaxStepLength = 550.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float MaxStepWidth = 450.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Jambes")
	float MinFootSpacing = 190.f;

	/** Amortissement des stabilisateurs quand aucune routine ne gère l'équilibre. */
	UPROPERTY(EditAnywhere, Category = "Mech|Equilibre")
	float PassiveDamping = 4.f;

	/** Distance du centre de gravité hors du polygone d'appui déclenchant la chute. */
	UPROPERTY(EditAnywhere, Category = "Mech|Equilibre")
	float FallMargin = 40.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Blocs")
	TArray<FMechBlockDef> Blocks;

private:
	void BuildBlockDefs();
	void BuildCockpit();
	void InitStance(const FVector2D& Center);
	void RefreshBalanceState();
	void UpdateSupportPolygon();
	void StepPhysics(float Dt);
	void StepFall(float Dt);
	void StartFall();
	void ComputePose(TArray<FTransform>& OutBlocks, FTransform& OutCamera) const;
	void ApplyPose(float DeltaSeconds);
	void DrawDebug() const;
	float TraceGroundZ(const FVector2D& XY, float Fallback) const;
	FVector2D PelvisXY() const { return CoM - CoGOffsetXY; }

	static FVector2D ClosestPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon, float& OutDistance);

	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TArray<TObjectPtr<UStaticMeshComponent>> BlockMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Mech|Cockpit")
	TArray<TObjectPtr<UStaticMeshComponent>> CockpitMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Mech|Cockpit")
	TObjectPtr<UWidgetComponent> TerminalScreen;

	UPROPERTY()
	TArray<TObjectPtr<UMechRoutine>> Routines;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BlockMaterial;

	// Entrées
	FVector2D GyroInput = FVector2D::ZeroVector;
	FVector HeadInput = FVector::ZeroVector;

	// État physique
	FMechFoot Feet[2];
	FVector2D CoM = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	FVector2D ExternalAccel = FVector2D::ZeroVector;
	FVector2D CoGOffsetXY = FVector2D::ZeroVector;
	FVector2D Zmp = FVector2D::ZeroVector;
	FVector2D ZmpTarget = FVector2D::ZeroVector;
	bool bHasZmpTarget = false;
	TArray<FVector2D> SupportPolygon;
	FVector2D SupportCenter = FVector2D::ZeroVector;
	float Omega = 1.f;
	float CoMHeight = 900.f;
	float GroundZ = 0.f;
	float Yaw0 = 0.f;
	float PelvisZ = 0.f;
	float PelvisBounce = 0.f;
	float PelvisBounceVel = 0.f;
	float TimeSinceLanding = 0.f;
	float PhysicsAccumulator = 0.f;
	int32 LastSwingFoot = INDEX_NONE;
	int32 StepCount = 0;
	FRotator TorsoLean = FRotator::ZeroRotator;
	FRotator HeadLocal = FRotator::ZeroRotator;
	FRotator HeadTarget = FRotator::ZeroRotator;
	float ArmSwing[2] = { 0.f, 0.f };

	// Chute
	bool bFallen = false;
	float FallAngle = 0.f;
	float FallRate = 0.f;
	float FallOffsetAngle = 0.f;
	FVector FallPivot = FVector::ZeroVector;
	FVector FallAxis = FVector::ForwardVector;

	FMechBalanceState Balance;
};
