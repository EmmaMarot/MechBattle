#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MechTerminalWidgets.generated.h"

class UEditableText;
class UTextBlock;

/** Contenu de l'écran du terminal, affiché dans le cockpit via un WidgetComponent. */
UCLASS()
class MECHBATTLE_API UMechTerminalScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetScreenText(const FString& Text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> ScreenText;

	FString PendingText;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMechTerminalText, const FString&);

/**
 * Saisie clavier du terminal : un champ texte UMG invisible qui garde le focus clavier.
 * Le texte tapé est affiché sur l'écran du cockpit, pas sur l'écran principal.
 */
UCLASS()
class MECHBATTLE_API UMechTerminalInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnMechTerminalText OnLineChanged;
	FOnMechTerminalText OnLineCommitted;

	UEditableText* GetEditableText() const { return Edit; }
	void ClearInput();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY()
	TObjectPtr<UEditableText> Edit;
};
