#include "GoNBlockoutLog.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GoNBuildInfo.h"
#include "GoNDeveloperSettings.h"
#include "GoNGymBuilder.h"
#include "GoNStringConv.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Logic/GoNGymLayout.h"
#include "Logic/GoNMovementGymCheck.h"
#include "GoNCharacter.h"
#include "GoNMovementSettings.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNBlockout);

class FGoNBlockoutModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNBlockout"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNBlockout, Log, TEXT("GoN Blockout: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNBlockout"));
	}
};

IMPLEMENT_MODULE(FGoNBlockoutModule, GoNBlockout);

namespace GoNBlockoutCommandsDetail
{
	FAutoConsoleCommandWithWorldAndArgs OpenGymCommand(
		TEXT("gon.gym.open"),
		TEXT("GoN Blockout: open the C++-generated test gym (Gym Map Path + AGoNGymGameMode)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || World->GetGameInstance() == nullptr)
			{
				UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: start Play (PIE) first, then run gon.gym.open"));
				return;
			}
			const FString MapPath = UGoNDeveloperSettings::Get()->GymMapPath;
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: opening gym on %s"), *MapPath);
			UGameplayStatics::OpenLevel(World, FName(*MapPath), true, TEXT("game=/Script/GoNBlockout.GoNGymGameMode"));
		}));

	FAutoConsoleCommandWithWorldAndArgs RebuildGymCommand(
		TEXT("gon.gym.rebuild"),
		TEXT("GoN Blockout: rebuild every gym builder in the current world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				return;
			}
			int32 Count = 0;
			for (TActorIterator<AGoNGymBuilder> It(World); It; ++It)
			{
				It->Rebuild();
				++Count;
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: rebuilt %d gym builder(s)"), Count);
		}));

	FAutoConsoleCommandWithWorldAndArgs CharMetricsCommand(
		TEXT("gon.char.metrics"),
		TEXT("GoN Character: compare the player's movement settings (or the defaults) with the gym metrics: vault/climb heights, steps, slopes, doors, capsule."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			GoN::Character::FMovementMetrics Movement = GetDefault<UGoNMovementSettings>()->Metrics.ToPure();
			FString MovementSource = TEXT("UGoNMovementSettings defaults");
			GoN::Blockout::FGymMetrics Gym;
			FString GymSource = TEXT("default gym metrics");
			if (World != nullptr)
			{
				const APlayerController* PlayerController = World->GetFirstPlayerController();
				if (const AGoNCharacter* Character = PlayerController ? Cast<AGoNCharacter>(PlayerController->GetPawn()) : nullptr)
				{
					Movement = Character->GetMetrics();
					MovementSource = Character->GetName();
				}
				for (TActorIterator<AGoNGymBuilder> It(World); It; ++It)
				{
					Gym = It->GetPureMetrics();
					GymSource = It->GetName();
					break;
				}
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Character: metrics of %s vs %s"), *MovementSource, *GymSource);
			const std::vector<GoN::Blockout::FMetricCheck> Checks = GoN::Blockout::CompareMovementWithGym(Movement, Gym);
			for (const GoN::Blockout::FMetricCheck& Check : Checks)
			{
				const FString Text = GoN::ToFString(Check.Text);
				switch (Check.Result)
				{
				case GoN::Blockout::EMetricResult::Ok: UE_LOG(LogGoNBlockout, Display, TEXT("GoN Character:   ok    %s"), *Text); break;
				case GoN::Blockout::EMetricResult::Note: UE_LOG(LogGoNBlockout, Display, TEXT("GoN Character:   note  %s"), *Text); break;
				case GoN::Blockout::EMetricResult::Fail: UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Character:   FAIL  %s"), *Text); break;
				}
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Character: %d ok, %d notes, %d failed"),
				GoN::Blockout::CountResults(Checks, GoN::Blockout::EMetricResult::Ok),
				GoN::Blockout::CountResults(Checks, GoN::Blockout::EMetricResult::Note),
				GoN::Blockout::CountResults(Checks, GoN::Blockout::EMetricResult::Fail));
		}));

	FAutoConsoleCommand ValidateGymCommand(
		TEXT("gon.gym.validate"),
		TEXT("GoN Blockout: check the default gym layout against its metrics."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const GoN::Blockout::FGymMetrics Metrics;
			const std::vector<std::string> Issues = GoN::Blockout::ValidateGymLayout(GoN::Blockout::BuildGymLayout(Metrics), Metrics);
			for (const std::string& Issue : Issues)
			{
				UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: %s"), *GoN::ToFString(Issue));
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: gym layout has %d issue(s)"), static_cast<int32>(Issues.size()));
		}));
}
