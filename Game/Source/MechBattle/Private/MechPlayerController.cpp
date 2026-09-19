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

namespace MechHotas
{
	// Plugin RawInput (voir DefaultInput.ini) : T.16000M -> GenericUSBController_Axis1..8,
	// TWCS -> GenericUSBController_Axis9..16, dans l'ordre des axes HID du périphérique.
	// L'ordre est supposé : à vérifier avec la commande "input" du terminal ou "showdebug rawinput".
	FKey JoystickX()     { return FKey(TEXT("GenericUSBController_Axis2")); }
	FKey JoystickY()     { return FKey(TEXT("GenericUSBController_Axis1")); }
	FKey JoystickTwist() { return FKey(TEXT("GenericUSBController_Axis3")); }
	FKey ThrottleMiniX() { return FKey(TEXT("GenericUSBController_Axis10")); }
	FKey ThrottleMiniY() { return FKey(TEXT("GenericUSBController_Axis9")); }

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

	GyroAction = NewObject<UInputAction>(this, TEXT("IA_Gyroscope"));
	GyroAction->ValueType = EInputActionValueType::Axis2D;

	HeadAction = NewObject<UInputAction>(this, TEXT("IA_Tete"));
	HeadAction->ValueType = EInputActionValueType::Axis3D;

	TerminalAction = NewObject<UInputAction>(this, TEXT("IA_Terminal"));
	TerminalAction->ValueType = EInputActionValueType::Boolean;

	PilotContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Pilote"));

	// Gyroscope principal : mini-stick du TWCS (X = droite, Y = avant).
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(GyroAction, MechHotas::ThrottleMiniX());
		AddDeadZone(this, Mapping, 0.08f);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(GyroAction, MechHotas::ThrottleMiniY());
		AddDeadZone(this, Mapping, 0.08f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
		AddNegate(this, Mapping, false, true, false);
	}

	// Secours clavier pour le gyroscope (flèches), utile sans HOTAS.
	PilotContext->MapKey(GyroAction, EKeys::Right);
	AddNegate(this, PilotContext->MapKey(GyroAction, EKeys::Left), true, false, false);
	AddSwizzle(this, PilotContext->MapKey(GyroAction, EKeys::Up), EInputAxisSwizzle::YXZ);
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(GyroAction, EKeys::Down);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
		AddNegate(this, Mapping, false, true, false);
	}

	// Tête : joystick en position absolue (X = inclinaison, Y = avant, Z = torsion).
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(HeadAction, MechHotas::JoystickX());
		AddDeadZone(this, Mapping, 0.02f);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(HeadAction, MechHotas::JoystickY());
		AddDeadZone(this, Mapping, 0.02f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::YXZ);
		AddNegate(this, Mapping, false, true, false);
	}
	{
		FEnhancedActionKeyMapping& Mapping = PilotContext->MapKey(HeadAction, MechHotas::JoystickTwist());
		AddDeadZone(this, Mapping, 0.04f);
		AddSwizzle(this, Mapping, EInputAxisSwizzle::ZYX);
	}

	PilotContext->MapKey(TerminalAction, EKeys::SpaceBar);
}

void AMechPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInputObjects();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(TerminalAction, ETriggerEvent::Started, this, &AMechPlayerController::OnTerminalKey);
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
}

void AMechPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (AMech* Mech = GetMech())
	{
		FVector2D Gyro = FVector2D::ZeroVector;
		FVector Head = FVector::ZeroVector;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (const UEnhancedPlayerInput* EnhancedInput = Subsystem->GetPlayerInput())
			{
				Gyro = EnhancedInput->GetActionValue(GyroAction).Get<FVector2D>();
				Head = EnhancedInput->GetActionValue(HeadAction).Get<FVector>();
			}
		}
		Mech->SetPilotInputs(Gyro, Head);
	}

	RefreshTerminalScreen(DeltaTime);
}

AMech* AMechPlayerController::GetMech() const
{
	return Cast<AMech>(GetPawn());
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
		Print(TEXT("  debug           afficher l'equilibre (CdG, appui, capture)"));
		Print(TEXT("  input           valeurs brutes des axes HOTAS"));
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
		Print(Mech->bDebugDraw ? TEXT("Debug equilibre : ON (vert CdG, bleu ZMP, rouge capture)") : TEXT("Debug equilibre : OFF"));
	}
	else if (Command == TEXT("input"))
	{
		FString Row;
		for (int32 i = 1; i <= MechHotas::NumRawAxes; ++i)
		{
			const float Value = GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("GenericUSBController_Axis%d"), i)));
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
