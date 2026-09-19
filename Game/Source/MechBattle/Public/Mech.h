#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Mech.generated.h"

class UAnimationAsset;
class UBlendSpace;
class UCameraComponent;
class UMaterialInterface;
class UMechRoutine;
class UMechTerminalScreenWidget;
class UPhysicalAnimationComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/** Blocs du mecha (voir Documentation/Structure.md). */
UENUM()
enum class EMechBlock : uint8
{
	Torso, Neck, Head, Backpack, Pelvis,
	ShoulderL, ShoulderR, ArmL, ArmR, ForearmL, ForearmR, HandL, HandR,
	ThighL, ThighR, ShinL, ShinR, FootL, FootR,
	Count UMETA(Hidden)
};

/** Caractéristiques en dur d'un bloc. */
USTRUCT()
struct FMechBlockDef
{
	GENERATED_BODY()

	/** Dimensions en cm (X avant, Y latéral, Z axe du bloc). Pour les membres, la longueur suit le squelette. */
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

/**
 * Gyroscope principal (composant du torse). Ce n'est pas un vrai gyroscope simulé : il se résume à deux stats
 * qui filtrent les forces extérieures (impacts, gravité) appliquées au torse. Le pilotage n'est pas concerné.
 */
USTRUCT()
struct FMechGyroscope
{
	GENERATED_BODY()

	/** Diviseur des forces extérieures (1 = aucun effet stabilisateur). */
	UPROPERTY(EditAnywhere, Category = "Gyroscope", meta = (ClampMin = "1.0"))
	float Stability = 4.f;

	/** Force minimale (kN, après division par la stabilité) pour que le torse bouge. */
	UPROPERTY(EditAnywhere, Category = "Gyroscope", meta = (ClampMin = "0.0"))
	float InertiaKN = 150.f;

	/** Consommation électrique (kW) — pas encore simulée. */
	UPROPERTY(EditAnywhere, Category = "Gyroscope")
	float PowerDrawKW = 400.f;

	/** Chaleur produite (kW) — pas encore simulée. */
	UPROPERTY(EditAnywhere, Category = "Gyroscope")
	float HeatOutputKW = 60.f;

	/** Un gyroscope détruit ne filtre plus rien (stabilité 1, inertie 0). */
	UPROPERTY(EditAnywhere, Category = "Gyroscope")
	bool bOperational = true;

	float EffectiveStability() const { return bOperational ? FMath::Max(Stability, 1.f) : 1.f; }
	float EffectiveInertiaN() const { return bOperational ? InertiaKN * 1000.f : 0.f; }
};

/**
 * Mecha : assemblage de blocs rigides.
 *
 * Animation : un squelette humanoïde (Manny, mis à l'échelle) joue des animations classiques
 * (idle / marche / jogging / chute). Ses jambes sont simulées physiquement et tirées vers la pose animée
 * par le Physical Animation Component : le mecha "cherche à reproduire" l'animation sous contraintes physiques.
 * Les blocs sont placés sur les os. La cadence des pas est fixée par la longueur de jambe (pendule),
 * l'amplitude varie avec la vitesse.
 *
 * Déplacement : le pilote commande le mouvement (mini-stick), le buste (joystick X), la tête et les réacteurs.
 * Centre de gravité, polygone d'appui et chute restent calculés pour les impacts.
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

	/** Déplacement : X = latéral (droite), Y = avant. Stick joystick : X = rotation du buste, Y = tête avant, Z = torsion. */
	void SetPilotInputs(const FVector2D& InMove, const FVector& InJoystick);
	const FVector2D& GetMoveInput() const { return MoveInput; }
	const FVector& GetJoystickInput() const { return JoystickInput; }

	/** Réacteurs dorsaux : puissance avant (0..1), verticale (0..1), latérale (-1..1, droite positive). */
	void SetThrusterInputs(float InForward, float InVertical, float InLateral);

	/** Verrouillage du torse : il garde son angle par rapport aux jambes ; le joystick fait alors tourner tout le mecha. */
	void ToggleTorsoLock();
	bool IsTorsoLocked() const { return bTorsoLocked; }

	/** Réaligne le buste sur les jambes (même verrouillé). */
	void AlignTorsoToLegs() { bAligningTorso = true; }

	bool IsAirborne() const { return bAirborne; }

	// ----- Équilibre (pour les impacts) -----

	/** Centre de gravité global (moyenne des blocs pondérée par leur masse). */
	const FVector& GetCenterOfGravity() const { return CoGWorld; }

	/** Fait basculer le mecha dans une direction (ex. coup trop violent). */
	void StartFall(const FVector2D& Direction);

	/**
	 * Force extérieure horizontale appliquée au torse (N) pendant DurationSeconds (ex. impact).
	 * Filtrée par le gyroscope : seule la part de (force / stabilité) qui dépasse l'inertie fait bouger le torse.
	 * @return la variation de vitesse effectivement subie (cm/s), 0 si la force est absorbée.
	 */
	float ApplyExternalForce(const FVector2D& ForceN, float DurationSeconds = 0.1f);

	float GetTotalMassKg() const;
	float GetTorsoYaw() const { return TorsoYaw; }

	UPROPERTY(EditAnywhere, Category = "Mech|Gyroscope")
	FMechGyroscope Gyroscope;

	/** Recul (cm/s) au-delà duquel un impact fait tomber le mecha. */
	UPROPERTY(EditAnywhere, Category = "Mech|Gyroscope")
	float FallKnockbackSpeed = 500.f;

	// ----- Système -----

	void ResetStance();
	UMechRoutine* FindRoutine(const FString& Name) const;
	const TArray<TObjectPtr<UMechRoutine>>& GetRoutines() const { return Routines; }
	FString GetStatusText() const;
	UMechTerminalScreenWidget* GetTerminalScreen() const;

	bool bDebugDraw = false;

	/** Vue externe de debug (touche V). */
	void SetExternalView(bool bExternal);
	bool IsExternalView() const { return bExternalView; }

	/** Affiche le "bonhomme bâton" animé que le mecha cherche à reproduire (debug). */
	void SetGuideVisible(bool bVisible);
	bool IsGuideVisible() const;

	/** Règle la force avec laquelle les jambes simulées suivent l'animation (0 = jambes non simulées). */
	void SetLegDrive(float OrientationStrength, float AngularVelocityStrength);

	/** Écart moyen (cm) entre les pieds simulés et les pieds du guide. */
	float GetLegDeviation() const;

	// ----- Réglages : gabarit et cadence -----

	/** Mise à l'échelle du squelette humanoïde (Manny ~1,8 m -> mecha ~18 m). */
	UPROPERTY(EditAnywhere, Category = "Mech|Gabarit")
	float SkeletonScale = 10.f;

	/** Longueur de jambe (hanche-cheville, cm) : fixe la cadence (pendule) et la vitesse de marche maximale. */
	UPROPERTY(EditAnywhere, Category = "Mech|Gabarit")
	float LegLength = 900.f;

	/** Durée d'un pas = CadenceFactor × π × sqrt(L / g). 0.53 reproduit la cadence de marche humaine. */
	UPROPERTY(EditAnywhere, Category = "Mech|Gabarit")
	float CadenceFactor = 0.53f;

	/** Nombre de Froude maximal en marche (v² / gL) : au-delà, un bipède ne peut plus marcher. */
	UPROPERTY(EditAnywhere, Category = "Mech|Gabarit")
	float MaxWalkFroude = 0.7f;

	// ----- Réglages : déplacement -----

	UPROPERTY(EditAnywhere, Category = "Mech|Deplacement")
	float BackwardSpeedRatio = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Mech|Deplacement")
	float StrafeSpeedRatio = 0.6f;

	/** Accélération maximale (cm/s²). */
	UPROPERTY(EditAnywhere, Category = "Mech|Deplacement")
	float MaxAcceleration = 650.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Deplacement")
	float MaxDeceleration = 900.f;

	// ----- Réglages : réacteurs dorsaux -----

	/** Puissance avant jusqu'à laquelle le mecha reste au sol (au-delà, il est porté par les réacteurs). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float CarryThreshold = 0.55f;

	/** Gain de vitesse au sol à la puissance CarryThreshold (1.0 = double de la marche : il court). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float RunBoostGain = 1.0f;

	/** Accélération des réacteurs avant à 100 % (cm/s²). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float ForwardThrustAccel = 1500.f;

	/** Accélération des réacteurs latéraux à 100 % (cm/s²). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float LateralThrustAccel = 900.f;

	/** Traînée en vol (1/s) : vitesse maximale = accélération / traînée (1500 / 0.33 = ~45 m/s). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float AirDrag = 0.33f;

	/** Puissance verticale qui compense exactement le poids : en dessous, le mecha est seulement "moins lourd". */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float LiftThreshold = 0.8f;

	/** Hauteur de vol quand le mecha est porté par les réacteurs avant (cm). */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float HoverHeight = 250.f;

	/** Inclinaison des tuyères avant vers le bas (°) : elles portent le mecha parallèlement au sol. */
	UPROPERTY(EditAnywhere, Category = "Mech|Reacteurs")
	float ForwardNozzleAngle = 8.f;

	// ----- Réglages : buste et tête -----

	/** Vitesse de rotation du buste à fond de stick (°/s). */
	UPROPERTY(EditAnywhere, Category = "Mech|Buste")
	float MaxTorsoYawRate = 90.f;

	/** Angle maximal buste / hanches ; au-delà, les hanches et les jambes suivent (°). */
	UPROPERTY(EditAnywhere, Category = "Mech|Buste")
	float MaxWaistTwist = 60.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Tete")
	float MaxHeadPitch = 45.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Tete")
	float MaxHeadYaw = 90.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Tete")
	float NeckSpeed = 150.f;

	// ----- Réglages : inclinaison en mouvement (animation) -----

	/** Inclinaison dans le sens du déplacement à la vitesse de marche maximale (°) ; croît avec le carré de la vitesse. */
	UPROPERTY(EditAnywhere, Category = "Mech|Inclinaison")
	float SpeedLean = 4.f;

	/** Inclinaison d'anticipation à accélération maximale (°). */
	UPROPERTY(EditAnywhere, Category = "Mech|Inclinaison")
	float AccelerationLean = 3.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Inclinaison")
	float LeanSmoothing = 3.f;

	// ----- Réglages : physique des jambes -----

	/**
	 * Raideur (1/s², mode accélération) avec laquelle les jambes simulées suivent la pose animée (Physical Animation).
	 * Amortissement conseillé : critique, 2 × sqrt(raideur). Trop d'amortissement fait traîner les jambes.
	 */
	UPROPERTY(EditAnywhere, Category = "Mech|Physique")
	float LegOrientationStrength = 20000.f;

	UPROPERTY(EditAnywhere, Category = "Mech|Physique")
	float LegAngularVelocityStrength = 280.f;

	/** Écart (cm) entre un pied simulé et le pied du guide au-delà duquel la jambe est recalée (jambe coincée). */
	UPROPERTY(EditAnywhere, Category = "Mech|Physique")
	float LegRecoverDistance = 300.f;

	/** Désactive la simulation des jambes (copie exacte de l'animation), pour comparer. */
	UPROPERTY(EditAnywhere, Category = "Mech|Physique")
	bool bSimulateLegs = true;

	UPROPERTY(EditAnywhere, Category = "Mech|Blocs")
	TArray<FMechBlockDef> Blocks;

	/** Luminosité (émissif HDR) de l'écran du terminal. */
	UPROPERTY(EditAnywhere, Category = "Mech|Cockpit")
	float ScreenBrightness = 6000.f;

	/** Vitesse de marche maximale (cm/s), déduite du nombre de Froude et de la longueur de jambe. */
	float GetMaxWalkSpeed() const;

	/** Durée d'un pas (s), déduite de la longueur de jambe. */
	float GetStepPeriod() const;

private:
	void BuildBlockDefs();
	void BuildCockpit();
	void SetupSkeletons();
	void InitStance(const FVector2D& Center);
	void UpdateTorso(float Dt);
	void UpdateMovement(float Dt);
	void UpdateFlight(float Dt);
	void UpdateLean(float Dt);
	void UpdateAnimation(float Dt);
	void SnapLegsToGuide();
	void TakeOff();
	void Land();
	void CheckGravity();
	void UpdateSupportPolygon();
	void StepFall(float Dt);
	void ComputePose(TArray<FTransform>& OutBlocks, FTransform& OutCamera) const;
	void ApplyPose(float DeltaSeconds);
	void DrawDebug() const;
	float TraceGroundZ(const FVector2D& XY, float Fallback) const;
	FVector2D PelvisXY() const { return Position - CoGOffsetXY; }
	FQuat HipsQuat() const { return FQuat(FRotator(0.f, HipsYaw, 0.f)); }
	FVector BoneLocation(const USkeletalMeshComponent* Mesh, FName Bone) const;

	static FVector2D ClosestPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon, float& OutDistance);

	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<USceneComponent> Root;

	/** Squelette du mecha : jambes simulées, tirées vers la pose animée. Invisible (les blocs le représentent). */
	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<USkeletalMeshComponent> BodySkeleton;

	/** "Bonhomme bâton" : la même animation, sans physique. Invisible sauf en debug. */
	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<USkeletalMeshComponent> GuideSkeleton;

	UPROPERTY(VisibleAnywhere, Category = "Mech")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

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

	UPROPERTY()
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	UPROPERTY()
	TObjectPtr<UAnimationAsset> FallAnimation;

	// Entrées
	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector JoystickInput = FVector::ZeroVector;
	float ThrustForward = 0.f;
	float ThrustVertical = 0.f;
	float ThrustLateral = 0.f;

	// Déplacement
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	FVector2D TargetVelocity = FVector2D::ZeroVector;
	FVector2D Acceleration = FVector2D::ZeroVector;
	float HipsYaw = 0.f;
	float TorsoYaw = 0.f;
	FRotator BalanceLean = FRotator::ZeroRotator;
	FRotator HeadLocal = FRotator::ZeroRotator;
	/** Torse qui encaisse un impact (pitch/roll en degrés), revient par ressort. */
	FVector2D HitTilt = FVector2D::ZeroVector;
	FVector2D HitTiltVelocity = FVector2D::ZeroVector;

	// Vol
	bool bAirborne = false;
	float Height = 0.f;
	float VerticalSpeed = 0.f;
	float LandingBounce = 0.f;
	float LandingBounceVelocity = 0.f;

	// Verrouillage du torse
	bool bTorsoLocked = false;
	bool bAligningTorso = false;
	float LockedTwist = 0.f;

	// Animation
	bool bPlayingFall = false;
	/** Temps depuis le dernier changement d'animation : la physique adoucit la transition, pas de recalage. */
	float TimeSinceAnimSwitch = 0.f;
	float AnimSpeed = 0.f;
	float AnimDirection = 0.f;
	float AnimPlayRate = 1.f;

	// Corps
	FVector2D CoGOffsetXY = FVector2D::ZeroVector;
	FVector CoGWorld = FVector::ZeroVector;
	float CoMHeight = 900.f;
	float GroundZ = 0.f;
	TArray<FVector2D> SupportPolygon;
	bool bFootPlanted[2] = { true, true };

	bool bExternalView = false;

	// Chute
	bool bFallen = false;
	float FallAngle = 0.f;
	float FallRate = 0.f;
	FVector FallPivot = FVector::ZeroVector;
	FVector FallAxis = FVector::ForwardVector;
};
