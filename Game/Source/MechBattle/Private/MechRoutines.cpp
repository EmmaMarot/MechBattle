#include "MechRoutines.h"

#include "Mech.h"

UMechBalanceRoutine::UMechBalanceRoutine()
{
	RoutineName = TEXT("EQUILIBRE");
}

void UMechBalanceRoutine::Process(AMech* Mech, float DeltaSeconds)
{
	const FMechBalanceState& S = Mech->GetBalanceState();
	if (S.bFallen)
	{
		return;
	}

	// Point de capture : là où il faudrait poser le point de pression pour s'arrêter,
	// décalé par la poussée du gyroscope principal.
	const float W = S.Omega;
	const FVector2D Capture = S.CoM + S.Velocity / W + S.ExternalAccel / (W * W);

	// Stratégie de cheville : ramène le point de capture vers le centre de l'appui.
	Mech->SetZmpTarget(Capture + (Capture - S.SupportCenter) * CaptureGain);

	if (S.bAnySwing || S.TimeSinceLanding < MinDoubleSupport)
	{
		return;
	}

	float Outside = 0.f;
	Mech->ClosestPointInSupport(Capture, Outside);
	if (Outside < StepTrigger)
	{
		return;
	}

	// Choix du pied : côté du déséquilibre en latéral, sinon le pied le plus éloigné (en alternant).
	const FVector2D Offset = Capture - (S.FootPos[0] + S.FootPos[1]) * 0.5;
	const float Along = FVector2D::DotProduct(Offset, S.Forward);
	const float Lateral = FVector2D::DotProduct(Offset, S.Right);
	const float Distance[2] = { FVector2D::Distance(Capture, S.FootPos[0]), FVector2D::Distance(Capture, S.FootPos[1]) };
	const float FeetSpacing = FMath::Abs(FVector2D::DotProduct(S.FootPos[1] - S.FootPos[0], S.Right));

	int32 Foot;
	if (FMath::Abs(Lateral) > 0.5f * FMath::Abs(Along))
	{
		Foot = (Lateral > 0.f) ? 1 : 0;
		// Pas chassé : si ce pied vient de s'écarter, l'autre le rejoint.
		if (Foot == S.LastSwingFoot && FeetSpacing > Mech->MinFootSpacing + 100.f)
		{
			Foot = 1 - Foot;
		}
	}
	else
	{
		Foot = (Distance[0] > Distance[1]) ? 0 : 1;
		if (Foot == S.LastSwingFoot && Distance[1 - Foot] > 0.6f * Distance[Foot])
		{
			Foot = 1 - Foot;
		}
	}

	// Prédiction du point de capture à la fin du pas, pendant l'appui sur l'autre pied.
	const FVector2D StanceZmp = Mech->ClosestPointOnFoot(1 - Foot, Capture);
	const FVector2D Predicted = StanceZmp + (Capture - StanceZmp) * FMath::Exp(W * StepDuration);
	const float Side = (Foot == 0) ? -1.f : 1.f;

	Mech->StartStep(Foot, Predicted + S.Right * (Side * LateralNudge), StepDuration);
}

UMechHeadAimRoutine::UMechHeadAimRoutine()
{
	RoutineName = TEXT("VISEE_TETE");
}

void UMechHeadAimRoutine::Process(AMech* Mech, float DeltaSeconds)
{
	// Stick avant = tête qui regarde vers le bas, torsion = lacet, inclinaison latérale = roulis.
	const FVector& Stick = Mech->GetHeadInput();
	Mech->SetHeadTarget(FRotator(-Stick.Y * MaxPitch, Stick.Z * MaxYaw, Stick.X * MaxRoll));
}
