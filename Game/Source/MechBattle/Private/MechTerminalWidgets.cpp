#include "MechTerminalWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

TSharedRef<SWidget> UMechTerminalScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Fond"));
		Background->SetBrushColor(FLinearColor(0.004f, 0.02f, 0.008f, 1.f));
		Background->SetPadding(FMargin(18.f, 14.f));

		ScreenText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Texte"));
		if (const UObject* MonoFont = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono")))
		{
			ScreenText->SetFont(FSlateFontInfo(MonoFont, 17));
		}
		ScreenText->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 1.f, 0.4f)));
		ScreenText->SetAutoWrapText(true);
		ScreenText->SetText(FText::FromString(PendingText));

		Background->SetContent(ScreenText);
		WidgetTree->RootWidget = Background;
	}
	return Super::RebuildWidget();
}

void UMechTerminalScreenWidget::SetScreenText(const FString& Text)
{
	PendingText = Text;
	if (ScreenText)
	{
		ScreenText->SetText(FText::FromString(Text));
	}
}

TSharedRef<SWidget> UMechTerminalInputWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Edit = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("Saisie"));
		Edit->SetClearKeyboardFocusOnCommit(false);
		Edit->SetRevertTextOnEscape(false);
		Edit->SetRenderOpacity(0.f);
		Edit->OnTextChanged.AddDynamic(this, &UMechTerminalInputWidget::HandleTextChanged);
		Edit->OnTextCommitted.AddDynamic(this, &UMechTerminalInputWidget::HandleTextCommitted);
		WidgetTree->RootWidget = Edit;
	}
	return Super::RebuildWidget();
}

void UMechTerminalInputWidget::ClearInput()
{
	if (Edit)
	{
		Edit->SetText(FText::GetEmpty());
	}
}

void UMechTerminalInputWidget::HandleTextChanged(const FText& Text)
{
	OnLineChanged.Broadcast(Text.ToString());
}

void UMechTerminalInputWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		const FString Line = Text.ToString();
		ClearInput();
		OnLineCommitted.Broadcast(Line);
	}
}
