#include "RSConsoleCommands.h"

#include "RSCheatManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
namespace
{
	using FRSConsoleAction = TFunction<void(URSCheatManager&, const TArray<FString>&)>;

	// 핫 리로드 중 엔진이 콘솔 객체를 교체할 수 있으므로, 해제할 때 이전 객체의 주소를 사용하지 않도록 이름을 보관합니다
	TArray<FString> RegisteredCommandNames;

	URSCheatManager* FindCheatManager(UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			World = nullptr;
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.World() && Context.World()->IsGameWorld())
					{
						if (World)
						{
							UE_LOG(LogTemp, Warning, TEXT("Multiple game worlds; execute this command in the intended PIE game console"));
							return nullptr;
						}
						World = Context.World();
					}
				}
			}
		}
		if (World)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Controller = It->Get();
				if (Controller && Controller->IsLocalController() && Controller->HasAuthority())
				{
					if (URSCheatManager* Cheat = Cast<URSCheatManager>(Controller->CheatManager))
					{
						return Cheat;
					}
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("No authoritative local RSCheatManager; start a single-player game with RSPlayerController and cheats enabled"));
		return nullptr;
	}

	void AddCommand(const TCHAR* Name, const TCHAR* Help, int32 ArgumentCount, FRSConsoleAction Action)
	{
		IConsoleManager::Get().RegisterConsoleCommand(Name, Help, FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[Name, Help, ArgumentCount, Action](const TArray<FString>& Arguments, UWorld* World)
			{
				if ((ArgumentCount >= 0 && Arguments.Num() != ArgumentCount) || (ArgumentCount < 0 && Arguments.Num() > 1))
				{
					UE_LOG(LogTemp, Warning, TEXT("%s: %s"), Name, Help);
					return;
				}
				if (URSCheatManager* Cheat = FindCheatManager(World))
				{
					Action(*Cheat, Arguments);
				}
			}), ECVF_Cheat);
		RegisteredCommandNames.Add(Name);
	}

	void AddAmountCommand(const TCHAR* Name, const TCHAR* Help, void (URSCheatManager::*Action)(int32))
	{
		AddCommand(Name, Help, 1, [Action](URSCheatManager& Cheat, const TArray<FString>& Arguments)
		{
			int32 Amount = 0;
			if (!Arguments[0].IsNumeric() || !LexTryParseString(Amount, *Arguments[0]) || Amount <= 0 || FString::FromInt(Amount) != Arguments[0])
			{
				UE_LOG(LogTemp, Warning, TEXT("Expected a positive integer amount (1..2147483647)"));
				return;
			}
			(Cheat.*Action)(Amount);
		});
	}
}
#endif

void RSConsoleCommands::Register()
{
#if !UE_BUILD_SHIPPING
	if (!RegisteredCommandNames.IsEmpty())
	{
		return;
	}
	AddAmountCommand(TEXT("RS.Player.Damage"), TEXT("RS.Player.Damage <positive integer>: applies the damage GameplayEffect."), &URSCheatManager::DamagePlayer);
	AddAmountCommand(TEXT("RS.Player.Heal"), TEXT("RS.Player.Heal <positive integer>: heals a living player."), &URSCheatManager::HealPlayer);
	AddAmountCommand(TEXT("RS.Boss.Damage"), TEXT("RS.Boss.Damage <positive integer>: alias of RS_DamageBoss."), &URSCheatManager::RS_DamageBoss);
	AddCommand(TEXT("RS.Player.Kill"), TEXT("Sets player Health to zero, bypassing damage immunity."), 0, [](URSCheatManager& Cheat, const TArray<FString>&) { Cheat.KillPlayer(); });
	AddCommand(TEXT("RS.Player.Invulnerable"), TEXT("RS.Player.Invulnerable <0|1>: removes/adds this cheat's damage immunity."), 1, [](URSCheatManager& Cheat, const TArray<FString>& Arguments)
	{
		if (Arguments[0] != TEXT("0") && Arguments[0] != TEXT("1"))
		{
			UE_LOG(LogTemp, Warning, TEXT("Expected 0 or 1"));
			return;
		}
		Cheat.SetPlayerInvulnerable(Arguments[0] == TEXT("1"));
	});
	AddCommand(TEXT("RS.Debug.DumpState"), TEXT("Prints Health, tags, active abilities, phase, cycle and encounter state."), 0, [](URSCheatManager& Cheat, const TArray<FString>&) { Cheat.DumpState(); });
	AddCommand(TEXT("RS.Boss.RunPattern"), TEXT("RS.Boss.RunPattern [name]: lists candidates or queues one selection on the next matching turn."), -1, [](URSCheatManager& Cheat, const TArray<FString>& Arguments) { Cheat.RunBossPattern(Arguments.IsEmpty() ? FString() : Arguments[0]); });
	AddCommand(TEXT("RS.Player.ToggleDownedTag"), TEXT("Alias of RS_ToggleDownedTag; do not use during GA_Downed."), 0, [](URSCheatManager& Cheat, const TArray<FString>&) { Cheat.RS_ToggleDownedTag(); });
	AddCommand(TEXT("RS.Boss.CompleteEncounter"), TEXT("Alias of RS_CompleteBossEncounter."), 0, [](URSCheatManager& Cheat, const TArray<FString>&) { Cheat.RS_CompleteBossEncounter(); });
	AddCommand(TEXT("RS.Boss.ExpireTimeLimit"), TEXT("Alias of RS_ExpireBossTimeLimit."), 0, [](URSCheatManager& Cheat, const TArray<FString>&) { Cheat.RS_ExpireBossTimeLimit(); });
#endif
}

void RSConsoleCommands::Unregister()
{
#if !UE_BUILD_SHIPPING
	for (const FString& Name : RegisteredCommandNames)
	{
		IConsoleManager::Get().UnregisterConsoleObject(*Name);
	}
	RegisteredCommandNames.Empty();
#endif
}
