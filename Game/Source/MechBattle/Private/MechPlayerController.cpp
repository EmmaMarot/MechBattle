#include "MechPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mech.h"
#include "MechRoutine.h"
#include "MechTerminalWidgets.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

namespace MechHotas
{
	// Plugin RawInput (voir DefaultInput.ini) : T.16000M -> MechHotas_Axis1..8, TWCS -> MechHotas_Axis9..16,
	// dans l'ordre des "value caps" HID du périphérique (touches analogiques déclarées dans MechBattle.cpp).
	// Relevé avec "showdebug rawinput" (usage HID entre parenthèses) :
	// TWCS : Axis9 Dial(55), Axis10 Slider(54), Axis11 Ry(52), Axis12 Rx(51), Axis13 Rz(53), Axis14 Y(49), Axis15 X(48), Axis16 Z(50).
	// T.16000M : Axis1 chapeau POV(57), Axis2 X(48), Axis3 Y(49), Axis4 Slider(54), Axis5 Rz/torsion(53).
	FKey JoystickX()     { return FKey(TEXT("MechHotas_Axis2")); }
	FKey JoystickY()     { return FKey(TEXT("MechHotas_Axis3")); }
	FKey JoystickTwist() { return FKey(TEXT("MechHotas_Axis5")); }
	FKey ThrottleMiniX() { return FKey(TEXT("MechHotas_Axis15")); }
	FKey ThrottleMiniY() { return FKey(TEXT("MechHotas_Axis14")); }
	FKey ThrottleMain()  { return FKey(TEXT("MechHotas_Axis16")); }	// Z : gaz principaux
	FKey ThrottleDial()  { return FKey(TEXT("MechHotas_Axis9")); }	// Slider 0 : molette "antenne"
	FKey ThrottleRocker(){ return FKey(TEXT("MechHotas_Axis13")); }	// Rz : bascule
	// Boutons TWCS : GenericUSBController_Button17..30 dans l'ordre HID (B1 = 17).
	FKey ThrottlePinky() { return FKey(TEXT("GenericUSBController_Button18")); }	// B2 auriculaire
	FKey ThrottleRing()  { return FKey(TEXT("GenericUSBController_Button19")); }	// B3 annulaire

	constexpr int32 NumRawAxes = 16;
	constexpr int32 VisibleLines = 18;
	constexpr int32 MaxStoredLines = 200;
}

namespace
{
	void AddDeadZone(UObject* Outer, FEnhancedActionKeyMapping& Mapping, float Threshold)
	{
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Outer);
		DeadZone->LowerThreshold = Threshold;
		DeadZone->Type = EDeadZoneType::Axial;
		Mapping.Modifiers.Add(DeadZone);
	}

	void AddSwizzle(UObject* Outer, FEnhancedActionKeyMapping& Mapping, EInputAxisSwizzle Order)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
		Swizzle->Order = Order;
		Mapping.Modifiers.Add(Swizzle);
	}

	void AddNegate(UObject* Outer, FEnhancedActionKeyMapping& Mapping, bool bX, bool bY, bool bZ)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer);
		Negate->bX = bX;
		Negate->bY = bY;
		Negate->bZ = bZ;
		Mapping.Modifiers.Add(Negate);
	}
}

void AMechPlayerController::CreateInputObjects()
{
	if (PilotContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Deplacement"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	JoystickAction = NewObject<UInputAction>(this, TEXT("IA_Joystick"));
	JoystickAction->ValueType = EInputActionValueType::Axis3D;

	TerminalAction = NewObject<UInputAction>(this, TEXT("IA_Terminal"));
	TerminalAction->ValueType = EInputActionValueType::Boolean;

	PilotContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Pilote"));

	// Déplacement : mini-stick du TWCS (X = latéral droite, Y = avant ; le Y brut est déjà positif vers l'avant).
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(MoveAction, MechHotas::ThrottleMiniX());
		AddDeadZone(this, Mapping, 0.08f);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(MoveAction, MechHotas::ThrottleMiniY());
		AddDeadZone(this, Mapping, 0.08f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
	}

	// Secours clavier pour le déplacement (flèches), utile sans HOTAS.
	PilotContext->MapKey(MoveAction, EKeys::Right);
	AddNegate(this, PilotContext->MapKey(MoveAction, EKeys::Left), true, false, false);
	AddSwizzle(this, PilotContext->MapKey(MoveAction, EKeys::Up), EInputAxisSwizzle::YXZ);
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(MoveAction, EKeys::Down);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
		AddNegate(this, Mapping, false, true, false);
	}

	// Joystick : X = rotation du buste (vitesse), Y = tête haut/bas (position absolue), torsion = tête gauche/droite.
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(JoystickAction, MechHotas::JoystickX());
		AddDeadZone(this, Mapping, 0.05f);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(JoystickAction, MechHotas::JoystickY());
		AddDeadZone(this, Mapping, 0.02f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
		AddNegate(this, Mapping, false, true, false);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(JoystickAction, MechHotas::JoystickTwist());
		AddDeadZone(this, Mapping, 0.04f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::ZYX);
	}

	// Secours clavier pour le buste (A / E).
	AddNegate(this, PilotContext->MapKey(JoystickAction, EKeys::A), true, false, false);
	PilotContext->MapKey(JoystickAction, EKeys::E);
	// Réacteurs dorsaux : X = gaz (Z), Y = molette (Slider 0), Z = bascule (Rz). Valeurs brutes -1..1,
	// converties en puissance dans PlayerTick.
	ThrustAction = NewObject<UInputAction>(this, TEXT("IA_Reacteurs"));
	ThrustAction->ValueType = EInputActionValueType::Axis3D;
	PilotContext->MapKey(ThrustAction, MechHotas::ThrottleMain());
	AddSwizzle(this, PilotContext->MapKey(ThrustAction, MechHotas::ThrottleDial()), EInputAxisSwizzle::YXZ);
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(ThrustAction, MechHotas::ThrottleRocker());
		AddDeadZone(this, Mapping, 0.1f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::ZYX);
	}

	// Verrouillage du torse : auriculaire (bascule) et annulaire (alignement sur les jambes). Secours clavier F / G.
	TorsoLockAction = NewObject<UInputAction>(this, TEXT("IA_VerrouTorse"));
	TorsoLockAction->ValueType = EInputActionValueType::Boolean;
	PilotContext->MapKey(TorsoLockAction, MechHotas::ThrottlePinky());
	PilotContext->MapKey(TorsoLockAction, EKeys::F);
	TorsoAlignAction = NewObject<UInputAction>(this, TEXT("IA_AlignerTorse"));
	TorsoAlignAction->ValueType = EInputActionValueType::Boolean;
	PilotContext->MapKey(TorsoAlignAction, MechHotas::ThrottleRing());
	PilotContext->MapKey(TorsoAlignAction, EKeys::G);

	PilotContext->MapKey(TerminalAction, EKeys::SpaceBar);

	// Debug uniquement (absent du jeu final) : V bascule en vue troisième personne.
	DebugViewAction = NewObject<UInputAction>(this, TEXT("IA_VueDebug"));
	DebugViewAction->ValueType = EInputActionValueType::Boolean;
	PilotContext->MapKey(DebugViewAction, EKeys::V);
}

void AMechPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInputObjects();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(TerminalAction, ETriggerEvent::Started, this, &AMechPlayerController::OnTerminalKey);
		Input->BindAction(TorsoLockAction, ETriggerEvent::Started, this, &AMechPlayerController::OnTorsoLock);
		Input->BindAction(TorsoAlignAction, ETriggerEvent::Started, this, &AMechPlayerController::OnTorsoAlign);
		Input->BindAction(DebugViewAction, ETriggerEvent::Started, this, &AMechPlayerController::OnDebugView);
	}
}

void AMechPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	CreateInputObjects();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(PilotContext, 0);
	}

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	TerminalInput = CreateWidget<UMechTerminalInputWidget>(this, UMechTerminalInputWidget::StaticClass());
	if (TerminalInput)
	{
		TerminalInput->OnLineChanged.AddUObject(this, &AMechPlayerController::HandleLineChanged);
		TerminalInput->OnLineCommitted.AddUObject(this, &AMechPlayerController::HandleLineCommitted);
	}

	Print(TEXT("MECHBATTLE OS v0.1 - console de configuration"));
	Print(TEXT("Tapez 'help' pour la liste des commandes."));

	// Mode test automatisé : commandes terminal passées en ligne de commande, ex. -MechTestCommands="vue;debug".
	FString TestCommands;
	if (FParse::Value(FCommandLine::Get(), TEXT("MechTestCommands="), TestCommands))
	{
		TArray<FString> Commands;
		TestCommands.ParseIntoArray(Commands, TEXT(";"));
		GetWorldTimerManager().SetTimerForNextTick([this, Commands]()
		{
			for (const FString& Command : Commands)
			{
				Print(TEXT("> ") + Command);
				ExecuteCommand(Command);
			}
		});
	}
}

void AMechPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (AMech* Mech = GetMech())
	{
		FVector2D Move = FVector2D::ZeroVector;
		FVector Joystick = FVector::ZeroVector;
		FVector Thrust = FVector::ZeroVector;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (const UEnhancedPlayerInput* EnhancedInput = Subsystem->GetPlayerInput())
			{
				Move = EnhancedInput->GetActionValue(MoveAction).Get<FVector2D>();
				Joystick = EnhancedInput->GetActionValue(JoystickAction).Get<FVector>();
				Thrust = EnhancedInput->GetActionValue(ThrustAction).Get<FVector>();
			}
		}
		// Mode test automatisé (développement) : entrées simulées passées en ligne de commande,
		// ex. -MechTestInput=0,1,0 -MechTestDelay=3 -MechTestDuration=6 (déplacement X, Y, rotation du buste).
		FString TestInput;
		if (FParse::Value(FCommandLine::Get(), TEXT("MechTestInput="), TestInput, /*bShouldStopOnSeparator*/ false))
		{
			float Delay = 3.f;
			float Duration = 6.f;
			FParse::Value(FCommandLine::Get(), TEXT("MechTestDelay="), Delay);
			FParse::Value(FCommandLine::Get(), TEXT("MechTestDuration="), Duration);
			const float Time = GetWorld()->GetTimeSeconds();
			bTestThrust = true;
			ThrustForward = ThrustVertical = ThrustLateral = 0.f;
			if (Time >= Delay && Time < Delay + Duration)
			{
				TArray<FString> Values;
				TestInput.ParseIntoArray(Values, TEXT(","));
				Move.X = Values.IsValidIndex(0) ? FCString::Atof(*Values[0]) : 0.f;
				Move.Y = Values.IsValidIndex(1) ? FCString::Atof(*Values[1]) : 0.f;
				Joystick.X = Values.IsValidIndex(2) ? FCString::Atof(*Values[2]) : 0.f;
				ThrustForward = Values.IsValidIndex(3) ? FCString::Atof(*Values[3]) : 0.f;
				ThrustVertical = Values.IsValidIndex(4) ? FCString::Atof(*Values[4]) : 0.f;
				ThrustLateral = Values.IsValidIndex(5) ? FCString::Atof(*Values[5]) : 0.f;
			}
		}
		Mech->SetPilotInputs(Move, Joystick);

		// Réacteurs : gaz poussés vers l'avant = puissance (le Z brut vaut +1 tiré vers le pilote) ;
		// molette : la valeur brute croît avec la puissance.
		// Tant qu'un axe n'a rien envoyé (pas de HOTAS), sa puissance reste à 0.
		// Sécurité : un réacteur ne s'arme qu'une fois son axe ramené près de 0 (comme une coupure des gaz),
		// pour ne pas décoller au lancement si une molette est restée en haut.
		if (!bTestThrust)
		{
			auto Received = [this](const FKey& Key) { return RawAxisValues.Contains(Key.GetFName()); };
			const float RawForward = Received(MechHotas::ThrottleMain()) ? (1.f - Thrust.X) * 0.5f : 0.f;
			const float RawVertical = Received(MechHotas::ThrottleDial()) ? (1.f + Thrust.Y) * 0.5f : 0.f;
			bForwardThrustArmed |= RawForward < 0.05f;
			bVerticalThrustArmed |= RawVertical < 0.05f;
			ThrustForward = bForwardThrustArmed ? RawForward : 0.f;
			ThrustVertical = bVerticalThrustArmed ? RawVertical : 0.f;
			ThrustLateral = Thrust.Z;
			if (Mech->bDebugDraw && GEngine && (!bForwardThrustArmed || !bVerticalThrustArmed))
			{
				GEngine->AddOnScreenDebugMessage(9005, 0.f, FColor::Orange, FString::Printf(TEXT("Reacteurs desarmes (ramenez a 0 pour armer) : avant %s, vertical %s"),
					bForwardThrustArmed ? TEXT("arme") : TEXT("DESARME"), bVerticalThrustArmed ? TEXT("arme") : TEXT("DESARME")));
			}
		}
		Mech->SetThrusterInputs(ThrustForward, ThrustVertical, ThrustLateral);

		// Diagnostic HOTAS (commande "debug") : événements reçus et valeurs brutes des axes utilisés.
		HotasRateTimer += DeltaTime;
		if (HotasRateTimer >= 1.f)
		{
			HotasEventsPerSecond = HotasEventCount / HotasRateTimer;
			HotasEventCount = 0;
			HotasRateTimer = 0.f;
		}
		if (Mech->bDebugDraw && GEngine)
		{
			auto Raw = [this](const FKey& Key) { const float* V = RawAxisValues.Find(Key.GetFName()); return V ? *V : 0.f; };
			GEngine->AddOnScreenDebugMessage(9003, 0.f, FColor::Yellow, FString::Printf(
				TEXT("HOTAS %.0f evt/s | Joy X %.2f Y %.2f Torsion %.2f | TWCS mini X %.2f Y %.2f"),
				HotasEventsPerSecond, Raw(MechHotas::JoystickX()), Raw(MechHotas::JoystickY()), Raw(MechHotas::JoystickTwist()),
				Raw(MechHotas::ThrottleMiniX()), Raw(MechHotas::ThrottleMiniY())));
		}
	}

	RefreshTerminalScreen(DeltaTime);
}

bool AMechPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Diagnostic : dernier bouton du HOTAS pressé (vérification du mapping).
	if (Params.Event == IE_Pressed && Params.Key.GetFName().ToString().StartsWith(TEXT("GenericUSBController_Button")))
	{
		LastButton = Params.Key.GetFName();
	}

	// Mémorise la dernière valeur reçue de chaque axe du HOTAS (diagnostic "input" du terminal).
	if (Params.Key.IsAxis1D())
	{
		RawAxisValues.Add(Params.Key.GetFName(), Params.AmountDepressed);
		if (Params.Key.GetFName().ToString().StartsWith(TEXT("MechHotas_")))
		{
			++HotasEventCount;
		}
	}
	return Super::InputKey(Params);
}

AMech* AMechPlayerController::GetMech() const
{
	return Cast<AMech>(GetPawn());
}

void AMechPlayerController::OnTorsoLock(const FInputActionValue& Value)
{
	if (AMech* Mech = GetMech())
	{
		Mech->ToggleTorsoLock();
	}
}

void AMechPlayerController::OnDebugView(const FInputActionValue& Value)
{
	if (AMech* Mech = GetMech())
	{
		Mech->SetExternalView(!Mech->IsExternalView());
	}
}

void AMechPlayerController::OnTorsoAlign(const FInputActionValue& Value)
{
	if (AMech* Mech = GetMech())
	{
		Mech->AlignTorsoToLegs();
	}
}

void AMechPlayerController::OnTerminalKey(const FInputActionValue& Value)
{
	OpenTerminal();
}

void AMechPlayerController::OpenTerminal()
{
	if (bTerminalOpen || !TerminalInput)
	{
		return;
	}

	bTerminalOpen = true;
	CurrentLine.Reset();
	TerminalInput->AddToViewport(100);
	TerminalInput->ClearInput();

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TerminalInput->GetEditableText()->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	InputMode.SetHideCursorDuringCapture(true);
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
}

void AMechPlayerController::CloseTerminal()
{
	if (!bTerminalOpen)
	{
		return;
	}

	bTerminalOpen = false;
	CurrentLine.Reset();
	if (TerminalInput)
	{
		TerminalInput->RemoveFromParent();
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AMechPlayerController::HandleLineChanged(const FString& Line)
{
	CurrentLine = Line;
}

void AMechPlayerController::HandleLineCommitted(const FString& Line)
{
	CurrentLine.Reset();
	Print(TEXT("> ") + Line);
	ExecuteCommand(Line);
}

void AMechPlayerController::Print(const FString& Line)
{
	TerminalLines.Add(Line);
	if (TerminalLines.Num() > MechHotas::MaxStoredLines)
	{
		TerminalLines.RemoveAt(0, TerminalLines.Num() - MechHotas::MaxStoredLines);
	}
}

void AMechPlayerController::ExecuteCommand(const FString& Line)
{
	TArray<FString> Parts;
	Line.TrimStartAndEnd().ParseIntoArrayWS(Parts);
	if (Parts.Num() == 0)
	{
		return;
	}

	const FString Command = Parts[0].ToLower();
	AMech* Mech = GetMech();

	if (Command == TEXT("help") || Command == TEXT("aide"))
	{
		Print(TEXT("  exit            fermer la console"));
		Print(TEXT("  quit            quitter le jeu"));
		Print(TEXT("  status          etat du mecha"));
		Print(TEXT("  routines        liste des routines"));
		Print(TEXT("  on|off <nom>    activer / desactiver une routine"));
		Print(TEXT("  reset           remettre le mecha debout"));
		Print(TEXT("  debug           afficher l'equilibre (CdG, appui) et l'animation"));
		Print(TEXT("  input           valeurs brutes des axes HOTAS"));
		Print(TEXT("  vue             basculer vue cockpit / vue externe"));
		Print(TEXT("  baton           afficher le bonhomme baton anime (guide)"));
		Print(TEXT("  jambes [force amort.]  suivi physique des jambes (force 0 = animation pure)"));
		Print(TEXT("  verrou | aligner  verrouiller / aligner le torse (auriculaire / annulaire)"));
		Print(TEXT("  impact <kN> [avant|arriere|gauche|droite]  test d'impact"));
		Print(TEXT("  gyro [stab inertie_kN | detruit | repare]  gyroscope"));
		Print(TEXT("  clear           effacer l'ecran"));
	}
	else if (Command == TEXT("exit"))
	{
		CloseTerminal();
	}
	else if (Command == TEXT("quit"))
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
	else if (Command == TEXT("clear"))
	{
		TerminalLines.Reset();
	}
	else if (!Mech)
	{
		Print(TEXT("Aucun mecha connecte."));
	}
	else if (Command == TEXT("status"))
	{
		Print(Mech->GetStatusText());
	}
	else if (Command == TEXT("routines"))
	{
		if (Mech->GetRoutines().IsEmpty())
		{
			Print(TEXT("  Aucune routine chargee (deplacement, tete et buste sont natifs)."));
		}
		for (const UMechRoutine* Routine : Mech->GetRoutines())
		{
			Print(FString::Printf(TEXT("  %-12s %-4s prio %d"), *Routine->RoutineName, Routine->IsActive() ? TEXT("ON") : TEXT("OFF"), Routine->Priority));
		}
	}
	else if ((Command == TEXT("on") || Command == TEXT("off")) && Parts.Num() >= 2)
	{
		UMechRoutine* Routine = Mech->FindRoutine(Parts[1]);
		if (!Routine)
		{
			Print(FString::Printf(TEXT("Routine inconnue : %s"), *Parts[1]));
		}
		else
		{
			Command == TEXT("on") ? Routine->Activate() : Routine->Deactivate();
			Print(FString::Printf(TEXT("%s -> %s"), *Routine->RoutineName, Routine->IsActive() ? TEXT("ON") : TEXT("OFF")));
		}
	}
	else if (Command == TEXT("reset"))
	{
		Mech->ResetStance();
		Print(TEXT("Mecha remis debout."));
	}
	else if (Command == TEXT("debug"))
	{
		Mech->bDebugDraw = !Mech->bDebugDraw;
		Print(Mech->bDebugDraw ? TEXT("Debug : ON (vert CdG, jaune appui)") : TEXT("Debug equilibre : OFF"));
	}
	else if (Command == TEXT("impact"))
	{
		// Test d'impact : impact <kN> [avant|arriere|gauche|droite] = sens dans lequel le mecha est poussé.
		const float ForceKN = Parts.Num() >= 2 ? FCString::Atof(*Parts[1]) : 5000.f;
		const FString Where = Parts.Num() >= 3 ? Parts[2].ToLower() : TEXT("arriere");
		FVector Local(-1.0, 0.0, 0.0);
		if (Where == TEXT("avant")) { Local = FVector(1.0, 0.0, 0.0); }
		else if (Where == TEXT("gauche")) { Local = FVector(0.0, -1.0, 0.0); }
		else if (Where == TEXT("droite")) { Local = FVector(0.0, 1.0, 0.0); }
		const FVector2D Direction(FRotator(0.f, Mech->GetTorsoYaw(), 0.f).RotateVector(Local));
		const float Knockback = Mech->ApplyExternalForce(Direction * (ForceKN * 1000.f), 0.1f);
		Print(Knockback > 0.f
			? FString::Printf(TEXT("Impact %.0f kN (%s) : recul %.1f km/h"), ForceKN, *Where, Knockback * 0.036f)
			: FString::Printf(TEXT("Impact %.0f kN (%s) : absorbe par le gyroscope"), ForceKN, *Where));
	}
	else if (Command == TEXT("gyro"))
	{
		// gyro [stabilite inertie_kN] | gyro detruit | gyro repare
		if (Parts.Num() >= 3)
		{
			Mech->Gyroscope.Stability = FMath::Max(1.f, FCString::Atof(*Parts[1]));
			Mech->Gyroscope.InertiaKN = FMath::Max(0.f, FCString::Atof(*Parts[2]));
		}
		else if (Parts.Num() == 2)
		{
			Mech->Gyroscope.bOperational = Parts[1].ToLower() != TEXT("detruit");
		}
		const FMechGyroscope& Gyro = Mech->Gyroscope;
		Print(FString::Printf(TEXT("Gyroscope %s | stabilite %.1f | inertie %.0f kN | %.0f kW | chaleur %.0f kW"),
			Gyro.bOperational ? TEXT("OK") : TEXT("DETRUIT"), Gyro.EffectiveStability(), Gyro.EffectiveInertiaN() / 1000.f, Gyro.PowerDrawKW, Gyro.HeatOutputKW));
	}
	else if (Command == TEXT("verrou"))
	{
		Mech->ToggleTorsoLock();
		Print(Mech->IsTorsoLocked() ? TEXT("Torse verrouille sur les jambes") : TEXT("Torse libre"));
	}
	else if (Command == TEXT("aligner"))
	{
		Mech->AlignTorsoToLegs();
		Print(TEXT("Alignement du torse sur les jambes"));
	}
	else if (Command == TEXT("vue"))
	{
		Mech->SetExternalView(!Mech->IsExternalView());
		Print(Mech->IsExternalView() ? TEXT("Vue externe (developpement)") : TEXT("Vue cockpit"));
	}
	else if (Command == TEXT("baton"))
	{
		Mech->SetGuideVisible(!Mech->IsGuideVisible());
		Print(Mech->IsGuideVisible() ? TEXT("Bonhomme baton visible (animation cible)") : TEXT("Bonhomme baton masque"));
	}
	else if (Command == TEXT("jambes"))
	{
		if (Parts.Num() >= 2)
		{
			const float Strength = FMath::Max(0.f, FCString::Atof(*Parts[1]));
			const float Damping = Parts.Num() >= 3 ? FMath::Max(0.f, FCString::Atof(*Parts[2])) : 2.f * FMath::Sqrt(Strength); // amortissement critique
			Mech->SetLegDrive(Strength, Damping);
		}
		Print(FString::Printf(TEXT("Jambes %s | force %.0f | amortissement %.0f | ecart %.0f cm"), Mech->bSimulateLegs ? TEXT("simulees") : TEXT("animees"),
			Mech->LegOrientationStrength, Mech->LegAngularVelocityStrength, Mech->GetLegDeviation()));
	}
	else if (Command == TEXT("input"))
	{
		FString Row;
		for (int32 i = 1; i <= MechHotas::NumRawAxes; ++i)
		{
			const float* Received = RawAxisValues.Find(FName(*FString::Printf(TEXT("MechHotas_Axis%d"), i)));
			const float Value = Received ? *Received : 0.f;
			Row += FString::Printf(TEXT("A%-2d %+.2f   "), i, Value);
			if (i % 4 == 0)
			{
				Print(Row);
				Row.Reset();
			}
		}
		Print(TEXT("T.16000M = A1-A8, TWCS = A9-A16"));
	}
	else
	{
		Print(FString::Printf(TEXT("Commande inconnue : %s (tapez help)"), *Command));
	}
}

void AMechPlayerController::RefreshTerminalScreen(float DeltaTime)
{
	AMech* Mech = GetMech();
	UMechTerminalScreenWidget* Screen = Mech ? Mech->GetTerminalScreen() : nullptr;
	if (!Screen)
	{
		return;
	}

	BlinkTime += DeltaTime;
	const bool bCursorOn = FMath::Fmod(BlinkTime, 1.f) < 0.5f;

	FString Text = Mech->GetStatusText() + TEXT("\n----------------------------------------------------------------\n");
	if (bTerminalOpen)
	{
		const int32 First = FMath::Max(0, TerminalLines.Num() - MechHotas::VisibleLines);
		for (int32 i = First; i < TerminalLines.Num(); ++i)
		{
			Text += TerminalLines[i] + TEXT("\n");
		}
		Text += TEXT("> ") + CurrentLine + (bCursorOn ? TEXT("_") : TEXT(" "));
	}
	else
	{
		Text += TEXT("\n\n   Console en veille.\n\n   [ESPACE] pour ouvrir la console.");
	}

	if (Text != LastScreenText)
	{
		LastScreenText = Text;
		Screen->SetScreenText(Text);
	}
}
