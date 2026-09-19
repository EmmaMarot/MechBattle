#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "MechBattle"

/**
 * Module principal.
 * Déclare les axes du HOTAS comme de vrais axes analogiques : les touches GenericUSBController_Axis* du plugin
 * RawInput sont déclarées comme de simples boutons, et le moteur jette alors leurs valeurs analogiques.
 * RawInput est configuré (DefaultInput.ini) pour émettre ces touches à la place.
 */
class FMechBattleModule : public FDefaultGameModuleImpl
{
public:
	static constexpr int32 NumHotasAxes = 16;

	virtual void StartupModule() override
	{
		EKeys::AddMenuCategoryDisplayInfo(TEXT("MechHotas"), LOCTEXT("MechHotasCategory", "HOTAS Mech"), TEXT("GraphEditor.PadEvent_16x"));
		for (int32 i = 1; i <= NumHotasAxes; ++i)
		{
			const FName Name(*FString::Printf(TEXT("MechHotas_Axis%d"), i));
			if (!EKeys::GetKeyDetails(FKey(Name)).IsValid())
			{
				EKeys::AddKey(FKeyDetails(FKey(Name), FText::Format(LOCTEXT("MechHotasAxis", "HOTAS Axe {0}"), i),
					FKeyDetails::GamepadKey | FKeyDetails::Axis1D, TEXT("MechHotas")));
			}
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMechBattleModule, MechBattle, "MechBattle");

#undef LOCTEXT_NAMESPACE
