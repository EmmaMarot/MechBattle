#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MechPlayerController.generated.h"

class AMech;
class UInputAction;
class UInputMappingContext;
class UMechTerminalInputWidget;
struct FInputActionValue;

/**
 * Contrôleur du pilote.
 * - HOTAS lu via le plugin RawInput (touches GenericUSBController_*, configurées dans DefaultInput.ini)
 *   puis Enhanced Input.
 * - Terminal de configuration : [Espace] l'ouvre, "exit" le ferme, "quit" quitte le jeu.
 */
UCLASS()
class MECHBATTLE_API AMechPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void CreateInputObjects();
	void OnTerminalKey(const FInputActionValue& Value);
	void OnTorsoLock(const FInputActionValue& Value);
	void OnTorsoAlign(const FInputActionValue& Value);
	void OnDebugView(const FInputActionValue& Value);

	void OpenTerminal();
	void CloseTerminal();
	void HandleLineChanged(const FString& Line);
	void HandleLineCommitted(const FString& Line);
	void ExecuteCommand(const FString& Line);
	void Print(const FString& Line);
	void RefreshTerminalScreen(float DeltaTime);

	AMech* GetMech() const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> PilotContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> JoystickAction;

	UPROPERTY()
	TObjectPtr<UInputAction> TerminalAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ThrustAction;

	UPROPERTY()
	TObjectPtr<UInputAction> TorsoLockAction;

	UPROPERTY()
	TObjectPtr<UInputAction> TorsoAlignAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DebugViewAction;

	UPROPERTY()
	TObjectPtr<UMechTerminalInputWidget> TerminalInput;

	/** Dernière valeur reçue par axe analogique (diagnostic). */
	TMap<FName, float> RawAxisValues;
	FName LastButton;
	float ThrustForward = 0.f;
	float ThrustVertical = 0.f;
	float ThrustLateral = 0.f;
	bool bTestThrust = false;
	bool bForwardThrustArmed = false;
	bool bVerticalThrustArmed = false;
	int32 HotasEventCount = 0;
	float HotasRateTimer = 0.f;
	float HotasEventsPerSecond = 0.f;

	bool bTerminalOpen = false;
	TArray<FString> TerminalLines;
	FString CurrentLine;
	FString LastScreenText;
	float BlinkTime = 0.f;
};
