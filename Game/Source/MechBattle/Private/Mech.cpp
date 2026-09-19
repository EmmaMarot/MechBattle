#include "Mech.h"

#include "Algo/Sort.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeomTools.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/ConvexHull2d.h"
#include "MechRoutines.h"
#include "MechTerminalWidgets.h"
#include "TwoBoneIK.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Gravity = 980.f;
	constexpr float PhysicsStep = 1.f / 120.f;
	constexpr int32 NumBlocks = static_cast<int32>(EMechBlock::Count);

	const TCHAR* BlockNames[NumBlocks] =
	{
		TEXT("Torse"), TEXT("Tete"), TEXT("Backpack"), TEXT("Bassin"),
		TEXT("EpauleG"), TEXT("EpauleD"), TEXT("BrasG"), TEXT("BrasD"), TEXT("AvantBrasG"), TEXT("AvantBrasD"), TEXT("MainG"), TEXT("MainD"),
		TEXT("CuisseG"), TEXT("CuisseD"), TEXT("JambeG"), TEXT("JambeD"), TEXT("PiedG"), TEXT("PiedD")
	};

	int32 Idx(EMechBlock Block) { return static_cast<int32>(Block); }

	/** Bloc allongé entre deux articulations : axe Z du bloc de B vers A. */
	FTransform LimbTransform(const FVector& A, const FVector& B, const FVector& ForwardHint)
	{
		return FTransform(FRotationMatrix::MakeFromZX(A - B, ForwardHint).ToQuat(), (A + B) * 0.5);
	}

	/** Direction d'un segment de bras pendant, incliné vers l'avant de AngleDeg (repère torse). */
	FVector HangingDirection(float AngleDeg)
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		return FVector(FMath::Sin(Rad), 0.0, -FMath::Cos(Rad));
	}
}

AMech::AMech()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BlockMaterial = ShapeMaterial.Object;

	BuildBlockDefs();

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

	Set(EMechBlock::Torso,    FVector(300, 460, 400), 18000.f, Frame);
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

	UMechBalanceRoutine* BalanceRoutine = NewObject<UMechBalanceRoutine>(this);
	BalanceRoutine->Activate();
	Routines.Add(BalanceRoutine);

	UMechHeadAimRoutine* HeadRoutine = NewObject<UMechHeadAimRoutine>(this);
	HeadRoutine->Activate();
	Routines.Add(HeadRoutine);

	Yaw0 = GetActorRotation().Yaw;
	const FVector Start = GetActorLocation();
	GroundZ = TraceGroundZ(FVector2D(Start), Start.Z);
	InitStance(FVector2D(Start));
}

void AMech::InitStance(const FVector2D& Center)
{
	const FQuat YawQ(FRotator(0.f, Yaw0, 0.f));
	const FVector Right = YawQ.GetRightVector();

	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = (i == 0) ? -1.f : 1.f;
		const FVector2D XY = Center + FVector2D(Right) * (Side * 110.f);
		Feet[i] = FMechFoot();
		Feet[i].Pos = FVector(XY, TraceGroundZ(XY, GroundZ));
	}

	CoM = Center;
	Velocity = FVector2D::ZeroVector;
	CoGOffsetXY = FVector2D::ZeroVector;
	TorsoLean = FRotator::ZeroRotator;
	PelvisZ = GroundZ + AnkleHeight + StandLegHeight + 60.f;
	PelvisBounce = PelvisBounceVel = 0.f;
	bFallen = false;
	FallAngle = FallRate = 0.f;
	LastSwingFoot = INDEX_NONE;
	TimeSinceLanding = 1.f;
	UpdateSupportPolygon();

	// Deux passes pour caler le décalage centre de gravité / bassin.
	ApplyPose(0.f);
	ApplyPose(0.f);
	RefreshBalanceState();
}

void AMech::ResetStance()
{
	InitStance(CoM);
}

void AMech::SetPilotInputs(const FVector2D& InGyro, const FVector& InHead)
{
	GyroInput = InGyro.ClampAxes(-1.0, 1.0);
	HeadInput = InHead.BoundToCube(1.0);
}

void AMech::SetZmpTarget(const FVector2D& Target)
{
	ZmpTarget = Target;
	bHasZmpTarget = true;
}

bool AMech::StartStep(int32 FootIndex, const FVector2D& Target, float Duration)
{
	if (bFallen || Feet[0].bSwing || Feet[1].bSwing || FootIndex < 0 || FootIndex > 1)
	{
		return false;
	}

	// Limites des articulations de la jambe, dans le repère du pied d'appui.
	const FMechFoot& Stance = Feet[1 - FootIndex];
	const FVector2D Forward = Balance.Forward;
	const FVector2D Right = Balance.Right;
	const FVector2D Delta = Target - FVector2D(Stance.Pos);
	const float Side = (FootIndex == 0) ? -1.f : 1.f;

	const float Along = FMath::Clamp(FVector2D::DotProduct(Delta, Forward), -MaxStepLength, MaxStepLength);
	const float Lateral = Side * FMath::Clamp(Side * FVector2D::DotProduct(Delta, Right), MinFootSpacing, MaxStepWidth);
	const FVector2D Landing = FVector2D(Stance.Pos) + Forward * Along + Right * Lateral;

	FMechFoot& Foot = Feet[FootIndex];
	Foot.bSwing = true;
	Foot.SwingStart = Foot.Pos;
	Foot.SwingTarget = FVector(Landing, TraceGroundZ(Landing, Foot.Pos.Z));
	Foot.SwingTime = 0.f;
	Foot.SwingDuration = FMath::Max(Duration, 0.2f);
	UpdateSupportPolygon();
	return true;
}

void AMech::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Min(DeltaSeconds, 1.f / 20.f);

	// Gyroscope principal (entrée directe, priorité 1000) : incline le torse et déséquilibre le mecha.
	if (!bFallen)
	{
		const FRotator TargetLean(-GyroInput.Y * MaxTorsoLean, 0.f, GyroInput.X * MaxTorsoLean);
		TorsoLean = FMath::RInterpConstantTo(TorsoLean, TargetLean, Dt, GyroLeanRate);
		ExternalAccel = (Balance.Forward * GyroInput.Y + Balance.Right * GyroInput.X) * MaxGyroAccel;
	}
	else
	{
		ExternalAccel = FVector2D::ZeroVector;
	}

	RefreshBalanceState();

	// Routines par ordre de priorité.
	Routines.StableSort([](const UMechRoutine& A, const UMechRoutine& B) { return A.Priority < B.Priority; });
	for (UMechRoutine* Routine : Routines)
	{
		Routine->TickRoutine(this, Dt);
	}

	if (bFallen)
	{
		StepFall(Dt);
	}
	else
	{
		PhysicsAccumulator += Dt;
		while (PhysicsAccumulator >= PhysicsStep && !bFallen)
		{
			StepPhysics(PhysicsStep);
			PhysicsAccumulator -= PhysicsStep;
		}
	}
	bHasZmpTarget = false;

	HeadLocal = FMath::RInterpConstantTo(HeadLocal, HeadTarget, Dt, NeckSpeed);

	ApplyPose(Dt);

	if (bDebugDraw)
	{
		DrawDebug();
	}
}

void AMech::RefreshBalanceState()
{
	const FQuat YawQ(FRotator(0.f, Yaw0, 0.f));
	Balance.CoM = CoM;
	Balance.Velocity = Velocity;
	Balance.ExternalAccel = ExternalAccel;
	Balance.SupportCenter = SupportCenter;
	Balance.SupportPolygon = SupportPolygon;
	Balance.Forward = FVector2D(YawQ.GetForwardVector()).GetSafeNormal();
	Balance.Right = FVector2D(YawQ.GetRightVector()).GetSafeNormal();
	Balance.Omega = Omega;
	Balance.CoMHeight = CoMHeight;
	Balance.TimeSinceLanding = TimeSinceLanding;
	Balance.LastSwingFoot = LastSwingFoot;
	Balance.bFallen = bFallen;
	for (int32 i = 0; i < 2; ++i)
	{
		Balance.FootPos[i] = FVector2D(Feet[i].Pos);
		Balance.bFootSwing[i] = Feet[i].bSwing;
	}
	Balance.bAnySwing = Feet[0].bSwing || Feet[1].bSwing;
}

void AMech::UpdateSupportPolygon()
{
	const FVector2D Forward = FVector2D(FQuat(FRotator(0.f, Yaw0, 0.f)).GetForwardVector());
	const FVector2D Right = FVector2D(FQuat(FRotator(0.f, Yaw0, 0.f)).GetRightVector());
	const FVector2D HalfLength = Forward * (Blocks[Idx(EMechBlock::FootL)].Size.X * 0.5);
	const FVector2D HalfWidth = Right * (Blocks[Idx(EMechBlock::FootL)].Size.Y * 0.5);

	TArray<FVector2D> Corners;
	for (const FMechFoot& Foot : Feet)
	{
		if (!Foot.bSwing)
		{
			const FVector2D C(Foot.Pos);
			Corners.Add(C + HalfLength + HalfWidth);
			Corners.Add(C + HalfLength - HalfWidth);
			Corners.Add(C - HalfLength - HalfWidth);
			Corners.Add(C - HalfLength + HalfWidth);
		}
	}

	TArray<int32> HullIndices;
	ConvexHull2D::ComputeConvexHull(Corners, HullIndices);

	SupportPolygon.Reset();
	SupportCenter = FVector2D::ZeroVector;
	for (int32 Index : HullIndices)
	{
		SupportPolygon.Add(Corners[Index]);
		SupportCenter += Corners[Index];
	}
	if (SupportPolygon.Num() > 0)
	{
		SupportCenter /= SupportPolygon.Num();
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

FVector2D AMech::ClosestPointInSupport(const FVector2D& Point, float& OutDistance) const
{
	return ClosestPointInPolygon(Point, SupportPolygon, OutDistance);
}

FVector2D AMech::ClosestPointOnFoot(int32 FootIndex, const FVector2D& Point) const
{
	const FVector2D Local = Point - FVector2D(Feet[FootIndex].Pos);
	const float HalfLength = Blocks[Idx(EMechBlock::FootL)].Size.X * 0.5f;
	const float HalfWidth = Blocks[Idx(EMechBlock::FootL)].Size.Y * 0.5f;
	const float Along = FMath::Clamp(FVector2D::DotProduct(Local, Balance.Forward), -HalfLength, HalfLength);
	const float Lateral = FMath::Clamp(FVector2D::DotProduct(Local, Balance.Right), -HalfWidth, HalfWidth);
	return FVector2D(Feet[FootIndex].Pos) + Balance.Forward * Along + Balance.Right * Lateral;
}

void AMech::StepPhysics(float Dt)
{
	// Pendule inversé linéaire : le centre de gravité accélère en s'éloignant du point de pression (ZMP),
	// qui ne peut pas sortir du polygone d'appui.
	const float Omega2 = Omega * Omega;
	const FVector2D Desired = bHasZmpTarget
		? ZmpTarget
		// Sans routine d'équilibre, les stabilisateurs tiennent les jambes raides.
		: CoM + (ExternalAccel + Velocity * PassiveDamping) / Omega2;

	float Unused = 0.f;
	Zmp = ClosestPointInPolygon(Desired, SupportPolygon, Unused);

	const FVector2D Accel = (CoM - Zmp) * Omega2 + ExternalAccel;
	Velocity += Accel * Dt;
	CoM += Velocity * Dt;

	TimeSinceLanding += Dt;

	for (int32 i = 0; i < 2; ++i)
	{
		FMechFoot& Foot = Feet[i];
		if (!Foot.bSwing)
		{
			continue;
		}

		Foot.SwingTime += Dt;
		const float Alpha = FMath::Clamp(Foot.SwingTime / Foot.SwingDuration, 0.f, 1.f);
		const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha);
		Foot.Pos = FMath::Lerp(Foot.SwingStart, Foot.SwingTarget, Smooth);
		Foot.Pos.Z += FMath::Sin(Alpha * UE_PI) * SwingHeight;

		if (Alpha >= 1.f)
		{
			Foot.Pos = Foot.SwingTarget;
			Foot.bSwing = false;
			LastSwingFoot = i;
			TimeSinceLanding = 0.f;
			++StepCount;
			PelvisBounceVel -= 70.f;
			UpdateSupportPolygon();
		}
	}

	// Chute : le centre de gravité est sorti du polygone d'appui. Pendant un pas, il dépasse
	// normalement le pied d'appui : seul un écart hors de portée du pied en l'air fait chuter.
	const bool bSwinging = Feet[0].bSwing || Feet[1].bSwing;
	float OutsideDistance = 0.f;
	ClosestPointInPolygon(CoM, SupportPolygon, OutsideDistance);
	if (OutsideDistance > (bSwinging ? FallMargin + MaxStepLength : FallMargin))
	{
		StartFall();
	}
}

void AMech::StartFall()
{
	float Distance = 0.f;
	const FVector2D Edge = ClosestPointInPolygon(CoM, SupportPolygon, Distance);
	FVector2D Direction = (CoM - Edge).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Velocity.GetSafeNormal();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = Balance.Forward;
	}

	bFallen = true;
	FallPivot = FVector(Edge, GroundZ);
	FallAxis = FVector::CrossProduct(FVector::UpVector, FVector(Direction, 0.0)).GetSafeNormal();
	FallAngle = 0.f;
	FallOffsetAngle = FMath::Atan2(Distance, FMath::Max(CoMHeight, 100.f));
	FallRate = Velocity.Size() / FMath::Max(CoMHeight, 100.f);
	Velocity = FVector2D::ZeroVector;
	for (FMechFoot& Foot : Feet)
	{
		Foot.bSwing = false;
	}
}

void AMech::StepFall(float Dt)
{
	const float MaxAngle = FMath::DegreesToRadians(82.f);
	if (FallAngle >= MaxAngle)
	{
		return;
	}
	FallRate += Omega * Omega * FMath::Sin(FallAngle + FallOffsetAngle) * Dt;
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

void AMech::ComputePose(TArray<FTransform>& OutBlocks, FTransform& OutCamera) const
{
	OutBlocks.SetNum(NumBlocks);

	const FQuat YawQ(FRotator(0.f, Yaw0, 0.f));
	const FVector Forward = YawQ.GetForwardVector();
	const FQuat TorsoQ = YawQ * FQuat(FRotator(TorsoLean.Pitch, 0.f, TorsoLean.Roll));
	const FVector TorsoForward = TorsoQ.GetForwardVector();

	const FVector PelvisPos(PelvisXY(), PelvisZ + PelvisBounce);
	OutBlocks[Idx(EMechBlock::Pelvis)] = FTransform(YawQ, PelvisPos);

	const FVector Waist = PelvisPos + FVector(0, 0, 75);
	OutBlocks[Idx(EMechBlock::Torso)] = FTransform(TorsoQ, Waist + TorsoQ.RotateVector(FVector(0, 0, 200)));
	OutBlocks[Idx(EMechBlock::Backpack)] = FTransform(TorsoQ, Waist + TorsoQ.RotateVector(FVector(-225, 0, 230)));

	const FVector Neck = Waist + TorsoQ.RotateVector(FVector(0, 0, 400));
	const FQuat HeadQ = TorsoQ * FQuat(HeadLocal);
	const FTransform HeadTransform(HeadQ, Neck + HeadQ.RotateVector(FVector(0, 0, 85)));
	OutBlocks[Idx(EMechBlock::Head)] = HeadTransform;
	OutCamera = FTransform(HeadQ, HeadTransform.TransformPosition(FVector(40, 0, 15)));

	const EMechBlock Shoulders[2] = { EMechBlock::ShoulderL, EMechBlock::ShoulderR };
	const EMechBlock Arms[2] = { EMechBlock::ArmL, EMechBlock::ArmR };
	const EMechBlock Forearms[2] = { EMechBlock::ForearmL, EMechBlock::ForearmR };
	const EMechBlock Hands[2] = { EMechBlock::HandL, EMechBlock::HandR };
	const EMechBlock Thighs[2] = { EMechBlock::ThighL, EMechBlock::ThighR };
	const EMechBlock Shins[2] = { EMechBlock::ShinL, EMechBlock::ShinR };
	const EMechBlock FootBlocks[2] = { EMechBlock::FootL, EMechBlock::FootR };

	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = (i == 0) ? -1.f : 1.f;

		// Bras : pendent sous l'épaule, balancent en opposition des jambes.
		const FVector ShoulderCenter = Waist + TorsoQ.RotateVector(FVector(0, Side * 300, 330));
		OutBlocks[Idx(Shoulders[i])] = FTransform(TorsoQ, ShoulderCenter);
		const FVector ShoulderJoint = ShoulderCenter + TorsoQ.RotateVector(FVector(0, Side * 10, -40));
		const FVector UpperDir = TorsoQ.RotateVector(HangingDirection(5.f + ArmSwing[i]));
		const FVector LowerDir = TorsoQ.RotateVector(HangingDirection(35.f + ArmSwing[i]));
		const FVector Elbow = ShoulderJoint + UpperDir * 300.0;
		const FVector Wrist = Elbow + LowerDir * 300.0;
		OutBlocks[Idx(Arms[i])] = LimbTransform(ShoulderJoint, Elbow, TorsoForward);
		OutBlocks[Idx(Forearms[i])] = LimbTransform(Elbow, Wrist, TorsoForward);
		OutBlocks[Idx(Hands[i])] = FTransform(FRotationMatrix::MakeFromZX(-LowerDir, TorsoForward).ToQuat(), Wrist + LowerDir * 45.0);

		// Jambes : IK à deux segments (AnimationCore), genou vers l'avant.
		const FVector Hip = PelvisPos + YawQ.RotateVector(FVector(0, Side * HipHalfWidth, -60));
		const FVector Ankle = Feet[i].Pos + FVector(0, 0, AnkleHeight);
		const FVector KneeGuess = (Hip + Ankle) * 0.5 + Forward * 100.0;
		const FVector Pole = (Hip + Ankle) * 0.5 + Forward * 500.0;
		FVector Knee, AnkleSolved;
		AnimationCore::SolveTwoBoneIK(Hip, KneeGuess, Ankle, Pole, Ankle, Knee, AnkleSolved, ThighLength, ShinLength, false, 1.0, 1.0);

		OutBlocks[Idx(Thighs[i])] = LimbTransform(Hip, Knee, Forward);
		OutBlocks[Idx(Shins[i])] = LimbTransform(Knee, AnkleSolved, Forward);
		OutBlocks[Idx(FootBlocks[i])] = FTransform(YawQ, Feet[i].Pos + FVector(0, 0, AnkleHeight * 0.5));
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

void AMech::ApplyPose(float DeltaSeconds)
{
	// Hauteur du bassin : position debout, abaissée si une jambe posée n'atteint plus son pied.
	if (!bFallen)
	{
		const FQuat YawQ(FRotator(0.f, Yaw0, 0.f));
		const float LegLength = (ThighLength + ShinLength) * 0.985f;
		float PlantedZ = 0.f;
		int32 Planted = 0;
		float HipZ = TNumericLimits<float>::Max();
		for (int32 i = 0; i < 2; ++i)
		{
			if (Feet[i].bSwing)
			{
				continue;
			}
			const float Side = (i == 0) ? -1.f : 1.f;
			const FVector2D HipXY = PelvisXY() + FVector2D(YawQ.GetRightVector()) * (Side * HipHalfWidth);
			const float Horizontal = FVector2D::Distance(HipXY, FVector2D(Feet[i].Pos));
			const float AnkleZ = Feet[i].Pos.Z + AnkleHeight;
			HipZ = FMath::Min(HipZ, AnkleZ + FMath::Sqrt(FMath::Max(0.f, FMath::Square(LegLength) - FMath::Square(Horizontal))));
			PlantedZ += Feet[i].Pos.Z;
			++Planted;
		}
		if (Planted > 0)
		{
			GroundZ = PlantedZ / Planted;
			HipZ = FMath::Min(HipZ, GroundZ + AnkleHeight + StandLegHeight);
			const float TargetPelvisZ = HipZ + 60.f;
			PelvisZ = (DeltaSeconds > 0.f) ? FMath::FInterpTo(PelvisZ, TargetPelvisZ, DeltaSeconds, 6.f) : TargetPelvisZ;
		}

		// Petit rebond du bassin à chaque appui.
		PelvisBounceVel += (-120.f * PelvisBounce - 14.f * PelvisBounceVel) * DeltaSeconds;
		PelvisBounce += PelvisBounceVel * DeltaSeconds;

		// Balancement des bras en opposition du pied en l'air.
		for (int32 i = 0; i < 2; ++i)
		{
			float Target = 0.f;
			const FMechFoot& Other = Feet[1 - i];
			if (Other.bSwing)
			{
				Target = 15.f * FMath::Sin(FMath::Clamp(Other.SwingTime / Other.SwingDuration, 0.f, 1.f) * UE_PI);
			}
			ArmSwing[i] = (DeltaSeconds > 0.f) ? FMath::FInterpTo(ArmSwing[i], Target, DeltaSeconds, 8.f) : Target;
		}
	}

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
		BlockMeshes[i]->SetWorldTransform(FTransform(T.GetRotation(), T.GetLocation(), Blocks[i].Size / 100.0));
		WeightedSum += T.TransformPosition(Blocks[i].CenterOfGravity) * Blocks[i].MassKg;
		TotalMass += Blocks[i].MassKg;
	}
	Camera->SetWorldLocationAndRotation(CameraTransform.GetLocation(), CameraTransform.GetRotation());

	if (!bFallen && TotalMass > 0.f)
	{
		const FVector CoG = WeightedSum / TotalMass;
		CoGOffsetXY = FVector2D(CoG) - PelvisXY();
		CoMHeight = FMath::Max(CoG.Z - GroundZ, 200.f);
		Omega = FMath::Sqrt(Gravity / CoMHeight);
	}
}

void AMech::DrawDebug() const
{
	UWorld* World = GetWorld();
	const float Z = GroundZ + 5.f;
	for (int32 i = 0; i < SupportPolygon.Num(); ++i)
	{
		DrawDebugLine(World, FVector(SupportPolygon[i], Z), FVector(SupportPolygon[(i + 1) % SupportPolygon.Num()], Z), FColor::Yellow, false, 0.f, 0, 4.f);
	}
	const FVector2D Capture = CoM + Velocity / Omega + ExternalAccel / (Omega * Omega);
	DrawDebugSphere(World, FVector(CoM, Z), 25.f, 8, FColor::Green);
	DrawDebugSphere(World, FVector(Zmp, Z), 20.f, 8, FColor::Blue);
	DrawDebugSphere(World, FVector(Capture, Z), 25.f, 8, FColor::Red);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::Green, FString::Printf(TEXT("Gyro %.2f %.2f | Tete %.2f %.2f %.2f"), GyroInput.X, GyroInput.Y, HeadInput.X, HeadInput.Y, HeadInput.Z));
		GEngine->AddOnScreenDebugMessage(9002, 0.f, FColor::Green, FString::Printf(TEXT("Vitesse %.1f km/h | Pas %d | Hauteur CdG %.1f m"), Velocity.Size() * 0.036f, StepCount, CoMHeight / 100.f));
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
	const TCHAR* State = bFallen ? TEXT("CHUTE") : ((Feet[0].bSwing || Feet[1].bSwing) ? TEXT("MARCHE") : TEXT("DEBOUT"));
	float TotalMass = 0.f;
	for (const FMechBlockDef& Def : Blocks)
	{
		TotalMass += Def.MassKg;
	}
	return FString::Printf(TEXT("ETAT %s | %.1f km/h | %d pas | masse %.1f t | CdG %.1f m"),
		State, Velocity.Size() * 0.036f, StepCount, TotalMass / 1000.f, CoMHeight / 100.f);
}

UMechTerminalScreenWidget* AMech::GetTerminalScreen() const
{
	return TerminalScreen ? Cast<UMechTerminalScreenWidget>(TerminalScreen->GetUserWidgetObject()) : nullptr;
}
