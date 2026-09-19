#include "Mech.h"

#include "Algo/Sort.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeomTools.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/ConvexHull2d.h"
#include "MechRoutine.h"
#include "MechTerminalWidgets.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogMech, Log, All);

namespace
{
	constexpr float Gravity = 980.f;
	constexpr int32 NumBlocks = static_cast<int32>(EMechBlock::Count);

	/** Durée d'un pas de l'animation de marche humaine (4 pas par boucle de 1,5 s). */
	constexpr float WalkAnimationStep = 0.375f;
	/** Vitesse du jogging dans le blend space (cm/s, taille humaine). */
	constexpr float BlendSpaceMaxSpeed = 600.f;

	const TCHAR* BlockNames[NumBlocks] =
	{
		TEXT("Torse"), TEXT("Cou"), TEXT("Tete"), TEXT("Backpack"), TEXT("Bassin"),
		TEXT("EpauleG"), TEXT("EpauleD"), TEXT("BrasG"), TEXT("BrasD"), TEXT("AvantBrasG"), TEXT("AvantBrasD"), TEXT("MainG"), TEXT("MainD"),
		TEXT("CuisseG"), TEXT("CuisseD"), TEXT("JambeG"), TEXT("JambeD"), TEXT("PiedG"), TEXT("PiedD")
	};

	int32 Idx(EMechBlock Block) { return static_cast<int32>(Block); }

	FQuat YawQuat(float Yaw) { return FQuat(FRotator(0.f, Yaw, 0.f)); }

	/** Bloc allongé entre deux articulations : axe Z du bloc de B vers A, longueur = distance A-B. */
	FTransform LimbTransform(const FVector& A, const FVector& B, const FVector& ForwardHint, const FVector& Size)
	{
		return FTransform(FRotationMatrix::MakeFromZX(A - B, ForwardHint).ToQuat(), (A + B) * 0.5,
			FVector(Size.X, Size.Y, FVector::Distance(A, B)) / 100.0);
	}
}

AMech::AMech()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	// Le squelette physique touche le sol dès l'apparition.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UBlendSpace> Locomotion(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Fall(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	BlockMaterial = ShapeMaterial.Object;
	LocomotionBlendSpace = Locomotion.Object;
	FallAnimation = Fall.Object;

	BuildBlockDefs();

	// Deux squelettes humanoïdes mis à l'échelle, invisibles : le corps (jambes simulées) et le guide
	// ("bonhomme bâton", animation pure, affichable en debug).
	auto MakeSkeleton = [&](const TCHAR* Name)
	{
		USkeletalMeshComponent* Skeleton = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Skeleton->SetupAttachment(Root);
		Skeleton->SetSkeletalMesh(MannyMesh.Object);
		Skeleton->SetUsingAbsoluteLocation(true);
		Skeleton->SetUsingAbsoluteRotation(true);
		Skeleton->SetUsingAbsoluteScale(true);
		Skeleton->SetRelativeScale3D(FVector(SkeletonScale));
		Skeleton->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Skeleton->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Skeleton->SetVisibility(false);
		Skeleton->SetCastShadow(false);
		return Skeleton;
	};
	BodySkeleton = MakeSkeleton(TEXT("SqueletteCorps"));
	BodySkeleton->SetCollisionProfileName(TEXT("Ragdoll"));
	// L'animation pose déjà les pieds au sol : le contact avec le décor statique accrochait les pieds
	// (jambe bloquée, pied freiné en course). Les jambes gardent masse et inertie, et restent sensibles
	// aux objets dynamiques.
	BodySkeleton->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	GuideSkeleton = MakeSkeleton(TEXT("SqueletteGuide"));
	GuideSkeleton->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("AnimationPhysique"));

	for (int32 i = 0; i < NumBlocks; ++i)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(BlockNames[i]);
		Mesh->SetupAttachment(Root);
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		BlockMeshes.Add(Mesh);
	}

	// La caméra suit la tête : ce que voit le pilote sur l'écran principal, c'est ce que voit la tête.
	BlockMeshes[Idx(EMechBlock::Head)]->SetOwnerNoSee(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraTete"));
	Camera->SetupAttachment(Root);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetFieldOfView(90.f);
	Camera->bUsePawnControlRotation = false;

	BuildCockpit();
}

void AMech::BuildBlockDefs()
{
	Blocks.SetNum(NumBlocks);

	auto Set = [this](EMechBlock Block, const FVector& Size, float Mass, const FLinearColor& Color)
	{
		FMechBlockDef& Def = Blocks[Idx(Block)];
		Def.Size = Size;
		Def.MassKg = Mass;
		Def.Color = Color;
	};

	const FLinearColor Frame(0.30f, 0.34f, 0.42f);
	const FLinearColor Armor(0.78f, 0.78f, 0.80f);
	const FLinearColor Dark(0.12f, 0.12f, 0.14f);

	// Pour les membres (et le cou), Size.Z est remplacé par la longueur du segment du squelette.
	Set(EMechBlock::Torso,    FVector(300, 460, 400), 18000.f, Frame);
	Set(EMechBlock::Neck,     FVector(80, 80, 70),      800.f, Dark);
	Set(EMechBlock::Head,     FVector(150, 130, 150),  1500.f, Armor);
	Set(EMechBlock::Backpack, FVector(150, 320, 300),  5000.f, Dark);
	Set(EMechBlock::Pelvis,   FVector(180, 300, 150),  6000.f, Dark);

	for (EMechBlock Block : { EMechBlock::ShoulderL, EMechBlock::ShoulderR }) { Set(Block, FVector(160, 140, 160), 2000.f, Armor); }
	for (EMechBlock Block : { EMechBlock::ArmL, EMechBlock::ArmR })           { Set(Block, FVector(100, 100, 300), 3000.f, Frame); }
	for (EMechBlock Block : { EMechBlock::ForearmL, EMechBlock::ForearmR })   { Set(Block, FVector(110, 110, 300), 3000.f, Armor); }
	for (EMechBlock Block : { EMechBlock::HandL, EMechBlock::HandR })         { Set(Block, FVector(70, 60, 90),    1000.f, Dark); }
	for (EMechBlock Block : { EMechBlock::ThighL, EMechBlock::ThighR })       { Set(Block, FVector(130, 130, 450), 7000.f, Frame); }
	for (EMechBlock Block : { EMechBlock::ShinL, EMechBlock::ShinR })         { Set(Block, FVector(120, 120, 450), 6000.f, Armor); }
	for (EMechBlock Block : { EMechBlock::FootL, EMechBlock::FootR })         { Set(Block, FVector(300, 160, 80),  4000.f, Dark); }
}

void AMech::BuildCockpit()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Cadre du cockpit, fixe autour de l'écran principal (repère caméra : X avant, Y droite, Z haut).
	struct FPiece { const TCHAR* Name; FVector Location; FVector Size; };
	const FPiece Pieces[] =
	{
		{ TEXT("Cockpit_Console"),  FVector(58, 0, -36),  FVector(30, 130, 8) },
		{ TEXT("Cockpit_MontantG"), FVector(58, -58, 0),  FVector(8, 6, 80) },
		{ TEXT("Cockpit_MontantD"), FVector(58, 58, 0),   FVector(8, 6, 80) },
		{ TEXT("Cockpit_Plafond"),  FVector(58, 0, 33),   FVector(8, 130, 5) },
	};

	for (const FPiece& Piece : Pieces)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Piece.Name);
		Mesh->SetupAttachment(Camera);
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeLocation(Piece.Location);
		Mesh->SetRelativeScale3D(Piece.Size / 100.0);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetOnlyOwnerSee(true);
		CockpitMeshes.Add(Mesh);
	}

	// Écran du terminal : un écran secondaire posé sur la console, orienté vers le pilote.
	const FVector ScreenLocation(55, 30, -20);
	const FRotator ScreenRotation = (-ScreenLocation).Rotation();

	UStaticMeshComponent* Bezel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cockpit_EcranCadre"));
	Bezel->SetupAttachment(Camera);
	Bezel->SetStaticMesh(CubeMesh.Object);
	Bezel->SetRelativeLocation(ScreenLocation - ScreenRotation.Vector() * 1.0);
	Bezel->SetRelativeRotation(ScreenRotation);
	Bezel->SetRelativeScale3D(FVector(1.0, 35.0, 23.0) / 100.0);
	Bezel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bezel->SetCastShadow(false);
	Bezel->SetOnlyOwnerSee(true);
	CockpitMeshes.Add(Bezel);

	TerminalScreen = CreateDefaultSubobject<UWidgetComponent>(TEXT("Cockpit_Terminal"));
	TerminalScreen->SetupAttachment(Camera);
	TerminalScreen->SetWidgetSpace(EWidgetSpace::World);
	TerminalScreen->SetWidgetClass(UMechTerminalScreenWidget::StaticClass());
	TerminalScreen->SetDrawSize(FVector2D(800, 500));
	TerminalScreen->SetBlendMode(EWidgetBlendMode::Opaque);
	// Le projet utilise une luminance physique : un émissif de 1 est invisible en plein jour.
	// La teinte HDR donne à l'écran une luminosité lisible dans le cockpit.
	TerminalScreen->SetTintColorAndOpacity(FLinearColor(ScreenBrightness, ScreenBrightness, ScreenBrightness, 1.f));
	TerminalScreen->SetRelativeLocation(ScreenLocation);
	TerminalScreen->SetRelativeRotation(ScreenRotation);
	TerminalScreen->SetRelativeScale3D(FVector(0.04));
	TerminalScreen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerminalScreen->SetCastShadow(false);
	TerminalScreen->SetOnlyOwnerSee(true);
}

void AMech::BeginPlay()
{
	Super::BeginPlay();

	for (int32 i = 0; i < NumBlocks; ++i)
	{
		if (UMaterialInstanceDynamic* Material = BlockMeshes[i]->CreateDynamicMaterialInstance(0, BlockMaterial))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Blocks[i].Color);
		}
	}
	for (UStaticMeshComponent* Mesh : CockpitMeshes)
	{
		if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(0, BlockMaterial))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.035f));
		}
	}

	HipsYaw = TorsoYaw = GetActorRotation().Yaw;
	const FVector Start = GetActorLocation();
	GroundZ = TraceGroundZ(FVector2D(Start), Start.Z);
	Position = FVector2D(Start);
	SetupSkeletons();
	InitStance(FVector2D(Start));
}

void AMech::SetupSkeletons()
{
	// Les animations "motion matching" font avancer l'os racine : on le verrouille pour les jouer sur place,
	// c'est notre déplacement qui fait avancer le mecha.
	auto LockRoot = [](UAnimSequence* Sequence)
	{
		if (Sequence)
		{
			Sequence->bForceRootLock = true;
			Sequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
		}
	};
	if (LocomotionBlendSpace)
	{
		for (const FBlendSample& Sample : LocomotionBlendSpace->GetBlendSamples())
		{
			LockRoot(Sample.Animation);
		}
	}
	LockRoot(Cast<UAnimSequence>(FallAnimation));

	const FVector SkeletonLocation(Position, GroundZ);
	const FRotator SkeletonRotation(0.f, HipsYaw - 90.f, 0.f);
	for (USkeletalMeshComponent* Skeleton : { BodySkeleton.Get(), GuideSkeleton.Get() })
	{
		Skeleton->SetWorldScale3D(FVector(SkeletonScale));
		Skeleton->SetWorldLocationAndRotation(SkeletonLocation, SkeletonRotation, false, nullptr, ETeleportType::ResetPhysics);
		Skeleton->PlayAnimation(LocomotionBlendSpace, true);
		Skeleton->TickAnimation(0.f, false);
		Skeleton->RefreshBoneTransforms();
	}

	PhysicalAnimation->SetSkeletalMeshComponent(BodySkeleton);
	SetLegDrive(bSimulateLegs ? LegOrientationStrength : 0.f, LegAngularVelocityStrength);
}

void AMech::SetLegDrive(float OrientationStrength, float AngularVelocityStrength)
{
	// Jambes simulées, tirées vers la pose animée : le mecha "cherche à reproduire" l'animation
	// avec les contraintes de la physique (masse, contact du sol).
	bSimulateLegs = OrientationStrength > 0.f;
	LegOrientationStrength = OrientationStrength;
	LegAngularVelocityStrength = AngularVelocityStrength;

	// Cibles en espace monde (orientation + position) : en espace local, chaque os suit son parent physique
	// avec une image de retard, ce qui fait traîner les jambes d'un mecha qui avance à plusieurs m/s.
	FPhysicalAnimationData Drive;
	Drive.bIsLocalSimulation = false;
	Drive.OrientationStrength = OrientationStrength;
	Drive.AngularVelocityStrength = AngularVelocityStrength;
	Drive.PositionStrength = OrientationStrength;
	Drive.VelocityStrength = AngularVelocityStrength;
	Drive.MaxLinearForce = 0.f;
	Drive.MaxAngularForce = 0.f;
	for (const TCHAR* Thigh : { TEXT("thigh_l"), TEXT("thigh_r") })
	{
		PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(Thigh, Drive, true);
		BodySkeleton->SetAllBodiesBelowSimulatePhysics(Thigh, bSimulateLegs, true);
	}
	if (bSimulateLegs)
	{
		SnapLegsToGuide();
	}
}

float AMech::GetLegDeviation() const
{
	return 0.5f * (FVector::Distance(BoneLocation(BodySkeleton, TEXT("foot_l")), BoneLocation(GuideSkeleton, TEXT("foot_l")))
		+ FVector::Distance(BoneLocation(BodySkeleton, TEXT("foot_r")), BoneLocation(GuideSkeleton, TEXT("foot_r"))));
}

void AMech::InitStance(const FVector2D& Center)
{
	TorsoYaw = HipsYaw;
	Position = Center;
	Velocity = TargetVelocity = Acceleration = FVector2D::ZeroVector;
	CoGOffsetXY = FVector2D::ZeroVector;
	BalanceLean = FRotator::ZeroRotator;
	bFallen = false;
	bAirborne = false;
	Height = VerticalSpeed = 0.f;
	FallAngle = FallRate = 0.f;
	LandingBounce = LandingBounceVelocity = 0.f;
	GroundZ = TraceGroundZ(Center, GroundZ);
	ApplyPose(0.f);
}

void AMech::ResetStance()
{
	InitStance(Position);
}

void AMech::SetPilotInputs(const FVector2D& InMove, const FVector& InJoystick)
{
	MoveInput = InMove.ClampAxes(-1.0, 1.0);
	JoystickInput = InJoystick.BoundToCube(1.0);
}

void AMech::SetThrusterInputs(float InForward, float InVertical, float InLateral)
{
	ThrustForward = FMath::Clamp(InForward, 0.f, 1.f);
	ThrustVertical = FMath::Clamp(InVertical, 0.f, 1.f);
	ThrustLateral = FMath::Clamp(InLateral, -1.f, 1.f);
}

float AMech::GetStepPeriod() const
{
	// Jambe = pendule : la cadence ne dépend que de la longueur de jambe.
	return CadenceFactor * UE_PI * FMath::Sqrt(LegLength / Gravity);
}

float AMech::GetMaxWalkSpeed() const
{
	// Au-delà d'un nombre de Froude ~0,7, un bipède ne peut plus marcher : il doit courir.
	return FMath::Sqrt(MaxWalkFroude * Gravity * LegLength);
}

void AMech::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Min(DeltaSeconds, 1.f / 20.f);

	// Routines embarquées (aucune chargée pour l'instant : la locomotion de base est native).
	Routines.StableSort([](const UMechRoutine& A, const UMechRoutine& B) { return A.Priority < B.Priority; });
	for (UMechRoutine* Routine : Routines)
	{
		Routine->TickRoutine(this, Dt);
	}

	if (bFallen)
	{
		StepFall(Dt);
	}
	else if (Dt > 0.f)
	{
		UpdateTorso(Dt);
		if (bAirborne)
		{
			UpdateFlight(Dt);
		}
		else
		{
			UpdateMovement(Dt);
			CheckGravity();
		}
		UpdateLean(Dt);
	}

	// Torse qui encaisse un impact, bassin qui encaisse un atterrissage : ressorts amortis.
	HitTiltVelocity += (HitTilt * -40.f - HitTiltVelocity * 9.f) * Dt;
	HitTilt += HitTiltVelocity * Dt;
	LandingBounceVelocity += (-LandingBounce * 30.f - LandingBounceVelocity * 8.f) * Dt;
	LandingBounce += LandingBounceVelocity * Dt;

	// Tête : position absolue du joystick (avant = regarder en bas, torsion = tourner la tête).
	const FRotator HeadTarget(-JoystickInput.Y * MaxHeadPitch, JoystickInput.Z * MaxHeadYaw, 0.f);
	HeadLocal = FMath::RInterpConstantTo(HeadLocal, HeadTarget, Dt, NeckSpeed);

	UpdateAnimation(Dt);
	ApplyPose(Dt);

	if (bDebugDraw)
	{
		DrawDebug();
	}
}

void AMech::ToggleTorsoLock()
{
	bTorsoLocked = !bTorsoLocked;
	LockedTwist = FMath::FindDeltaAngleDegrees(HipsYaw, TorsoYaw);
}

void AMech::UpdateTorso(float Dt)
{
	const float Turn = JoystickInput.X * MaxTorsoYawRate * Dt;

	if (bTorsoLocked)
	{
		// Verrouillé : le buste garde son angle par rapport aux jambes, le joystick fait tourner tout le mecha.
		if (bAligningTorso)
		{
			LockedTwist = FMath::FInterpConstantTo(LockedTwist, 0.f, Dt, MaxTorsoYawRate);
			bAligningTorso = !FMath::IsNearlyZero(LockedTwist, 0.5f);
		}
		HipsYaw = FRotator::NormalizeAxis(HipsYaw + Turn);
		TorsoYaw = FRotator::NormalizeAxis(HipsYaw + LockedTwist);
		return;
	}

	// Libre : le joystick fait tourner le buste. Au-delà de l'angle maximal avec les hanches,
	// les hanches (et donc les jambes) suivent : le mecha tourne.
	TorsoYaw = FRotator::NormalizeAxis(TorsoYaw + Turn);
	float Twist = FMath::FindDeltaAngleDegrees(HipsYaw, TorsoYaw);
	if (bAligningTorso)
	{
		Twist = FMath::FInterpConstantTo(Twist, 0.f, Dt, MaxTorsoYawRate);
		TorsoYaw = FRotator::NormalizeAxis(HipsYaw + Twist);
		bAligningTorso = !FMath::IsNearlyZero(Twist, 0.5f);
	}
	if (FMath::Abs(Twist) > MaxWaistTwist)
	{
		HipsYaw = FRotator::NormalizeAxis(TorsoYaw - FMath::Sign(Twist) * MaxWaistTwist);
	}
}

void AMech::UpdateMovement(float Dt)
{
	// Déplacement dans le repère des hanches (le mecha marche là où vont ses jambes).
	const FQuat Hips = HipsQuat();
	const FVector2D Forward(Hips.GetForwardVector());
	const FVector2D Right(Hips.GetRightVector());
	const FQuat Torso = YawQuat(TorsoYaw);
	const FVector2D TorsoForward(Torso.GetForwardVector());
	const FVector2D TorsoRight(Torso.GetRightVector());

	// Sans réacteurs, le mecha ne fait que marcher : sa vitesse est limitée par sa taille (Froude).
	// Les réacteurs dorsaux (fixés au torse) le poussent au-delà : il court, et l'entraînent même sans consigne.
	const float WalkSpeed = GetMaxWalkSpeed();
	const float Boost = FMath::Clamp(ThrustForward / CarryThreshold, 0.f, 1.f);
	const float Lightness = FMath::Clamp(ThrustVertical / LiftThreshold, 0.f, 1.f);
	const float SpeedGain = 1.f + RunBoostGain * Boost;
	const float MaxBoostedSpeed = WalkSpeed * (1.f + RunBoostGain);
	const float StrafeSpeed = WalkSpeed * StrafeSpeedRatio;

	const float Along = MoveInput.Y >= 0.f ? MoveInput.Y * WalkSpeed * SpeedGain : MoveInput.Y * WalkSpeed * BackwardSpeedRatio;
	TargetVelocity = (Forward * Along + Right * (MoveInput.X * StrafeSpeed)).GetClampedToMaxSize(WalkSpeed * SpeedGain);

	const float ThrustSpeed = Boost * MaxBoostedSpeed;
	const float TargetAlongTorso = FVector2D::DotProduct(TargetVelocity, TorsoForward);
	if (TargetAlongTorso < ThrustSpeed)
	{
		TargetVelocity += TorsoForward * (ThrustSpeed - TargetAlongTorso);
	}
	const float LateralThrustSpeed = ThrustLateral * StrafeSpeed * (1.f + RunBoostGain);
	const float TargetAcrossTorso = FVector2D::DotProduct(TargetVelocity, TorsoRight);
	if (FMath::Abs(TargetAcrossTorso) < FMath::Abs(LateralThrustSpeed))
	{
		TargetVelocity += TorsoRight * (LateralThrustSpeed - TargetAcrossTorso);
	}

	// Inertie : la vitesse suit la consigne avec une accélération limitée (meilleure quand le mecha est
	// poussé ou allégé par les réacteurs).
	const float AccelGain = 1.f + Boost + 0.5f * Lightness;
	const FVector2D Previous = Velocity;
	const FVector2D Delta = TargetVelocity - Velocity;
	const bool bBraking = FVector2D::DotProduct(Delta, Velocity) < 0.f;
	Velocity += Delta.GetClampedToMaxSize((bBraking ? MaxDeceleration : MaxAcceleration * AccelGain) * Dt);
	Position += Velocity * Dt;
	Acceleration = FMath::Vector2DInterpTo(Acceleration, (Velocity - Previous) / Dt, Dt, 6.f);

	// Au-delà des seuils, les réacteurs portent le mecha.
	if (ThrustForward > CarryThreshold || ThrustVertical > LiftThreshold)
	{
		TakeOff();
	}
}

void AMech::TakeOff()
{
	bAirborne = true;
	Height = 0.f;
	VerticalSpeed = 0.f;
	SupportPolygon.Reset();
	UE_LOG(LogMech, Log, TEXT("DECOLLAGE : avant %.0f %%, vertical %.0f %%"), ThrustForward * 100.f, ThrustVertical * 100.f);
}

void AMech::Land()
{
	bAirborne = false;
	UE_LOG(LogMech, Log, TEXT("ATTERRISSAGE : vitesse verticale %.0f cm/s, horizontale %.0f cm/s"), VerticalSpeed, Velocity.Size());
	// Le corps encaisse l'atterrissage (léger affaissement) puis revient.
	LandingBounceVelocity -= FMath::Min(FMath::Abs(VerticalSpeed) * 0.15f + 60.f, 400.f);
	Height = 0.f;
	VerticalSpeed = 0.f;
}

void AMech::UpdateFlight(float Dt)
{
	const FQuat Hips = HipsQuat();
	const FQuat Torso = YawQuat(TorsoYaw);

	// Horizontal : poussée des réacteurs (fixés au torse) moins la traînée, plus un peu de contrôle
	// d'attitude au mini-stick.
	const FVector2D Previous = Velocity;
	FVector2D Accel = FVector2D(Torso.GetForwardVector()) * (ThrustForward * ForwardThrustAccel)
		+ FVector2D(Torso.GetRightVector()) * (ThrustLateral * LateralThrustAccel)
		- Velocity * AirDrag;
	Accel += (FVector2D(Hips.GetForwardVector()) * MoveInput.Y + FVector2D(Hips.GetRightVector()) * MoveInput.X) * 150.f;
	Velocity += Accel * Dt;
	Position += Velocity * Dt;
	Acceleration = FMath::Vector2DInterpTo(Acceleration, (Velocity - Previous) / Dt, Dt, 6.f);

	// Vertical : la poussée verticale compense le poids à LiftThreshold, au-delà le mecha monte.
	// Porté par les réacteurs avant (tuyères légèrement orientées vers le bas), il ne descend pas sous HoverHeight.
	const bool bCarried = ThrustForward > CarryThreshold;
	float VerticalAccel = Gravity * (ThrustVertical / LiftThreshold - 1.f);
	if (bCarried && Height < HoverHeight)
	{
		VerticalAccel = FMath::Max(VerticalAccel, (HoverHeight - Height) * 4.f - VerticalSpeed * 3.f);
	}
	VerticalSpeed += VerticalAccel * Dt;
	Height += VerticalSpeed * Dt;

	if (Height <= 0.f)
	{
		Height = 0.f;
		if (!bCarried && ThrustVertical <= LiftThreshold)
		{
			Land();
		}
		else
		{
			VerticalSpeed = FMath::Max(VerticalSpeed, 0.f);
		}
	}
}

void AMech::UpdateLean(float Dt)
{
	// Inclinaison "animation" dans le sens du déplacement : discrète en marche, plus nette en course
	// (carré de la vitesse), plus une légère anticipation à l'accélération.
	const FQuat Torso = YawQuat(TorsoYaw);
	const FVector2D TorsoForward(Torso.GetForwardVector());
	const FVector2D TorsoRight(Torso.GetRightVector());

	const float SpeedRatio = FMath::Min(Velocity.Size() / GetMaxWalkSpeed(), 2.f);
	const FVector2D SpeedLeanVector = Velocity.GetSafeNormal() * (SpeedLean * SpeedRatio * SpeedRatio * (bAirborne ? 0.2f : 1.f));
	const FVector2D AccelLeanVector = (Acceleration / MaxAcceleration).GetClampedToMaxSize(1.f) * AccelerationLean;
	const FVector2D Lean = SpeedLeanVector + AccelLeanVector;

	const FRotator TargetLean(-FVector2D::DotProduct(Lean, TorsoForward), 0.f, FVector2D::DotProduct(Lean, TorsoRight));
	BalanceLean = FMath::RInterpTo(BalanceLean, TargetLean, Dt, LeanSmoothing);
}

void AMech::UpdateAnimation(float Dt)
{
	const bool bWantFall = bAirborne && !bFallen;
	if (bWantFall != bPlayingFall)
	{
		// En vol : boucle de chute (jambes pendantes). Au sol : locomotion.
		bPlayingFall = bWantFall;
		TimeSinceAnimSwitch = 0.f;
		for (USkeletalMeshComponent* Skeleton : { BodySkeleton.Get(), GuideSkeleton.Get() })
		{
			Skeleton->PlayAnimation(bWantFall ? FallAnimation.Get() : static_cast<UAnimationAsset*>(LocomotionBlendSpace.Get()), true);
		}
	}

	// Une jambe simulée peut rester coincée (sol traversé à l'atterrissage...) : trop loin de la pose du guide,
	// elle y est recalée.
	TimeSinceAnimSwitch += Dt;
	if (bSimulateLegs && TimeSinceAnimSwitch > 0.5f)
	{
		for (const TCHAR* Foot : { TEXT("foot_l"), TEXT("foot_r") })
		{
			if (FVector::Distance(BoneLocation(BodySkeleton, Foot), BoneLocation(GuideSkeleton, Foot)) > LegRecoverDistance)
			{
				UE_LOG(LogMech, Log, TEXT("Jambe recalee sur l'animation (%s)"), Foot);
				SnapLegsToGuide();
				break;
			}
		}
	}
	if (bPlayingFall)
	{
		// Un corps dix fois plus grand bouge sqrt(10) fois plus lentement.
		const float FallPlayRate = 1.f / FMath::Sqrt(SkeletonScale);
		for (USkeletalMeshComponent* Skeleton : { BodySkeleton.Get(), GuideSkeleton.Get() })
		{
			Skeleton->SetPlayRate(FallPlayRate);
		}
		return;
	}

	// Cadence fixe (pendule de la jambe) : l'animation de marche est jouée pour que chaque pas dure
	// GetStepPeriod(). L'amplitude suit la vitesse : le blend space passe d'idle à marche puis jogging
	// selon la vitesse "à taille humaine" équivalente. Au-delà du jogging, seule la cadence peut encore monter.
	const float BaseRate = WalkAnimationStep / GetStepPeriod();
	const float Speed = Velocity.Size();
	float BlendSpeed = Speed / (SkeletonScale * BaseRate);
	float Rate = BaseRate;
	if (BlendSpeed > BlendSpaceMaxSpeed)
	{
		Rate = Speed / (SkeletonScale * BlendSpaceMaxSpeed);
		BlendSpeed = BlendSpaceMaxSpeed;
	}

	// Direction du mouvement dans le repère des hanches (droite positive) : marche latérale / arrière.
	float Direction = AnimDirection;
	if (Speed > 20.f)
	{
		const FQuat Hips = HipsQuat();
		Direction = FMath::RadiansToDegrees(FMath::Atan2(FVector2D::DotProduct(Velocity, FVector2D(Hips.GetRightVector())),
			FVector2D::DotProduct(Velocity, FVector2D(Hips.GetForwardVector()))));
	}

	AnimSpeed = BlendSpeed;
	AnimDirection = FRotator::NormalizeAxis(AnimDirection + FMath::FindDeltaAngleDegrees(AnimDirection, Direction) * FMath::Min(Dt * 6.f, 1.f));
	AnimPlayRate = Rate;

	for (USkeletalMeshComponent* Skeleton : { BodySkeleton.Get(), GuideSkeleton.Get() })
	{
		if (UAnimSingleNodeInstance* Node = Skeleton->GetSingleNodeInstance())
		{
			Node->SetBlendSpacePosition(FVector(AnimDirection, AnimSpeed, 0.f));
			Node->SetPlayRate(AnimPlayRate);
		}
	}
}

void AMech::SnapLegsToGuide()
{
	// Le guide joue exactement la même animation au même endroit : ses os donnent la pose cible.
	for (FBodyInstance* Body : BodySkeleton->Bodies)
	{
		if (Body && Body->IsInstanceSimulatingPhysics() && Body->GetBodySetup())
		{
			const int32 BoneIndex = GuideSkeleton->GetBoneIndex(Body->GetBodySetup()->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				Body->SetBodyTransform(GuideSkeleton->GetBoneTransform(BoneIndex), ETeleportType::TeleportPhysics);
				Body->SetLinearVelocity(FVector::ZeroVector, false);
				Body->SetAngularVelocityInRadians(FVector::ZeroVector, false);
			}
		}
	}
}

float AMech::GetTotalMassKg() const
{
	float TotalMass = 0.f;
	for (const FMechBlockDef& Def : Blocks)
	{
		TotalMass += Def.MassKg;
	}
	return TotalMass;
}

float AMech::ApplyExternalForce(const FVector2D& ForceN, float DurationSeconds)
{
	if (bFallen || ForceN.IsNearlyZero())
	{
		return 0.f;
	}

	// Gyroscope principal : la force est divisée par la stabilité, puis seule la part qui dépasse
	// l'inertie fait bouger le torse.
	const FVector2D Effective = ForceN / Gyroscope.EffectiveStability();
	const float EffectiveSize = Effective.Size();
	const float Inertia = Gyroscope.EffectiveInertiaN();
	if (EffectiveSize <= Inertia)
	{
		UE_LOG(LogMech, Log, TEXT("IMPACT absorbe : %.0f kN / stabilite %.1f = %.0f kN <= inertie %.0f kN"),
			ForceN.Size() / 1000.f, Gyroscope.EffectiveStability(), EffectiveSize / 1000.f, Inertia / 1000.f);
		return 0.f;
	}

	const FVector2D Excess = Effective * (1.f - Inertia / EffectiveSize);
	const FVector2D DeltaVelocity = Excess * DurationSeconds / GetTotalMassKg() * 100.f; // m/s -> cm/s
	Velocity += DeltaVelocity;

	// Le torse encaisse dans le sens du coup.
	const FQuat Torso = YawQuat(TorsoYaw);
	const float Kick = FMath::Min(DeltaVelocity.Size() * 0.04f, 25.f);
	const FVector2D Dir = DeltaVelocity.GetSafeNormal();
	HitTiltVelocity += FVector2D(-FVector2D::DotProduct(Dir, FVector2D(Torso.GetForwardVector())), FVector2D::DotProduct(Dir, FVector2D(Torso.GetRightVector()))) * Kick * 8.f;

	UE_LOG(LogMech, Log, TEXT("IMPACT : %.0f kN / stabilite %.1f - inertie %.0f kN -> recul %.0f cm/s"),
		ForceN.Size() / 1000.f, Gyroscope.EffectiveStability(), Inertia / 1000.f, DeltaVelocity.Size());

	if (DeltaVelocity.Size() > FallKnockbackSpeed)
	{
		StartFall(Dir);
	}
	return DeltaVelocity.Size();
}

void AMech::CheckGravity()
{
	// La gravité est une force extérieure comme une autre : si le centre de gravité sort de l'appui,
	// le couple de bascule (ramené à une force horizontale) passe par le gyroscope.
	// Seulement à l'arrêt, pieds posés : en mouvement, l'appui change sans cesse.
	if (Velocity.Size() > 100.f || !bFootPlanted[0] || !bFootPlanted[1])
	{
		return;
	}
	float Outside = 0.f;
	const FVector2D Edge = ClosestPointInPolygon(FVector2D(CoGWorld), SupportPolygon, Outside);
	if (Outside <= 0.f)
	{
		return;
	}
	// La poussée verticale des réacteurs allège le mecha.
	const float Weight = 1.f - FMath::Clamp(ThrustVertical / LiftThreshold, 0.f, 1.f);
	const float TippingForce = GetTotalMassKg() * Weight * (Gravity / 100.f) * Outside / FMath::Max(CoMHeight, 100.f);
	if (TippingForce / Gyroscope.EffectiveStability() > Gyroscope.EffectiveInertiaN())
	{
		UE_LOG(LogMech, Log, TEXT("DESEQUILIBRE : centre de gravite hors appui de %.0f cm, force %.0f kN"), Outside, TippingForce / 1000.f);
		StartFall(FVector2D(CoGWorld) - Edge);
	}
}

FVector2D AMech::ClosestPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon, float& OutDistance)
{
	OutDistance = 0.f;
	if (Polygon.Num() < 3 || FGeomTools2D::IsPointInPolygon(Point, Polygon))
	{
		return Point;
	}

	FVector2D Best = Point;
	double BestDistSq = TNumericLimits<double>::Max();
	for (int32 i = 0; i < Polygon.Num(); ++i)
	{
		const FVector2D Candidate = FMath::ClosestPointOnSegment2D(Point, Polygon[i], Polygon[(i + 1) % Polygon.Num()]);
		const double DistSq = FVector2D::DistSquared(Point, Candidate);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	OutDistance = FMath::Sqrt(BestDistSq);
	return Best;
}

void AMech::StartFall(const FVector2D& Direction)
{
	if (bFallen)
	{
		return;
	}
	const FVector2D Dir = Direction.GetSafeNormal().IsNearlyZero() ? FVector2D(HipsQuat().GetForwardVector()) : Direction.GetSafeNormal();

	// Pivot : bord de l'appui dans la direction de la chute.
	float Unused = 0.f;
	const FVector2D Edge = ClosestPointInPolygon(Position + Dir * 2000.f, SupportPolygon, Unused);

	bFallen = true;
	bAirborne = false;
	FallPivot = FVector(Edge, GroundZ);
	FallAxis = FVector::CrossProduct(FVector::UpVector, FVector(Dir, 0.0)).GetSafeNormal();
	FallAngle = 0.f;
	FallRate = 0.3f;
	Velocity = FVector2D::ZeroVector;
	UE_LOG(LogMech, Log, TEXT("CHUTE vers (%.2f, %.2f)"), Dir.X, Dir.Y);
}

void AMech::StepFall(float Dt)
{
	const float MaxAngle = FMath::DegreesToRadians(82.f);
	if (FallAngle >= MaxAngle)
	{
		return;
	}
	FallRate += (Gravity / FMath::Max(CoMHeight, 200.f)) * FMath::Sin(FallAngle + 0.05f) * Dt;
	FallAngle = FMath::Min(FallAngle + FallRate * Dt, MaxAngle);
}

float AMech::TraceGroundZ(const FVector2D& XY, float Fallback) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MechGround), false, this);
	const FVector Start(XY, Fallback + 5000.0);
	const FVector End(XY, Fallback - 50000.0);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return Fallback;
}

FVector AMech::BoneLocation(const USkeletalMeshComponent* Mesh, FName Bone) const
{
	return Mesh->GetBoneLocation(Bone, EBoneSpaces::WorldSpace);
}

void AMech::ComputePose(TArray<FTransform>& OutBlocks, FTransform& OutCamera) const
{
	OutBlocks.SetNum(NumBlocks);
	const USkeletalMeshComponent* Body = BodySkeleton;

	const FQuat Hips = HipsQuat();
	const FVector HipsForward = Hips.GetForwardVector();
	const FQuat TorsoQ = YawQuat(TorsoYaw) * FQuat(FRotator(BalanceLean.Pitch + HitTilt.X, 0.f, BalanceLean.Roll + HitTilt.Y));
	const FVector TorsoForward = TorsoQ.GetForwardVector();
	auto Size = [&](EMechBlock Block) { return Blocks[Idx(Block)].Size; };

	// Haut du corps : pose animée, tournée autour de la taille par la rotation du buste, l'inclinaison et les impacts.
	// L'affaissement à l'atterrissage ne touche que le haut du corps (les pieds restent au sol).
	const FVector Bounce(0.0, 0.0, LandingBounce);
	const FVector Waist = BoneLocation(Body, TEXT("spine_01"));
	const FQuat UpperRotation = TorsoQ * Hips.Inverse();
	auto Upper = [&](const TCHAR* Bone) { return Waist + Bounce + UpperRotation.RotateVector(BoneLocation(Body, Bone) - Waist); };

	// Rotation d'un os par rapport à sa pose de référence, exprimée dans le monde (pour orienter les pieds).
	const FReferenceSkeleton& RefSkeleton = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
	auto BoneDelta = [&](FName Bone)
	{
		const FQuat Ref = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Body->GetBoneIndex(Bone)).GetRotation();
		return Body->GetBoneQuaternion(Bone, EBoneSpaces::WorldSpace) * Ref.Inverse() * Body->GetComponentQuat().Inverse();
	};

	OutBlocks[Idx(EMechBlock::Pelvis)] = FTransform(Hips, BoneLocation(Body, TEXT("pelvis")) + Bounce * 0.5, Size(EMechBlock::Pelvis) / 100.0);

	const FVector TorsoCenter = (Upper(TEXT("spine_02")) + Upper(TEXT("spine_05"))) * 0.5;
	OutBlocks[Idx(EMechBlock::Torso)] = FTransform(TorsoQ, TorsoCenter, Size(EMechBlock::Torso) / 100.0);
	OutBlocks[Idx(EMechBlock::Backpack)] = FTransform(TorsoQ, TorsoCenter + TorsoQ.RotateVector(FVector(-225, 0, 30)), Size(EMechBlock::Backpack) / 100.0);

	// Cou : du haut du torse à la base de la tête (la caméra ne voit pas l'intérieur du torse).
	const FVector NeckBase = Upper(TEXT("neck_01"));
	const FVector HeadBase = Upper(TEXT("head"));
	OutBlocks[Idx(EMechBlock::Neck)] = LimbTransform(HeadBase, NeckBase, TorsoForward, Size(EMechBlock::Neck));

	const FQuat HeadQ = TorsoQ * FQuat(HeadLocal);
	OutBlocks[Idx(EMechBlock::Head)] = FTransform(HeadQ, HeadBase + HeadQ.RotateVector(FVector(0, 0, 75)), Size(EMechBlock::Head) / 100.0);
	OutCamera = FTransform(HeadQ, HeadBase + HeadQ.RotateVector(FVector(40, 0, 90)));

	struct FSide { const TCHAR* Suffix; EMechBlock Shoulder, Arm, Forearm, Hand, Thigh, Shin, Foot; };
	const FSide Sides[2] =
	{
		{ TEXT("_l"), EMechBlock::ShoulderL, EMechBlock::ArmL, EMechBlock::ForearmL, EMechBlock::HandL, EMechBlock::ThighL, EMechBlock::ShinL, EMechBlock::FootL },
		{ TEXT("_r"), EMechBlock::ShoulderR, EMechBlock::ArmR, EMechBlock::ForearmR, EMechBlock::HandR, EMechBlock::ThighR, EMechBlock::ShinR, EMechBlock::FootR },
	};

	for (const FSide& S : Sides)
	{
		auto Bone = [&](const TCHAR* Base) { return FName(FString(Base) + S.Suffix); };

		// Bras : pose animée du haut du corps.
		const FVector Shoulder = Upper(*Bone(TEXT("upperarm")).ToString());
		const FVector Elbow = Upper(*Bone(TEXT("lowerarm")).ToString());
		const FVector Wrist = Upper(*Bone(TEXT("hand")).ToString());
		const FVector ForearmDir = (Wrist - Elbow).GetSafeNormal();
		OutBlocks[Idx(S.Shoulder)] = FTransform(TorsoQ, Shoulder, Size(S.Shoulder) / 100.0);
		OutBlocks[Idx(S.Arm)] = LimbTransform(Shoulder, Elbow, TorsoForward, Size(S.Arm));
		OutBlocks[Idx(S.Forearm)] = LimbTransform(Elbow, Wrist, TorsoForward, Size(S.Forearm));
		OutBlocks[Idx(S.Hand)] = FTransform(FRotationMatrix::MakeFromZX(-ForearmDir, TorsoForward).ToQuat(), Wrist + ForearmDir * 45.0, Size(S.Hand) / 100.0);

		// Jambes : os simulés physiquement.
		const FVector Hip = BoneLocation(Body, Bone(TEXT("thigh")));
		const FVector Knee = BoneLocation(Body, Bone(TEXT("calf")));
		const FVector Ankle = BoneLocation(Body, Bone(TEXT("foot")));
		OutBlocks[Idx(S.Thigh)] = LimbTransform(Hip, Knee, HipsForward, Size(S.Thigh));
		OutBlocks[Idx(S.Shin)] = LimbTransform(Knee, Ankle, HipsForward, Size(S.Shin));
		const FQuat FootQ = BoneDelta(Bone(TEXT("foot"))) * Hips;
		OutBlocks[Idx(S.Foot)] = FTransform(FootQ, Ankle + FootQ.RotateVector(FVector(60, 0, -Size(S.Foot).Z * 0.5)), Size(S.Foot) / 100.0);
	}

	if (bFallen && FallAngle > 0.f)
	{
		const FQuat FallQ(FallAxis, FallAngle);
		auto Tilt = [&](FTransform& T)
		{
			T.SetLocation(FallPivot + FallQ.RotateVector(T.GetLocation() - FallPivot));
			T.SetRotation(FallQ * T.GetRotation());
		};
		for (FTransform& T : OutBlocks)
		{
			Tilt(T);
		}
		Tilt(OutCamera);
	}
}

void AMech::UpdateSupportPolygon()
{
	// Appui : pieds dont le bloc touche le sol.
	TArray<FVector2D> Corners;
	const EMechBlock FootBlocks[2] = { EMechBlock::FootL, EMechBlock::FootR };
	for (int32 i = 0; i < 2; ++i)
	{
		const FTransform& Foot = BlockMeshes[Idx(FootBlocks[i])]->GetComponentTransform();
		const FVector Half = Blocks[Idx(FootBlocks[i])].Size * 0.5;
		bFootPlanted[i] = !bAirborne && (Foot.GetLocation().Z - Half.Z) < GroundZ + 60.f;
		if (bFootPlanted[i])
		{
			for (const FVector2D& Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1), FVector2D(-1, 1) })
			{
				Corners.Add(FVector2D(Foot.GetLocation() + Foot.GetRotation().RotateVector(FVector(Half.X * Corner.X, Half.Y * Corner.Y, 0.0))));
			}
		}
	}

	TArray<int32> HullIndices;
	ConvexHull2D::ComputeConvexHull(Corners, HullIndices);
	SupportPolygon.Reset();
	for (int32 Index : HullIndices)
	{
		SupportPolygon.Add(Corners[Index]);
	}
}

void AMech::ApplyPose(float DeltaSeconds)
{
	// Les squelettes sont posés au sol sous le mecha, orientés comme les hanches (le squelette humain
	// regarde vers +Y). La racine avance exactement à la vitesse du mecha : les pieds animés ne glissent pas.
	GroundZ = TraceGroundZ(Position, GroundZ);
	const FVector SkeletonLocation(Position, GroundZ + Height);
	const FRotator SkeletonRotation(0.f, HipsYaw - 90.f, 0.f);
	BodySkeleton->SetWorldLocationAndRotation(SkeletonLocation, SkeletonRotation);
	GuideSkeleton->SetWorldLocationAndRotation(SkeletonLocation, SkeletonRotation);

	TArray<FTransform> Pose;
	FTransform CameraTransform;
	ComputePose(Pose, CameraTransform);

	SetActorLocation(Pose[Idx(EMechBlock::Pelvis)].GetLocation());

	// Centre de gravité global = moyenne des centres de gravité des blocs pondérée par leur masse.
	FVector WeightedSum = FVector::ZeroVector;
	float TotalMass = 0.f;
	for (int32 i = 0; i < NumBlocks; ++i)
	{
		const FTransform& T = Pose[i];
		BlockMeshes[i]->SetWorldTransform(T);
		WeightedSum += (T.GetLocation() + T.GetRotation().RotateVector(Blocks[i].CenterOfGravity)) * Blocks[i].MassKg;
		TotalMass += Blocks[i].MassKg;
	}
	UpdateSupportPolygon();

	if (bExternalView)
	{
		// Vue de debug (touche V) : troisième personne derrière le buste, légèrement décalée, visant le bassin.
		const FVector Target = Pose[Idx(EMechBlock::Pelvis)].GetLocation();
		const FVector Eye = Target + YawQuat(TorsoYaw).RotateVector(FVector(-2600, -900, 800));
		Camera->SetWorldLocationAndRotation(Eye, (Target - Eye).Rotation());
	}
	else
	{
		Camera->SetWorldLocationAndRotation(CameraTransform.GetLocation(), CameraTransform.GetRotation());
	}

	if (TotalMass > 0.f)
	{
		CoGWorld = WeightedSum / TotalMass;
		if (!bFallen)
		{
			CoGOffsetXY = FVector2D(CoGWorld) - Position;
			CoMHeight = FMath::Max(CoGWorld.Z - GroundZ, 200.f);
		}
	}
}

void AMech::SetExternalView(bool bExternal)
{
	bExternalView = bExternal;
	for (UStaticMeshComponent* Mesh : CockpitMeshes)
	{
		Mesh->SetVisibility(!bExternal);
	}
	BlockMeshes[Idx(EMechBlock::Head)]->SetOwnerNoSee(!bExternal);
}

void AMech::SetGuideVisible(bool bVisible)
{
	GuideSkeleton->SetVisibility(bVisible);
}

bool AMech::IsGuideVisible() const
{
	return GuideSkeleton->IsVisible();
}

void AMech::DrawDebug() const
{
	UWorld* World = GetWorld();
	const float Z = GroundZ + 5.f;
	for (int32 i = 0; i < SupportPolygon.Num(); ++i)
	{
		DrawDebugLine(World, FVector(SupportPolygon[i], Z), FVector(SupportPolygon[(i + 1) % SupportPolygon.Num()], Z), FColor::Yellow, false, 0.f, 0, 4.f);
	}
	DrawDebugSphere(World, FVector(FVector2D(CoGWorld), Z), 25.f, 8, FColor::Green);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::Green, FString::Printf(TEXT("Deplacement %.2f %.2f | Joystick %.2f %.2f %.2f"),
			MoveInput.X, MoveInput.Y, JoystickInput.X, JoystickInput.Y, JoystickInput.Z));
		GEngine->AddOnScreenDebugMessage(9002, 0.f, FColor::Green, FString::Printf(TEXT("Vitesse %.1f km/h (marche max %.1f) | pas %.2f s | anim vitesse %.0f dir %.0f x%.2f | buste/hanches %.0f deg"),
			Velocity.Size() * 0.036f, GetMaxWalkSpeed() * 0.036f, GetStepPeriod(), AnimSpeed, AnimDirection, AnimPlayRate, FMath::FindDeltaAngleDegrees(HipsYaw, TorsoYaw)));
		GEngine->AddOnScreenDebugMessage(9006, 0.f, FColor::Cyan, FString::Printf(TEXT("Jambes %s | force %.0f amort. %.0f | ecart pieds/guide %.0f cm"),
			bSimulateLegs ? TEXT("simulees") : TEXT("animees"), LegOrientationStrength, LegAngularVelocityStrength, GetLegDeviation()));
		GEngine->AddOnScreenDebugMessage(9004, 0.f, FColor::Cyan, FString::Printf(TEXT("Reacteurs avant %.0f%% vertical %.0f%% lateral %.0f%% | %s | altitude %.1f m | torse %s"),
			ThrustForward * 100.f, ThrustVertical * 100.f, ThrustLateral * 100.f, bAirborne ? TEXT("porte") : TEXT("au sol"), Height / 100.f,
			bTorsoLocked ? TEXT("verrouille") : TEXT("libre")));
	}
}

UMechRoutine* AMech::FindRoutine(const FString& Name) const
{
	for (UMechRoutine* Routine : Routines)
	{
		if (Routine->RoutineName.Equals(Name, ESearchCase::IgnoreCase))
		{
			return Routine;
		}
	}
	return nullptr;
}

FString AMech::GetStatusText() const
{
	const float Speed = Velocity.Size();
	const TCHAR* State = bFallen ? TEXT("CHUTE")
		: bAirborne ? TEXT("VOL")
		: Speed > GetMaxWalkSpeed() * 1.05f ? TEXT("COURSE")
		: Speed > 15.f ? TEXT("MARCHE") : TEXT("DEBOUT");
	return FString::Printf(TEXT("%s%s | %.1f km/h | alt %.1f m | %.1f t | CdG %.1f m"),
		State, bTorsoLocked ? TEXT(" VERROU") : TEXT(""), Speed * 0.036f, Height / 100.f, GetTotalMassKg() / 1000.f, CoMHeight / 100.f);
}

UMechTerminalScreenWidget* AMech::GetTerminalScreen() const
{
	return TerminalScreen ? Cast<UMechTerminalScreenWidget>(TerminalScreen->GetUserWidgetObject()) : nullptr;
}
