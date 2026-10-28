#include "GoNGymGameMode.h"

#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GoNBlockoutLog.h"
#include "GoNCharacter.h"
#include "GoNDeveloperSettings.h"
#include "GoNGymBuilder.h"
#include "GoNMarkers.h"
#include "GoNNarrativeSubsystem.h"
#include "GoNPlayerController.h"
#include "GoNWoundedComrade.h"

AGoNGymGameMode::AGoNGymGameMode()
{
	DefaultPawnClass = AGoNCharacter::StaticClass();
	PlayerControllerClass = AGoNPlayerController::StaticClass();
}

void AGoNGymGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (UClass* PawnClass = UGoNDeveloperSettings::Get()->GymPawnClass.LoadSynchronous())
	{
		DefaultPawnClass = PawnClass;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	AGoNGymBuilder* Builder = nullptr;
	for (TActorIterator<AGoNGymBuilder> It(World); It; ++It)
	{
		Builder = *It;
		break;
	}
	if (Builder == nullptr)
	{
		Builder = World->SpawnActor<AGoNGymBuilder>(AGoNGymBuilder::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: spawned the test gym on %s"), *MapName);
	}

	if (!TActorIterator<ADirectionalLight>(World))
	{
		const FTransform SunTransform(FRotator(-50.0f, 35.0f, 0.0f), FVector(0.0f, 0.0f, 1000.0f));
		if (ADirectionalLight* Sun = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				SunLight->SetMobility(EComponentMobility::Movable);
				SunLight->SetAtmosphereSunLight(true);
			}
			Sun->FinishSpawning(SunTransform);
		}
		if (ASkyLight* Sky = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			if (USkyLightComponent* SkyLight = Sky->GetLightComponent())
			{
				SkyLight->SetMobility(EComponentMobility::Movable);
				SkyLight->bRealTimeCapture = true;
			}
			Sky->FinishSpawning(FTransform::Identity);
		}
		World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params);
	}

	if (Builder == nullptr)
	{
		return;
	}
	const FTransform Start = Builder->GetPlayerStartTransform();
	if (!TActorIterator<APlayerStart>(World))
	{
		World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Start, Params);
	}

	// A wounded comrade lying 3 m ahead of the start (gym floor top is the builder's Z).
	if (!TActorIterator<AGoNWoundedComrade>(World))
	{
		const FVector Ahead = Start.GetLocation() + Start.GetRotation().GetForwardVector() * 300.0;
		const FVector OnFloor(Ahead.X, Ahead.Y, Builder->GetActorLocation().Z + 60.0);
		World->SpawnActor<AGoNWoundedComrade>(AGoNWoundedComrade::StaticClass(), FTransform(FRotator(0.0, 90.0, 0.0), OnFloor), Params);
	}

	// Narrative trigger around the first low cover: completes reach_cover in gym_test_mission.
	if (!TActorIterator<AGoNNarrativeTrigger>(World))
	{
		const FBox Cover = Builder->GetLowCoverBounds();
		if (Cover.IsValid)
		{
			if (AGoNNarrativeTrigger* Trigger = World->SpawnActor<AGoNNarrativeTrigger>(AGoNNarrativeTrigger::StaticClass(), FTransform(Cover.GetCenter()), Params))
			{
				Trigger->SetEventName(TEXT("gym.reached_cover"));
				Trigger->TriggerBox->SetBoxExtent(Cover.GetExtent() + FVector(120.0, 120.0, 60.0));
			}
		}
	}
}

void AGoNGymGameMode::StartPlay()
{
	Super::StartPlay();
	const FName Mission = UGoNDeveloperSettings::Get()->GymAutoStartMission;
	if (Mission.IsNone())
	{
		return;
	}
	if (UGoNNarrativeSubsystem* Narrative = UGoNNarrativeSubsystem::Get(this))
	{
		Narrative->StartMission(Mission);
	}
}
